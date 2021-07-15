// Copyright (c) 2020, Tencent Inc.
// All rights reserve

#pragma once

#include <vector>

#include "envoy/access_log/access_log.h"
#include "envoy/router/rds.h"
#include "trpc/stats.h"

namespace Envoy::Extensions::NetworkFilters::TrpcProxy {

/**
 * Config is a configuration interface for ConnectionManager.
 */
class Config {
 public:
  virtual ~Config() = default;
  virtual Envoy::Router::RouteConfigProvider* routeConfigProvider() = 0;
  virtual std::vector<AccessLog::InstanceSharedPtr> const& accessLogs() = 0;
  virtual TrpcFilterStats& stats() = 0;
};

}  // namespace Envoy::Extensions::NetworkFilters::TrpcProxy
