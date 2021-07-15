package(default_visibility = ["//visibility:public"])

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_cc_library",
)

envoy_cc_binary(
    name = "envoy-demo-proxy",
    repository = "@envoy",
    deps = [
        "//demo",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)

envoy_cc_binary(
    name = "envoy-trpc-proxy",
    repository = "@envoy",
    deps = [
        "//meta-protocol-proxy",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)
