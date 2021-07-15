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

api_proto_package()

envoy_cc_library(
    name = "health_checker",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    deps = [
        ":pkg_cc_proto",
        ":trpc",
        "@envoy//include/envoy/registry",
        "@envoy//include/envoy/server:health_checker_config_interface",
    ],
)

envoy_cc_library(
    name = "trpc",
    srcs = ["trpc.cc"],
    hdrs = ["trpc.h"],
    repository = "@envoy",
    deps = [
        ":pkg_cc_proto",
        "//trpc:codec_checker",
        "//trpc:protocol",
        "@envoy//include/envoy/buffer:buffer_interface",
        "@envoy//include/envoy/network:connection_interface",
        "@envoy//include/envoy/network:filter_interface",
        "@envoy//include/envoy/server:health_checker_config_interface",
        "@envoy//include/envoy/upstream:outlier_detection_interface",
        "@envoy//source/common/buffer:buffer_lib",
        "@envoy//source/common/common:assert_lib",
        "@envoy//source/common/common:linked_object",
        "@envoy//source/common/common:minimal_logger_lib",
        "@envoy//source/common/network:filter_lib",
        "@envoy//source/common/upstream:health_checker_base_lib",
        "@envoy//source/common/upstream:load_balancer_lib",
        "@envoy//source/extensions/filters/network/common:factory_base_lib",
    ],
)
