#!/bin/bash
bazel-trpc/external/envoy/tools/stack_decode.py ./bazel-bin/envoy-trpc-proxy -c conf/trpc-static.yaml -l debug
