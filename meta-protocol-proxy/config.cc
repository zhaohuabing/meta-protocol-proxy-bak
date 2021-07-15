// Copyright (c) 2020, Tencent Inc.
// All rights reserve

#include "trpc/config.h"

#include <memory>

#include "common/access_log/access_log_impl.h"
#include "envoy/registry/registry.h"

#include "trpc/conn_manager.h"

namespace Envoy::Extensions::NetworkFilters::TrpcProxy {

/**
 * Macro used to statically register singletons managed by the singleton manager
 * defined in envoy/singleton/manager.h. After the NAME has been registered use the
 * SINGLETON_MANAGER_REGISTERED_NAME macro to access the name registered with the
 * singleton manager.
 */
#define TRPC_SINGLETON_MANAGER_REGISTRATION(NAME)  \
  static constexpr char NAME##_singleton_name[] = #NAME "_singleton";

#define TRPC_SINGLETON_MANAGER_REGISTERED_NAME(NAME) NAME##_singleton_name

TRPC_SINGLETON_MANAGER_REGISTRATION(route_config_provider_manager);

Network::FilterFactoryCb TrpcProxyFilterConfigFactory::createFilterFactoryFromProtoTyped(
    envoy::config::filter::network::trpc_proxy::v3::TrpcProxy const& proto_config,
    Server::Configuration::FactoryContext& context) {
  Utility::Singletons singletons;

  /**issue#27
   * 从全局tracker中找到routes，判断rds的singletons是否初始化过
   * 找不到就直接创建，找到了直接从配置中提取
  */
  auto iter = context.admin().getConfigTracker().getCallbacksMap().find("routes");
  if (iter == context.admin().getConfigTracker().getCallbacksMap().end()) {
    singletons = Utility::createSingletons(context);
  } else {
    singletons = {
      context.singletonManager().getTyped<Envoy::Router::RouteConfigProviderManagerImpl>(
      TRPC_SINGLETON_MANAGER_REGISTERED_NAME(route_config_provider_manager),
      []{ return nullptr;})
    };
  }
  auto filter_config = std::make_shared<ConfigImpl>(proto_config, context,
                                                    *(singletons.route_config_provider_manager_));

  return [singletons, filter_config, &context](Network::FilterManager& filter_manager) {
    filter_manager.addReadFilter(std::make_shared<ConnectionManager>(*filter_config, context));
  };
}

REGISTER_FACTORY(TrpcProxyFilterConfigFactory,
                 Server::Configuration::NamedNetworkFilterConfigFactory);

// Singleton registration via macro defined in envoy/singleton/manager.h
SINGLETON_MANAGER_REGISTRATION(trpc_route_config_provider_manager);

Utility::Singletons Utility::createSingletons(Server::Configuration::FactoryContext& context) {
  return {context.singletonManager().getTyped<TrpcRouteConfigProviderManagerImpl>(
      SINGLETON_MANAGER_REGISTERED_NAME(trpc_route_config_provider_manager), [&context] {
        return std::make_shared<TrpcRouteConfigProviderManagerImpl>(context.admin());
      })};
}

ConfigImpl::ConfigImpl(TrpcProxyConfig const& config,
                       Server::Configuration::FactoryContext& context,
                       Envoy::Router::RouteConfigProviderManager& route_config_provider_manager)
    : context_(context),
      stats_prefix_(fmt::format("trpc.{}.", config.stat_prefix())),
      stats_(TrpcFilterStats::generateStats(stats_prefix_, context_.scope())),
      route_config_provider_manager_(route_config_provider_manager) {
  switch (config.route_specifier_case()) {
    case TrpcProxyConfig::RouteSpecifierCase::kRds:
      route_config_provider_ = route_config_provider_manager_.createRdsRouteConfigProvider(
          config.rds(), context_.getServerFactoryContext(), stats_prefix_, context_.initManager());
      break;
    case TrpcProxyConfig::RouteSpecifierCase::kRouteConfig:
      route_config_provider_ = route_config_provider_manager_.createStaticRouteConfigProvider(
          config.route_config(), context_.getServerFactoryContext(),
          context_.messageValidationVisitor());
      break;
    default:
      NOT_REACHED_GCOVR_EXCL_LINE;
  }

  access_logs_.reserve(config.access_log_size());
  for (auto const& log_config : config.access_log()) {
    access_logs_.emplace_back(AccessLog::AccessLogFactory::fromProto(log_config, context_));
  }
}

}  // namespace Envoy::Extensions::NetworkFilters::TrpcProxy
