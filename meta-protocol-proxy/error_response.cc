// Copyright (c) 2020, Tencent Inc.
// All rights reserve

#include "trpc/error_response.h"

#include "trpc/metadata.h"

namespace Envoy::Extensions::NetworkFilters::TrpcProxy {

DirectResponse::ResponseType ErrResponse::encode(MessageMetadata const &meta,
                                                 Buffer::Instance &data) const {
  TrpcResponseProtocol response_protocol;
  // rsp_header_
  auto &header = response_protocol.protocol_header_;
  header.set_request_id(meta.request_protocol.request_id());
  header.set_call_type(meta.request_protocol.call_type());
  header.set_version(meta.request_protocol.version());
  header.set_content_type(meta.request_protocol.content_type());
  header.set_content_encoding(meta.request_protocol.content_encoding());
  header.set_error_msg(err_text_);
  header.set_ret(err_code_);
  header.set_func_ret(err_code_);
  // 不需要rsp_body

  response_protocol.encode(data);

  return ResponseType::ErrorReply;
}

}  // namespace Envoy::Extensions::NetworkFilters::TrpcProxy