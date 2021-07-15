// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#pragma once

#include <string>

#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"

namespace Envoy::Extensions::NetworkFilters::TrpcProxy {

/**
 * All tRPC filter stats. @see stats_macros.h
 */
#define ALL_TRPC_FILTER_STATS(COUNTER, GAUGE, HISTOGRAM) \
  COUNTER(request_decoding_success)                      \
  COUNTER(request_decoding_error)                        \
  COUNTER(request_oneway_call)                           \
  COUNTER(request_unary_call)                            \
  COUNTER(no_conn_pool)                                  \
  COUNTER(dismatch_route)                                \
  COUNTER(unknow_cluster)                                \
  COUNTER(conn_pool_failure)                             \
  COUNTER(conn_pool_remote_close)                        \
  COUNTER(conn_pool_local_close)                         \
  COUNTER(response_success)                              \
  COUNTER(response_decoding_error)                       \
  COUNTER(response_different_request_id)                 \
  COUNTER(cx_destroy_local_with_active_rq)               \
  COUNTER(cx_destroy_remote_with_active_rq)              \
  GAUGE(request_active, Accumulate)                      \
  HISTOGRAM(request_time_ms, Milliseconds)

/**
 * Struct definition for all tRPC proxy stats. @see stats_macros.h
 */
struct TrpcFilterStats {
  ALL_TRPC_FILTER_STATS(GENERATE_COUNTER_STRUCT, GENERATE_GAUGE_STRUCT, GENERATE_HISTOGRAM_STRUCT)

  static TrpcFilterStats generateStats(std::string const& prefix, Stats::Scope& scope) {
    return TrpcFilterStats{ALL_TRPC_FILTER_STATS(POOL_COUNTER_PREFIX(scope, prefix),
                                                 POOL_GAUGE_PREFIX(scope, prefix),
                                                 POOL_HISTOGRAM_PREFIX(scope, prefix))};
  }
};

}  // namespace Envoy::Extensions::NetworkFilters::TrpcProxy