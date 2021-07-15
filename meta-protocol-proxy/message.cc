// Copyright (c) 2020, Tencent Inc.
// All rights reserve

#include "trpc/message.h"

#include <memory>

#include "common/common/assert.h"
#include "common/stats/timespan_impl.h"

#include "trpc/conn_manager.h"
#include "trpc/protocol.h"
#include "trpc/router.h"

namespace Envoy::Extensions::NetworkFilters::TrpcProxy {

ActiveMessage::ActiveMessage(ConnectionManager& parent)
    : parent_(parent),
      request_timer_(std::make_unique<Stats::HistogramCompletableTimespanImpl>(
          stats().request_time_ms_, parent.timeSystem())),
      stream_info_(parent_.timeSystem()) {
  ENVOY_LOG(debug, "trpc ActiveMessage ctor");
  router_ = std::make_unique<Router>(parent_.clusterManager(), *this);

  stream_info_.setDownstreamLocalAddress(parent_.connection()->localAddress());
  stream_info_.setDownstreamRemoteAddress(parent_.connection()->remoteAddress());
  stream_info_.setDownstreamDirectRemoteAddress(parent_.connection()->directRemoteAddress());
  stats().request_active_.inc();
}

ActiveMessage::~ActiveMessage() {
  ENVOY_LOG(debug, "trpc ActiveMessage destory");
  stats().request_active_.dec();
}

void ActiveMessage::onReset() {
  router_->onReset();
  request_timer_->complete();
  stream_info_.onRequestComplete();
  accessLog();
  parent_.deferredMessage(*this);
}

void ActiveMessage::onSteamDecoded(std::unique_ptr<Buffer::OwnedImpl> data) {
  ENVOY_LOG(debug, "trpc ActiveMessage decoded:{}", metadata_->request_protocol.call_type());
  ASSERT(data->length() == metadata_->pkg_size);
  switch (metadata_->request_protocol.call_type()) {
    case trpc::TRPC_UNARY_CALL:
      stats().request_unary_call_.inc();
      break;
    case trpc::TRPC_ONEWAY_CALL:
      stats().request_oneway_call_.inc();
      one_way_call_ = true;
      break;
    default:
      break;
  }
  stats().request_decoding_success_.inc();
  router_->onMessageDecoded(metadata_, *data);
}

void ActiveMessage::sendLocalReply(DirectResponse const& response, bool end_stream) {
  ASSERT(!replay_);
  replay_ = true;
  stream_info_.response_code_ = response.err_code();
  auto rsp_len = parent_.sendLocalReply(*metadata_, response, end_stream);
  stream_info_.addBytesSent(rsp_len);
}

Network::Connection* ActiveMessage::connection() { return parent_.connection(); }

Envoy::Router::RouteConstSharedPtr ActiveMessage::route() {
  ASSERT(metadata_);

  auto* route_config_provider = parent_.config().routeConfigProvider();
  if (route_config_provider == nullptr) {
    return nullptr;
  }

  // print debug info
  if (ENVOY_LOG_CHECK_LEVEL(debug)) {
    auto config_info = route_config_provider->configInfo();
    if (config_info.has_value()) {
      ENVOY_LOG(debug, "Route config: version='{}' info='{}'", config_info.value().version_,
                config_info.value().config_.ShortDebugString());
    } else {
      ENVOY_LOG(error, "Empty Route config info");
    }
  }

  // do routing
  auto route_config = route_config_provider->config();
  if (auto const* headers = metadata_->requestHttpHeaders(); route_config && headers != nullptr) {
    stream_info_.setRequestHeaders(*headers);
    // TODO(chabbyguo): random
    uint64_t random_value{1};
    return route_config->route(*headers, stream_info_, random_value);
  }

  ENVOY_LOG(error, "Failed to get Route Config");
  return nullptr;
}

void ActiveMessage::accessLog() {
  for (auto const& log_handler : parent_.config().accessLogs()) {
    log_handler->log(metadata_->requestHttpHeaders(), nullptr, nullptr, stream_info_);
  }
}

TrpcFilterStats& ActiveMessage::stats() { return parent_.stats(); }

}  // namespace Envoy::Extensions::NetworkFilters::TrpcProxy
