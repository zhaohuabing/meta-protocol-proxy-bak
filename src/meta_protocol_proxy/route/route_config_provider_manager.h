#pragma once

#include <memory>

#include "envoy/server/factory_context.h"

#include "api/v1alpha/route.pb.h"
#include "api/v1alpha/meta_protocol_proxy.pb.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace MetaProtocolProxy {
namespace Route {

/**
 * The RouteConfigProviderManager exposes the ability to get a RouteConfigProvider.
 */
class RouteConfigProviderManager {
public:
  virtual ~RouteConfigProviderManager() = default;
  using OptionalHttpFilters = absl::flat_hash_set<std::string>;

  /**
   * Get a RouteConfigProviderPtr for a route from RDS. Ownership of the RouteConfigProvider is the
   * MetaProtocol ConnectionManagers who calls this function. The RouteConfigProviderManager holds
   * raw pointers to the RouteConfigProviders. Clean up of the pointers happen from the destructor
   * of the RouteConfigProvider. This method creates a RouteConfigProvider which may share the
   * underlying RDS subscription with the same (route_config_name, cluster).
   * @param rds supplies the proto configuration of an RDS-configured RouteConfigProvider.
   * @param factory_context is the context to use for the route config provider.
   * @param stat_prefix supplies the stat_prefix to use for the provider stats.
   * @param init_manager the Init::Manager used to coordinate initialization of a the underlying RDS
   * subscription.
   */
  virtual RouteConfigProviderSharedPtr createRdsRouteConfigProvider(
      const envoy::extensions::filters::network::meta_protocol_proxy::v1alpha::Rds& rds,
      Server::Configuration::ServerFactoryContext& factory_context, const std::string& stat_prefix,
      Init::Manager& init_manager) PURE;

  /**
   * Get a RouteConfigSharedPtr for a statically defined route. Ownership is as described for
   * getRdsRouteConfigProvider above. This method always create a new RouteConfigProvider.
   * @param route_config supplies the RouteConfiguration for this route
   * @param factory_context is the context to use for the route config provider.
   * @param validator is the message validator for route config.
   */
  virtual RouteConfigProviderPtr createStaticRouteConfigProvider(
      const envoy::extensions::filters::network::meta_protocol_proxy::v1alpha::RouteConfiguration&
          route_config,
      Server::Configuration::ServerFactoryContext& factory_context,
      ProtobufMessage::ValidationVisitor& validator) PURE;
};

using RouteConfigProviderManagerPtr = std::unique_ptr<RouteConfigProviderManager>;
using RouteConfigProviderManagerSharedPtr = std::shared_ptr<RouteConfigProviderManager>;

} // namespace Route
} // namespace MetaProtocolProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
