#pragma once

#include <string>

#include "api/v1alpha/meta_protocol_proxy.pb.h"
#include "api/v1alpha/meta_protocol_proxy.pb.validate.h"

#include "extensions/filters/network/common/factory_base.h"

#include "source/conn_manager.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace MetaProtocolProxy {

constexpr char CanonicalName[] = "envoy.filters.network.meta_protocol_proxy";

/**
 * Config registration for the meta protocol proxy filter. @see NamedNetworkFilterConfigFactory.
 */
class MetaProtocolProxyFilterConfigFactory
    : public Common::FactoryBase<envoy::extensions::filters::network::meta_protocol_proxy::v1alpha::MetaProtocolProxy> {
public:
    // Meta protocol proxy filter
    const std::string MetaProtocolProxy = "envoy.filters.network.meta_protocol_proxy";

    MetaProtocolProxyFilterConfigFactory() : FactoryBase(CanonicalName, true) {}

private:
  Network::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::extensions::filters::network::meta_protocol_proxy::v1alpha::MetaProtocolProxy& proto_config,
      Server::Configuration::FactoryContext& context) override;
};

class ConfigImpl : public Config,
                   //public Router::Config,
                   //public DubboFilters::FilterChainFactory,
                   Logger::Loggable<Logger::Id::config> {
public:
  using MetaProctolProxyConfig = envoy::extensions::filters::network::meta_protocol_proxy::v1alpha::MetaProtocolProxy;
  //using MetaProctolFilterConfig = envoy::extensions::filters::network::meta_protocol_proxy::v1alpha::MetaProtocolFilter;

  ConfigImpl(const MetaProctolProxyConfig& config, Server::Configuration::FactoryContext& context);
  ~ConfigImpl() override = default;

  // DubboFilters::FilterChainFactory
  //void createFilterChain(DubboFilters::FilterChainFactoryCallbacks& callbacks) override;

  // Router::Config
  //Router::RouteConstSharedPtr route(const MessageMetadata& metadata,
  //                                  uint64_t random_value) const override;

  // Config
  //DubboFilterStats& stats() override { return stats_; }
  //DubboFilters::FilterChainFactory& filterFactory() override { return *this; }
  //Router::Config& routerConfig() override { return *this; }
  //ProtocolPtr createProtocol() override;

//private:
  //void registerFilter(const DubboFilterConfig& proto_config);

  Server::Configuration::FactoryContext& context_;
  const std::string stats_prefix_;
  //DubboFilterStats stats_;
  //const SerializationType serialization_type_;
  //const ProtocolType protocol_type_;
  //Router::RouteMatcherPtr route_matcher_;

  //std::list<DubboFilters::FilterFactoryCb> filter_factories_;
};

} // namespace MetaProtocolProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
