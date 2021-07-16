#pragma once

#include <string>

#include "api/v1alpha/meta_protocol_proxy.pb.h"

#include "extensions/filters/network/common/factory_base.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace MetaProtocolProxy {

/**
 * Config registration for the meta protocol proxy filter. @see NamedNetworkFilterConfigFactory.
 */
class MetaProtocolProxyFilterConfigFactory
    : public Common::FactoryBase<envoy::extensions::filters::network::meta_protocol_proxy::v1alpha::MetaProtocolProxy> {
public:
    // Meta protocol proxy filter
    const std::string MetaProtocolProxy = "envoy.filters.network.meta_protocol_proxy";

    MetaProtocolProxyFilterConfigFactory() : FactoryBase(MetaProtocolProxy, true) {}

private:
  Network::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::extensions::filters::network::meta_protocol_proxy::v1alpha::MetaProtocolProxy& proto_config,
      Server::Configuration::FactoryContext& context) override;
};

} // namespace MetaProtocolProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
