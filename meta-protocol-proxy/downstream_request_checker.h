// Copyright (c) 2020, Tencent Inc.
// All rights reserve

#pragma once

#include <memory>
#include <string>
#include <utility>

#include "common/buffer/buffer_impl.h"
#include "common/common/logger.h"
#include "envoy/network/filter.h"
#include "envoy/server/filter_config.h"

#include "trpc/codec_checker.h"
#include "trpc/protocol.h"
#include "trpc/trpc.pb.h"

namespace Envoy::Extensions::NetworkFilters::TrpcProxy {

class ConnectionManager;
class ActiveMessage;
struct MessageMetadata;

// 把downstream发送的请求按照tRPC协议分割
class RequestChecker : public CodecCheckerCallBacks, public Logger::Loggable<Logger::Id::filter> {
 public:
  explicit RequestChecker(ConnectionManager& conn) : parent_(conn), decoder_base_(*this) {}
  ~RequestChecker() override = default;

  void onData(Buffer::Instance& data);
  std::shared_ptr<MessageMetadata> metadata();

  // DecoderBaseCallBacks
  void onFixedHeaderDecoded(std::unique_ptr<TrpcFixedHeader> fixed_header) override {
    fixed_header_ = std::move(fixed_header);
  }
  bool onDecodeRequestProtocol(std::string&& header_raw) override;
  void onCompleted(std::unique_ptr<Buffer::OwnedImpl> buffer) override;

 private:
  ConnectionManager& parent_;
  ActiveMessage* message_{nullptr};
  CodecChecker decoder_base_;
  std::unique_ptr<TrpcFixedHeader> fixed_header_{nullptr};
};

using RequestCheckerPtr = std::unique_ptr<RequestChecker>;

}  // namespace Envoy::Extensions::NetworkFilters::TrpcProxy