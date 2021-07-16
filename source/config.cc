#include "source/config.h"
#include "source/conn_manager.h"

#include "envoy/extensions/filters/network/meta_protocol_proxy/v1alpha/meta_protcol_proxy.pb.h"
#include "envoy/registry/registry.h"

#include "common/config/utility.h"

#include "absl/container/flat_hash_map.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace MetaProtocolProxy {

Network::FilterFactoryCb MetaProtocolProxyFilterConfigFactory::createFilterFactoryFromProtoTyped(
    const envoy::extensions::filters::network::meta_protocol_proxy::v1alpha::MetaProtocolProxy& proto_config,
    Server::Configuration::FactoryContext& context) {
  //std::shared_ptr<Config> filter_config(std::make_shared<ConfigImpl>(proto_config, context));

  return [filter_config, &context](Network::FilterManager& filter_manager) -> void {
    filter_manager.addReadFilter(std::make_shared<ConnectionManager>(
        *filter_config, context.api().randomGenerator(), context.dispatcher().timeSource()));
  };
}

/**
 * Static registration for the dubbo filter. @see RegisterFactory.
 */
REGISTER_FACTORY(MetaProtocolProxyFilterConfigFactory,
                 Server::Configuration::NamedNetworkFilterConfigFactory);
} // namespace MetaProtocolProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
