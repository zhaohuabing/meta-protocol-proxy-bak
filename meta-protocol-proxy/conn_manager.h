// Copyright (c) 2020, Tencent Inc.
// All rights reserve

#pragma once

#include <cstdint>
#include <list>

#include "common/buffer/buffer_impl.h"
#include "common/common/logger.h"
#include "envoy/common/time.h"
#include "envoy/network/filter.h"
#include "envoy/server/filter_config.h"
#include "envoy/upstream/cluster_manager.h"

#include "trpc/config_interface.h"
#include "trpc/downstream_request_checker.h"
#include "trpc/message.h"
#include "trpc/protocol.h"

namespace Envoy::Extensions::NetworkFilters::TrpcProxy {

class ConnectionManager : public Network::ReadFilter,
                          public Network::ConnectionCallbacks,
                          Logger::Loggable<Logger::Id::filter> {
 public:
  ConnectionManager(Config& config, Server::Configuration::FactoryContext& context);
  ~ConnectionManager() override;

  // Network::ReadFilter
  Network::FilterStatus onData(Buffer::Instance& data, bool end_stream) override;

  Network::FilterStatus onNewConnection() override { return Network::FilterStatus::Continue; }
  void initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) override;

  // Network::ConnectionCallbacks
  void onEvent(Network::ConnectionEvent) override;
  void onAboveWriteBufferHighWatermark() override;
  void onBelowWriteBufferLowWatermark() override;

  Network::Connection* connection();
  Config& config() { return config_; }
  TimeSource& timeSystem() { return time_system_; }
  TrpcFilterStats& stats() { return config_.stats(); }
  Upstream::ClusterManager& clusterManager() { return cluster_manager_; }

  // send response to client
  uint64_t sendLocalReply(MessageMetadata const& meta, DirectResponse const& resp, bool end_stream);
  ActiveMessage* newMessage();

 private:
  friend class ActiveMessage;
  void dispatch();
  void resetAllMessage(bool local_reset);
  void deferredMessage(ActiveMessage& message);

  Config& config_;
  TimeSource& time_system_;
  Upstream::ClusterManager& cluster_manager_;
  Network::ReadFilterCallbacks* read_callbacks_{nullptr};
  Buffer::OwnedImpl request_buffer_;
  std::list<ActiveMessagePtr> active_message_list_;
  RequestCheckerPtr request_checker_;
};

}  // namespace Envoy::Extensions::NetworkFilters::TrpcProxy