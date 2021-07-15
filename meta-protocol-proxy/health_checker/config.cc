// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include "trpc/health_checker/config.h"

#include <memory>
#include <utility>

#include "common/common/assert.h"
#include "envoy/registry/registry.h"

#include "trpc/health_checker/trpc.h"

namespace Envoy::Extensions::HealthCheckers::TrpcProxy {

Upstream::HealthCheckerSharedPtr TrpcHealthCheckerFactory::createCustomHealthChecker(
    envoy::config::core::v3::HealthCheck const& config,
    Server::Configuration::HealthCheckerFactoryContext& context) {
  envoy::config::health_checker::trpc_proxy::v3::Trpc trpc;

  RELEASE_ASSERT(config.custom_health_check().typed_config().UnpackTo(&trpc),
                 "Illegal custom_health_check config type");

  return std::make_shared<TrpcHealthChecker>(context, config, std::move(trpc));
}

REGISTER_FACTORY(TrpcHealthCheckerFactory, Server::Configuration::CustomHealthCheckerFactory);

}  // namespace Envoy::Extensions::HealthCheckers::TrpcProxy