// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include "trpc/codec_checker.h"

#include <cstdlib>
#include <string>
#include <utility>

#include "common/common/assert.h"

#include "trpc/protocol.h"

namespace Envoy::Extensions::NetworkFilters::TrpcProxy {

void CodecChecker::onData(Buffer::Instance& buffer, bool* buffer_underflow) {
  ENVOY_LOG(debug, "decoder onData: {}", buffer.length());
  // https://git.code.oa.com/trpc/trpc-protocol/blob/master/docs/protocol_design.md
  *buffer_underflow = false;

  while (decode_stage_ != DecodeStage::kDecodeDone) {
    auto state = handleState(buffer);
    if (state == DecodeStage::kWaitForData) {
      *buffer_underflow = true;
      return;
    }
    decode_stage_ = state;
  }

  ASSERT(decode_stage_ == DecodeStage::kDecodeDone);

  reset();
  *buffer_underflow = (buffer.length() == 0);
  ENVOY_LOG(debug, "trpc decoder: data length {}", buffer.length());
}

CodecChecker::DecodeStage CodecChecker::handleState(Buffer::Instance& buffer) {
  switch (decode_stage_) {
    case DecodeStage::kDecodeFixedHeader:
      return decodeFixedHeader(buffer);
    case DecodeStage::kDecodeProtocolHeader:
      return decodeProtocolHeader(buffer);
    case DecodeStage::kDecodePayload:
      return decodePayload(buffer);
    default:
      NOT_REACHED_GCOVR_EXCL_LINE;
  }
  return DecodeStage::kDecodeDone;
}

CodecChecker::DecodeStage CodecChecker::decodeFixedHeader(Buffer::Instance& buffer) {
  ENVOY_LOG(debug, "decoder FixedHeader: {}", buffer.length());
  if (buffer.length() < TrpcFixedHeader::TRPC_PROTO_PREFIX_SPACE) {
    ENVOY_LOG(debug, "continue {}", buffer.length());
    return DecodeStage::kWaitForData;
  }

  std::unique_ptr<TrpcFixedHeader> fixed_header = std::make_unique<TrpcFixedHeader>();
  if (!fixed_header->decode(buffer, false)) {
    throw EnvoyException(fmt::format("protocol invalid"));
  }

  total_size_ = fixed_header->data_frame_size;
  protocol_header_size_ = fixed_header->pb_header_size;

  call_backs_.onFixedHeaderDecoded(std::move(fixed_header));

  return DecodeStage::kDecodeProtocolHeader;
}

CodecChecker::DecodeStage CodecChecker::decodeProtocolHeader(Buffer::Instance& buffer) {
  ENVOY_LOG(debug, "decoder ProtocolHeader: {}", buffer.length());

  // 数据不全,继续收包
  if (buffer.length() < TrpcFixedHeader::TRPC_PROTO_PREFIX_SPACE + protocol_header_size_) {
    ENVOY_LOG(debug, "continue {}", buffer.length());
    return DecodeStage::kWaitForData;
  }
  std::string header_raw;
  header_raw.reserve(protocol_header_size_);
  header_raw.resize(protocol_header_size_);
  buffer.copyOut(TrpcFixedHeader::TRPC_PROTO_PREFIX_SPACE, protocol_header_size_, &(header_raw[0]));
  trpc::RequestProtocol header;

  if (!call_backs_.onDecodeRequestProtocol(std::move(header_raw))) {
    throw EnvoyException("parse header failed");
  }

  return DecodeStage::kDecodePayload;
}

CodecChecker::DecodeStage CodecChecker::decodePayload(Buffer::Instance& buffer) {
  ENVOY_LOG(debug, "decoder payload {} ? {}", total_size_, buffer.length());

  if (buffer.length() < total_size_) {
    ENVOY_LOG(debug, "continue {}", buffer.length());
    return DecodeStage::kWaitForData;
  }
  std::unique_ptr<Buffer::OwnedImpl> msg = std::make_unique<Buffer::OwnedImpl>();
  msg->move(buffer, static_cast<uint64_t>(total_size_));

  call_backs_.onCompleted(std::move(msg));

  return DecodeStage::kDecodeDone;
}

}  // namespace Envoy::Extensions::NetworkFilters::TrpcProxy