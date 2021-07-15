// Copyright (c) 2020, Tencent Inc.
// All rights reserve

#include "trpc/downstream_request_checker.h"

#include <cstdlib>
#include <string>
#include <utility>

#include "common/buffer/buffer_impl.h"
#include "common/common/assert.h"
#include "common/common/logger.h"

#include "trpc/conn_manager.h"
#include "trpc/message.h"
#include "trpc/metadata.h"

namespace Envoy::Extensions::NetworkFilters::TrpcProxy {

void RequestChecker::onData(Buffer::Instance& buffer) {
  ENVOY_LOG(debug, "decoder onData: {}", buffer.length());
  // https://git.code.oa.com/trpc/trpc-protocol/blob/master/docs/protocol_design.md
  bool underflow = false;
  // 表示是否还需要再从缓冲区读取数据；如果值是true表示当前数据不完整，
  // 需要读取到更多数据后再进行协议解析。
  while (!underflow) {
    decoder_base_.onData(buffer, &underflow);
  }
}

bool RequestChecker::onDecodeRequestProtocol(std::string&& header_raw) {
  trpc::RequestProtocol header;
  if (!header.ParseFromString(header_raw)) {
    return false;
  }
  message_ = parent_.newMessage();
  message_->metadata_ = std::make_shared<MessageMetadata>();
  message_->metadata_->pkg_size = fixed_header_->data_frame_size;
  message_->metadata_->request_protocol = std::move(header);
  return true;
}

void RequestChecker::onCompleted(std::unique_ptr<Buffer::OwnedImpl> buffer) {
  ASSERT(buffer->length() == message_->metadata_->pkg_size);
  // TODO(chabbyguo): health_checker 在这里回包
  message_->onSteamDecoded(std::move(buffer));
  message_ = nullptr;
}

std::shared_ptr<MessageMetadata> RequestChecker::metadata() { return message_->metadata_; }

}  // namespace Envoy::Extensions::NetworkFilters::TrpcProxy
