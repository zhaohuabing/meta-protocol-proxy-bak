#!/bin/bash
ulimit -c unlimited
ulimit -n 100000
target="envoy-trpc-proxy"
./bazel-bin/$target -c conf/trpc-static.yaml -l debug
