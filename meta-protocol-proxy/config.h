// Copyright (c) 2020, Tencent Inc.
// All rights reserve

#pragma once

#include <string>
#include <vector>

#include "common/router/rds_impl.h"
#include "envoy/access_log/access_log.h"
#include "envoy/router/route_config_provider_manager.h"
#include "envoy/server/filter_config.h"
#include "extensions/filters/network/common/factory_base.h"

#include "trpc/config.pb.h"
#include "trpc/config.pb.validate.h"
#include "trpc/config_interface.h"
#include "trpc/stats.h"


namespace Envoy::Extensions::NetworkFilters::TrpcProxy {

constexpr char CanonicalName[] = "envoy.filters.network.trpc_proxy";

class TrpcProxyFilterConfigFactory
    : Logger::Loggable<Logger::Id::config>,
      public Envoy::Extensions::NetworkFilters::Common::FactoryBase<
          envoy::config::filter::network::trpc_proxy::v3::TrpcProxy> {
 public:
  TrpcProxyFilterConfigFactory() : FactoryBase(CanonicalName, true) {}

 private:
  Network::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::config::filter::network::trpc_proxy::v3::TrpcProxy& proto_config,
      Server::Configuration::FactoryContext& context) override;
};

class Utility {
 public:
  struct Singletons {
    std::shared_ptr<Envoy::Router::RouteConfigProviderManager> route_config_provider_manager_;
  };

  static Singletons createSingletons(Server::Configuration::FactoryContext& context);
};

class TrpcRouteConfigProviderManagerImpl : public Envoy::Router::RouteConfigProviderManagerImpl {
 public:
  explicit TrpcRouteConfigProviderManagerImpl(Server::Admin& admin)
      : Envoy::Router::RouteConfigProviderManagerImpl(admin) {}
};

class ConfigImpl : public Config {
 public:
  using TrpcProxyConfig = envoy::config::filter::network::trpc_proxy::v3::TrpcProxy;

 public:
  ConfigImpl(TrpcProxyConfig const& config, Server::Configuration::FactoryContext& context,
             Envoy::Router::RouteConfigProviderManager& route_config_provider_manager);
  ~ConfigImpl() override = default;

  // Config
  Envoy::Router::RouteConfigProvider* routeConfigProvider() override {
    return route_config_provider_.get();
  }
  std::vector<AccessLog::InstanceSharedPtr> const& accessLogs() override { return access_logs_; }
  TrpcFilterStats& stats() override { return stats_; }

 private:
  Server::Configuration::FactoryContext& context_;
  std::string const stats_prefix_;
  TrpcFilterStats stats_;
  Envoy::Router::RouteConfigProviderSharedPtr route_config_provider_;
  Envoy::Router::RouteConfigProviderManager& route_config_provider_manager_;
  std::vector<AccessLog::InstanceSharedPtr> access_logs_;
};

}  // namespace Envoy::Extensions::NetworkFilters::TrpcProxy
