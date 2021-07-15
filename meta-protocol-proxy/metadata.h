// Copyright (c) 2020, Tencent Inc.
// All rights reserve

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "common/http/header_map_impl.h"

#include "trpc/trpc.pb.h"

namespace Envoy::Extensions::NetworkFilters::TrpcProxy {

struct MessageMetadata {
  uint32_t pkg_size{0};
  // tRPC数据包的包头
  trpc::RequestProtocol request_protocol;
  // http头，由tRPC协议转换而来的，用于适配http的RDS。
  Http::RequestHeaderMapPtr http_request_headers;

  std::string service_name() const { return request_protocol.callee(); }

  uint32_t request_id() const { return request_protocol.request_id(); }

  void buildHttpHeaders();

  Http::RequestHeaderMap const* requestHttpHeaders() {
    if (!http_request_headers) {
      buildHttpHeaders();
    }
    return http_request_headers.get();
  }
};

using MessageMetadataSharedPtr = std::shared_ptr<MessageMetadata>;

}  // namespace Envoy::Extensions::NetworkFilters::TrpcProxy