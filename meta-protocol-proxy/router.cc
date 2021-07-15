// Copyright (c) 2020, Tencent Inc.
// All rights reserve

#include "trpc/router.h"

#include <utility>

#include "common/buffer/buffer_impl.h"
#include "common/common/assert.h"
#include "common/upstream/load_balancer_impl.h"
#include "envoy/tcp/conn_pool.h"

#include "trpc/protocol.h"
#include "trpc/trpc.pb.h"

namespace Envoy::Extensions::NetworkFilters::TrpcProxy {

Router::Router(Upstream::ClusterManager& cluster_manager, ActiveMessage& message)
    : cluster_manager_(cluster_manager), message_(message), response_checker_(*this) {}

void Router::onEvent(Network::ConnectionEvent event) {
  ENVOY_LOG(debug, "on event: {}", event);

  if (!upstream_request_ || upstream_request_->response_complete_) {
    // Client closed connection after completing response.
    ENVOY_LOG(debug, "trpc upstream request: the upstream request had completed");
    return;
  }

  switch (event) {
    case Network::ConnectionEvent::RemoteClose:
      message_.stats().conn_pool_remote_close_.inc();
      upstream_request_->onResetStream(
          Tcp::ConnectionPool::PoolFailureReason::RemoteConnectionFailure);
      break;
    case Network::ConnectionEvent::LocalClose:
      message_.stats().conn_pool_local_close_.inc();
      upstream_request_->onResetStream(
          Tcp::ConnectionPool::PoolFailureReason::LocalConnectionFailure);
      break;
    default:
      // Connected is consumed by the connection pool.
      NOT_REACHED_GCOVR_EXCL_LINE;
  }
}

bool Router::shouldSelectAnotherHost(Upstream::Host const& host) {
  ENVOY_LOG(debug, "another host {}", host.address()->asString());
  if (upstream_request_) {
    if (upstream_request_->failed_hosts_.find(host.address()->asString()) !=
        upstream_request_->failed_hosts_.end()) {
      return true;
    }
    upstream_request_->retry_ = true;
  }
  return false;
}

void Router::onUpstreamData(Buffer::Instance& data, bool end_stream) {
  ENVOY_LOG(debug, "on upstream: {} {}", data.length(), end_stream);
  upstream_response_buffer_.move(data);
  end_stream_ = end_stream;
  try {
    // 暂时不需要考虑粘包
    bool underflow = false;
    response_checker_.onData(upstream_response_buffer_, &underflow);
  } catch (EnvoyException const& ex) {
    ENVOY_LOG(error, "recv resp packet decode error: {}", ex.what());
    message_.stats().response_decoding_error_.inc();
    message_.sendLocalReply(
        ErrResponse(trpc::TRPC_SERVER_DECODE_ERR,
                    fmt::format("recv resp packet decode error: {}", ex.what())),
        false);

    upstream_request_->messageFinished(true);
    return;
  }
}

absl::optional<uint64_t> Router::computeHashKey() {
  ENVOY_LOG(debug, "computeHashKey");
  if (auto const* downstream_headers = message_.metadata_->requestHttpHeaders();
      route_entry_ && downstream_headers != nullptr) {
    if (auto* hash_policy = route_entry_->hashPolicy(); hash_policy != nullptr) {
      return hash_policy->generateHash(
          message_.streamInfo().downstreamRemoteAddress().get(), *downstream_headers,
          [](const std::string&, const std::string&, std::chrono::seconds) {
            return std::string();
          },
          message_.streamInfo().filterState());
    }
  }

  return {};
}

Envoy::Router::MetadataMatchCriteria const* Router::metadataMatchCriteria() {
  ENVOY_LOG(debug, "metadataMatchCriteria");
  return nullptr;
}

Network::Connection const* Router::downstreamConnection() const {
  ENVOY_LOG(debug, "downstreamConnection");
  return message_.connection();
}

Http::RequestHeaderMap const* Router::downstreamHeaders() const {
  ENVOY_LOG(debug, "downstreamHeaders");
  return message_.metadata_->requestHttpHeaders();
}

void Router::onMessageDecoded(std::shared_ptr<MessageMetadata> const& metadata,
                              Envoy::Buffer::OwnedImpl& data) {
  downstream_metadata_ = metadata;
  ENVOY_LOG(debug, "message decoded: {}", data.length());
  upstream_request_buffer_.move(data);
  message_.streamInfo().addBytesReceived(upstream_request_buffer_.length());

  ASSERT(!upstream_request_);
  upstream_request_ = std::make_unique<UpstreamRequest>(*this);

  dispatch();
}

void Router::onCompleted(std::unique_ptr<Buffer::OwnedImpl> msg) {
  if (response_protocol_.request_id() != downstream_metadata_->request_id()) {
    ENVOY_LOG(error, "recv resp request_id:{}, expect:{}", response_protocol_.request_id(),
              downstream_metadata_->request_protocol.request_id());
    message_.stats().response_different_request_id_.inc();
    message_.sendLocalReply(
        ErrResponse(trpc::TRPC_SERVER_DECODE_ERR, fmt::format("recv resp request_id:{}, expect:{}",
                                                              response_protocol_.request_id(),
                                                              downstream_metadata_->request_id())),
        false);
    upstream_request_->messageFinished(true);
    return;
  }
  message_.streamInfo().addBytesSent(msg->length());
  message_.connection()->write(*msg, end_stream_);
  message_.stats().response_success_.inc();
  upstream_request_->messageFinished(false);
}

void Router::UpstreamRequest::messageFinished(bool reset) {
  response_complete_ = true;
  if (!reset && conn_data_) {
    // conn_data_ reset后才不会关闭upstream的连接
    conn_data_.reset();
  }
  parent_.message_.onReset();
}

void Router::dispatch() {
  auto service_name = downstream_metadata_->service_name();
  route_ = message_.route();
  if (!route_) {
    ENVOY_LOG(error, "trpc router: no cluster match for service '{}'", service_name);
    message_.streamInfo().setResponseFlag(StreamInfo::ResponseFlag::NoRouteFound);
    message_.stats().dismatch_route_.inc();
    message_.sendLocalReply(
        ErrResponse(trpc::TRPC_SERVER_NOSERVICE_ERR,
                    fmt::format("trpc router: no route for service '{}'", service_name)),
        true);
    return;
  }

  route_entry_ = route_->routeEntry();
  ENVOY_LOG(debug, "trpc router: onMessageDecoded cluster '{}'", route_entry_->clusterName());

  auto& cluster_name = route_entry_->clusterName();
  auto* cluster = cluster_manager_.get(cluster_name);
  if (cluster == nullptr) {
    ENVOY_LOG(error, "trpc router: unknown cluster '{}'", cluster_name);
    message_.streamInfo().setResponseFlag(StreamInfo::ResponseFlag::NoRouteFound);
    message_.stats().unknow_cluster_.inc();
    message_.sendLocalReply(
        ErrResponse(trpc::TRPC_SERVER_NOSERVICE_ERR,
                    fmt::format("trpc router: unknown cluster '{}'", cluster_name)),
        true);
    return;
  }
  message_.streamInfo().setUpstreamClusterInfo(cluster->info());

  Tcp::ConnectionPool::Instance* conn_pool = cluster_manager_.tcpConnPoolForCluster(
      cluster_name, Upstream::ResourcePriority::Default, this);

  if (!conn_pool) {
    ENVOY_LOG(debug, "trpc router: no conn pool for '{}'", cluster_name);
    message_.stats().no_conn_pool_.inc();
    message_.sendLocalReply(
        ErrResponse(trpc::TRPC_SERVER_SYSTEM_ERR,
                    fmt::format("trpc router: no conn pool for '{}'", cluster_name)),
        true);
    return;
  }

  return upstream_request_->start(*conn_pool);
}

void Router::onReset() {
  if (upstream_request_) {
    upstream_request_->onReset();
    upstream_request_.reset();
  }
}

void Router::UpstreamRequest::onReset() {
  if (conn_pool_handle_) {
    ASSERT(!conn_data_);
    conn_pool_handle_->cancel(Tcp::ConnectionPool::CancelPolicy::Default);
    conn_pool_handle_ = nullptr;
    ENVOY_LOG(debug, "upstream request: reset connection pool handler");
  }

  if (conn_data_) {
    conn_data_->connection().close(Network::ConnectionCloseType::NoFlush);
    conn_data_.reset();
  }
}

Router::UpstreamRequest::~UpstreamRequest() {
  ENVOY_LOG(debug, "trpc upstream: destory");

  if (conn_pool_handle_) {
    ASSERT(!conn_data_);
    conn_pool_handle_->cancel(Tcp::ConnectionPool::CancelPolicy::Default);
    conn_pool_handle_ = nullptr;
    ENVOY_LOG(debug, "upstream request: reset connection pool handler");
  }
}

void Router::UpstreamRequest::onPoolReady(Tcp::ConnectionPool::ConnectionDataPtr&& conn,
                                          Upstream::HostDescriptionConstSharedPtr host) {
  ENVOY_LOG(debug, "trpc upstream: onPoolReady buffer len {}",
            parent_.upstream_request_buffer_.length());
  onUpstreamHostSelected(host);
  conn_data_ = std::move(conn);
  parent_.message_.streamInfo().setUpstreamLocalAddress(conn_data_->connection().localAddress());
  conn_data_->addUpstreamCallbacks(parent_);
  conn_pool_handle_ = nullptr;
  // encode request data
  conn_data_->connection().write(parent_.upstream_request_buffer_, false);
  // 单项调用直接结束
  if (parent_.message_.one_way_call_) {
    messageFinished(false);
  }
}

void Router::UpstreamRequest::onPoolFailure(Tcp::ConnectionPool::PoolFailureReason reason,
                                            Upstream::HostDescriptionConstSharedPtr host) {
  ENVOY_LOG(debug, "trpc upstream: onPoolFailure {}", host ? host->address()->asString() : "");
  conn_pool_handle_ = nullptr;
  if (host) {
    failed_hosts_.emplace(host->address()->asString());
    if (retry_) {
      retry_ = false;
      parent_.dispatch();
      return;
    }
  }

  // Mimic an upstream reset.
  onUpstreamHostSelected(host);
  parent_.message_.stats().conn_pool_failure_.inc();
  parent_.upstream_request_buffer_.drain(parent_.upstream_request_buffer_.length());

  if (reason == Tcp::ConnectionPool::PoolFailureReason::Timeout ||
      reason == Tcp::ConnectionPool::PoolFailureReason::LocalConnectionFailure ||
      reason == Tcp::ConnectionPool::PoolFailureReason::RemoteConnectionFailure) {
    ENVOY_LOG(error, "trpc proxying connection failure");
  }
  onResetStream(reason);
}

void Router::UpstreamRequest::onUpstreamHostSelected(
    Upstream::HostDescriptionConstSharedPtr const& host) {
  ENVOY_LOG(debug, "trpc upstream request: selected upstream {}", host->address()->asString());
  parent_.message_.streamInfo().onUpstreamHostSelected(host);
  upstream_host_ = host;
}

void Router::UpstreamRequest::start(Tcp::ConnectionPool::Instance& pool) {
  Tcp::ConnectionPool::Cancellable* handle = pool.newConnection(*this);
  if (handle) {
    // Pause while we wait for a connection.
    conn_pool_handle_ = handle;
  }
}

void Router::UpstreamRequest::onResetStream(Tcp::ConnectionPool::PoolFailureReason reason) {
  if (parent_.message_.one_way_call_) {
    // For OneWay requests, we should not attempt a response. Reset the downstream to signal
    // an error.
    parent_.message_.connection()->close(Network::ConnectionCloseType::NoFlush);
    return;
  }

  // When the filter's callback does not end, the sendLocalReply function call
  // triggers the release of the current stream at the end of the filter's
  // callback.
  auto const host = upstream_host_ ? upstream_host_->address()->asString() : "null";
  switch (reason) {
    case Tcp::ConnectionPool::PoolFailureReason::Overflow:
      parent_.message_.streamInfo().setResponseFlag(StreamInfo::ResponseFlag::UpstreamOverflow);
      parent_.message_.sendLocalReply(
          ErrResponse(trpc::TRPC_SERVER_OVERLOAD_ERR,
                      fmt::format("trpc upstream request: too many connections")),
          false);
      break;
    case Tcp::ConnectionPool::PoolFailureReason::LocalConnectionFailure:
      // Should only happen if we closed the connection, due to an error condition, in which case
      // we've already handled any possible downstream response.
      parent_.message_.sendLocalReply(
          ErrResponse(trpc::TRPC_SERVER_SYSTEM_ERR,
                      fmt::format("trpc upstream request: local connection failure '{}'", host)),
          false);
      break;
    case Tcp::ConnectionPool::PoolFailureReason::RemoteConnectionFailure:
      parent_.message_.streamInfo().setResponseFlag(
          StreamInfo::ResponseFlag::UpstreamConnectionFailure);
      parent_.message_.sendLocalReply(
          ErrResponse(trpc::TRPC_SERVER_SYSTEM_ERR,
                      fmt::format("trpc upstream request: remote connection failure '{}'", host)),
          false);
      break;
    case Tcp::ConnectionPool::PoolFailureReason::Timeout:
      parent_.message_.streamInfo().setResponseFlag(
          StreamInfo::ResponseFlag::UpstreamConnectionFailure);
      parent_.message_.sendLocalReply(ErrResponse(trpc::TRPC_SERVER_TIMEOUT_ERR,
                                                  fmt::format("trpc upstream request: connection "
                                                              "failure '{}' due to timeout",
                                                              host)),
                                      false);
      break;
    default:
      NOT_REACHED_GCOVR_EXCL_LINE;
  }
  messageFinished(true);
}

}  // namespace Envoy::Extensions::NetworkFilters::TrpcProxy
