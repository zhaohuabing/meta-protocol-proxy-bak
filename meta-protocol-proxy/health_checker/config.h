// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#pragma once

#include <string>

#include "envoy/config/core/v3/health_check.pb.h"
#include "envoy/server/health_checker_config.h"

#include "trpc/health_checker/trpc.pb.h"

namespace Envoy::Extensions::HealthCheckers::TrpcProxy {

class TrpcHealthCheckerFactory : public Server::Configuration::CustomHealthCheckerFactory {
 public:
  Upstream::HealthCheckerSharedPtr createCustomHealthChecker(
      envoy::config::core::v3::HealthCheck const& config,
      Server::Configuration::HealthCheckerFactoryContext& context) override;

  [[nodiscard]] std::string name() const override { return "envoy.health_checkers.trpc"; }

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return ProtobufTypes::MessagePtr{new envoy::config::health_checker::trpc_proxy::v3::Trpc()};
  }
};

}  // namespace Envoy::Extensions::HealthCheckers::TrpcProxy