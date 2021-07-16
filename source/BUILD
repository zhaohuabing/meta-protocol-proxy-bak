licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_extension",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

# compile proto
load("@envoy_api//bazel:api_build_system.bzl", "api_proto_package")

api_proto_package(
    deps = [
        "//api/v1alpha:pkg",
        "@envoy_api//envoy/config/accesslog/v3:pkg",
        "@envoy_api//envoy/config/core/v3:pkg",
        "@envoy_api//envoy/config/route/v3:pkg",
        "@envoy_api//envoy/extensions/filters/network/http_connection_manager/v3:pkg",
    ],
)

envoy_cc_library(
    name = "meta_protocol_proxy",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    deps = [
        "//api/v1alpha:pkg_cc_proto",
        ":conn_manager",
        "@envoy//include/envoy/access_log:access_log_interface",
        "@envoy//include/envoy/registry",
        "@envoy//include/envoy/router:route_config_provider_manager_interface",
        "@envoy//include/envoy/server:filter_config_interface",
        "@envoy//source/common/access_log:access_log_lib",
        "@envoy//source/common/router:rds_lib",
        "@envoy//source/extensions/filters/network/common:factory_base_lib",
    ],
)

envoy_cc_library(
    name = "conn_manager",
    srcs = [
        "conn_manager.cc",
    ],
    hdrs = [
        "conn_manager.h",
    ],
    repository = "@envoy",
    deps = [
        "//api/v1alpha:pkg_cc_proto",
        "@envoy//include/envoy/buffer:buffer_interface",
        "@envoy//include/envoy/common:time_interface",
        "@envoy//include/envoy/event:deferred_deletable",
        "@envoy//include/envoy/network:connection_interface",
        "@envoy//include/envoy/network:filter_interface",
        "@envoy//include/envoy/server:filter_config_interface",
        "@envoy//include/envoy/stats:timespan_interface",
        "@envoy//include/envoy/upstream:cluster_manager_interface",
        "@envoy//source/common/buffer:buffer_lib",
        "@envoy//source/common/common:assert_lib",
        "@envoy//source/common/common:linked_object",
        "@envoy//source/common/common:minimal_logger_lib",
        "@envoy//source/common/stats:timespan_lib",
        "@envoy//source/common/stream_info:stream_info_lib",
        "@envoy//source/common/upstream:load_balancer_lib",
        "@envoy//source/extensions/filters/network/common:factory_base_lib",
    ],
)
