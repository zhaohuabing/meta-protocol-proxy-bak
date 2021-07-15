// Copyright (c) 2020, Tencent Inc.
// All rights reserve

#pragma once

#include <memory>
#include <string>
#include <unordered_set>

#include "common/buffer/buffer_impl.h"
#include "common/upstream/load_balancer_impl.h"
#include "envoy/router/router.h"
#include "envoy/server/filter_config.h"
#include "envoy/tcp/conn_pool.h"

#include "trpc/codec_checker.h"
#include "trpc/error_response.h"
#include "trpc/message.h"
#include "trpc/metadata.h"
#include "trpc/trpc.pb.h"

namespace Envoy::Extensions::NetworkFilters::TrpcProxy {

class ConnectionManager;
class ActiveMessage;

class Router : public Tcp::ConnectionPool::UpstreamCallbacks,
               public CodecCheckerCallBacks,
               public Upstream::LoadBalancerContextBase,
               public Logger::Loggable<Logger::Id::filter> {
 public:
  Router(Upstream::ClusterManager& cluster_manager, ActiveMessage& message_);
  ~Router() override = default;

  // Tcp::ConnectionPool::UpstreamCallbacks
  void onUpstreamData(Buffer::Instance& data, bool end_stream) override;
  void onEvent(Network::ConnectionEvent event) override;
  void onAboveWriteBufferHighWatermark() override {}
  void onBelowWriteBufferLowWatermark() override {}

  // Upstream::LoadBalancerContext
  absl::optional<uint64_t> computeHashKey() override;
  [[nodiscard]] Network::Connection const* downstreamConnection() const override;
  Envoy::Router::MetadataMatchCriteria const* metadataMatchCriteria() override;
  Http::RequestHeaderMap const* downstreamHeaders() const override;
  bool shouldSelectAnotherHost(Upstream::Host const&) override;
  [[nodiscard]] uint32_t hostSelectionRetryCount() const override { return 10; }

  void onMessageDecoded(std::shared_ptr<MessageMetadata> const& metadata,
                        Envoy::Buffer::OwnedImpl& data);

  // CodecCheckerCallBacks
  bool onDecodeRequestProtocol(std::string&& str) override {
    return response_protocol_.ParseFromString(str);
  }
  void onCompleted(std::unique_ptr<Buffer::OwnedImpl> msg) override;

  void dispatch();
  void onReset();

 private:
  struct UpstreamRequest : public Tcp::ConnectionPool::Callbacks {
    explicit UpstreamRequest(Router& parent) : parent_(parent) {}
    ~UpstreamRequest() override;

    // Tcp::ConnectionPool::Callbacks
    void onPoolFailure(Tcp::ConnectionPool::PoolFailureReason reason,
                       Upstream::HostDescriptionConstSharedPtr host) override;
    void onPoolReady(Tcp::ConnectionPool::ConnectionDataPtr&& conn,
                     Upstream::HostDescriptionConstSharedPtr host) override;

    void onReset();
    void start(Tcp::ConnectionPool::Instance& pool);
    void onUpstreamHostSelected(Upstream::HostDescriptionConstSharedPtr const& host);
    void onResetStream(Tcp::ConnectionPool::PoolFailureReason reason);
    /**
     * 转发完成，销毁ActiveMessage（暂时不考虑流式）
     * @param reset true:会关闭从conn pool里拿出来的连接
     */
    void messageFinished(bool reset = false);


    Router& parent_;
    Tcp::ConnectionPool::Cancellable* conn_pool_handle_{nullptr};
    Tcp::ConnectionPool::ConnectionDataPtr conn_data_;
    Upstream::HostDescriptionConstSharedPtr upstream_host_;
    std::unordered_set<std::string> failed_hosts_;
    bool retry_{false};
    bool response_complete_{false};
  };

  Upstream::ClusterManager& cluster_manager_;
  ActiveMessage& message_;
  MessageMetadataSharedPtr downstream_metadata_;
  std::unique_ptr<UpstreamRequest> upstream_request_;
  Buffer::OwnedImpl upstream_request_buffer_;
  Envoy::Router::RouteConstSharedPtr route_;
  Envoy::Router::RouteEntry const* route_entry_{nullptr};
  // 用于upstream回包的拆分
  trpc::ResponseProtocol response_protocol_;
  Buffer::OwnedImpl upstream_response_buffer_;
  CodecChecker response_checker_;
  bool end_stream_{false};
};

}  // namespace Envoy::Extensions::NetworkFilters::TrpcProxy
