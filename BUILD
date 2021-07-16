package(default_visibility = ["//visibility:public"])

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_cc_library",
)

envoy_cc_binary(
    name = "meta_protocol_proxy",
    repository = "@envoy",
    deps = [
        "//api/proto",
	"@envoy//source/exe:envoy_main_entry_lib",
    ],
)
