// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "envoy/api/api.h"
#include "envoy/config/core/v3/health_check.pb.h"
#include "envoy/server/health_checker_config.h"
#include "envoy/upstream/outlier_detection.h"

#include "common/buffer/buffer_impl.h"
#include "common/network/filter_impl.h"
#include "common/upstream/health_checker_base_impl.h"

#include "trpc/codec_checker.h"
#include "trpc/health_checker/trpc.pb.h"
#include "trpc/protocol.h"

namespace Envoy::Extensions::HealthCheckers::TrpcProxy {

class TrpcHealthChecker : public Upstream::HealthCheckerImplBase {
 public:
  TrpcHealthChecker(Server::Configuration::HealthCheckerFactoryContext& context,
                    envoy::config::core::v3::HealthCheck const& config,
                    envoy::config::health_checker::trpc_proxy::v3::Trpc&& trpc_config);

  ~TrpcHealthChecker() override = default;

 protected:
  envoy::data::core::v3::HealthCheckerType healthCheckerType() const override {
    return envoy::data::core::v3::TCP;
  }

 private:
  struct TrpcActiveHealthCheckSession;

  struct TrpcSessionCallbacks : public Network::ConnectionCallbacks,
                                public Network::ReadFilterBaseImpl {
    explicit TrpcSessionCallbacks(TrpcActiveHealthCheckSession& parent) : parent_(parent) {}

    // Network::ConnectionCallbacks
    void onEvent(Network::ConnectionEvent event) override { parent_.onEvent(event); }
    void onAboveWriteBufferHighWatermark() override {}
    void onBelowWriteBufferLowWatermark() override {}

    // Network::ReadFilter
    Network::FilterStatus onData(Buffer::Instance& data, bool) override {
      parent_.onData(data);
      return Network::FilterStatus::StopIteration;
    }

    TrpcActiveHealthCheckSession& parent_;
  };

  struct TrpcActiveHealthCheckSession : public ActiveHealthCheckSession,
                                        public NetworkFilters::TrpcProxy::CodecCheckerCallBacks {
    TrpcActiveHealthCheckSession(TrpcHealthChecker& parent, Upstream::HostSharedPtr const& host)
        : ActiveHealthCheckSession(parent, host), parent_(parent) {}
    ~TrpcActiveHealthCheckSession() override;

    bool shouldClose() const;
    uint32_t request_id() { return ++request_id_; }

    void onData(Buffer::Instance& data);
    void onEvent(Network::ConnectionEvent event);

    // ActiveHealthCheckSession
    void onInterval() override;
    void onTimeout() override;
    void onDeferredDelete() final;

    // BaseDecoderCallBacks
    bool onDecodeRequestProtocol(std::string&& raw_str) override {
      return pb_header_.ParseFromString(raw_str);
    }
    void onCompleted(std::unique_ptr<Buffer::OwnedImpl>) override;

    void sendHealthCheckRequest();

    TrpcHealthChecker& parent_;
    Network::ClientConnectionPtr client_;
    std::shared_ptr<TrpcSessionCallbacks> session_callbacks_;
    // If true, stream close was initiated by us, not e.g. remote close or TCP reset.
    // In this case healthcheck status already reported, only state cleanup required.
    bool expect_close_{false};
    uint32_t request_id_{0};
    Buffer::OwnedImpl response_buffer_;
    std::unique_ptr<NetworkFilters::TrpcProxy::CodecChecker> response_checker_;
    trpc::ResponseProtocol pb_header_;
  };

  using TrpcActiveHealthCheckSessionPtr = std::unique_ptr<TrpcActiveHealthCheckSession>;

  // HealthCheckerImplBase
  ActiveHealthCheckSessionPtr makeSession(Upstream::HostSharedPtr host) override {
    return std::make_unique<TrpcActiveHealthCheckSession>(*this, host);
  }

 private:
  envoy::config::health_checker::trpc_proxy::v3::Trpc config_;
};

}  // namespace Envoy::Extensions::HealthCheckers::TrpcProxy