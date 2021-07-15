// Copyright (c) 2020, Tencent Inc.
// All rights reserve

#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "common/buffer/buffer_impl.h"

#include "trpc/protocol.h"

namespace Envoy::Extensions::NetworkFilters::TrpcProxy {

class ErrResponse : public DirectResponse {
 public:
  ErrResponse(int32_t err_code, std::string &&err_text)
      : err_code_(err_code), err_text_(std::move(err_text)) {}

  ~ErrResponse() override = default;

  ResponseType encode(MessageMetadata const &meta, Buffer::Instance &data) const override;

  [[nodiscard]] int32_t err_code() const override { return err_code_; }

 private:
  int32_t err_code_;
  std::string err_text_;
};

}  // namespace Envoy::Extensions::NetworkFilters::TrpcProxy