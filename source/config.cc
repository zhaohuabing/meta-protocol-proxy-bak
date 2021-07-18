#include "source/config.h"
#include "source/conn_manager.h"

#include "api/v1alpha/meta_protocol_proxy.pb.h"
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
  std::shared_ptr<Config> filter_config(std::make_shared<ConfigImpl>(proto_config, context));

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

// class ConfigImpl.
ConfigImpl::ConfigImpl(const MetaProctolProxyConfig& config,
                       Server::Configuration::FactoryContext& context)
    : context_(context);//, stats_prefix_(fmt::format("metaprotocol.{}.", config.stat_prefix())),
      //stats_(DubboFilterStats::generateStats(stats_prefix_, context_.scope())),
      //serialization_type_(
      //    SerializationTypeMapper::lookupSerializationType(config.serialization_type())),
      //protocol_type_(ProtocolTypeMapper::lookupProtocolType(config.protocol_type())) {
  //auto type = RouteMatcherTypeMapper::lookupRouteMatcherType(config.protocol_type());
  //route_matcher_ = Router::NamedRouteMatcherConfigFactory::getFactory(type).createRouteMatcher(
      //config.route_config(), context);
  /*if (config.dubbo_filters().empty()) {
    ENVOY_LOG(debug, "using default router filter");

    envoy::extensions::filters::network::dubbo_proxy::v3::DubboFilter router_config;
    router_config.set_name(DubboFilters::DubboFilterNames::get().ROUTER);
    registerFilter(router_config);
  } else {
    for (const auto& filter_config : config.dubbo_filters()) {
      registerFilter(filter_config);
    }
  }*/
}

/*void ConfigImpl::createFilterChain(DubboFilters::FilterChainFactoryCallbacks& callbacks) {
  for (const DubboFilters::FilterFactoryCb& factory : filter_factories_) {
    factory(callbacks);
  }
}

Router::RouteConstSharedPtr ConfigImpl::route(const MessageMetadata& metadata,
                                              uint64_t random_value) const {
  return route_matcher_->route(metadata, random_value);
}

ProtocolPtr ConfigImpl::createProtocol() {
  return NamedProtocolConfigFactory::getFactory(protocol_type_).createProtocol(serialization_type_);
}

void ConfigImpl::registerFilter(const DubboFilterConfig& proto_config) {
  const auto& string_name = proto_config.name();
  ENVOY_LOG(debug, "    dubbo filter #{}", filter_factories_.size());
  ENVOY_LOG(debug, "      name: {}", string_name);
  ENVOY_LOG(debug, "    config: {}",
            MessageUtil::getJsonStringFromMessageOrError(proto_config.config(), true));

  auto& factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<DubboFilters::NamedDubboFilterConfigFactory>(
          string_name);
  ProtobufTypes::MessagePtr message = factory.createEmptyConfigProto();
  Envoy::Config::Utility::translateOpaqueConfig(proto_config.config(),
                                                ProtobufWkt::Struct::default_instance(),
                                                context_.messageValidationVisitor(), *message);
  DubboFilters::FilterFactoryCb callback =
      factory.createFilterFactoryFromProto(*message, stats_prefix_, context_);

  filter_factories_.push_back(callback);
}*/

} // namespace MetaProtocolProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
