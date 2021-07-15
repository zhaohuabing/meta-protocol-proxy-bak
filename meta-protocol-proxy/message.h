// Copyright (c) 2020, Tencent Inc.
// All rights reserve

#pragma once

#include <memory>

#include "common/buffer/buffer_impl.h"
#include "common/common/linked_object.h"
#include "common/common/logger.h"
#include "common/stream_info/stream_info_impl.h"
#include "envoy/buffer/buffer.h"
#include "envoy/event/deferred_deletable.h"
#include "envoy/network/connection.h"
#include "envoy/router/router.h"
#include "envoy/stats/timespan.h"

#include "trpc/stats.h"

namespace Envoy::Extensions::NetworkFilters::TrpcProxy {

class ConnectionManager;
class DirectResponse;
struct MessageMetadata;
class Router;

class ActiveMessage : public LinkedObject<ActiveMessage>,
                      public Event::DeferredDeletable,
                      Logger::Loggable<Logger::Id::filter> {
 public:
  explicit ActiveMessage(ConnectionManager& parent);
  ~ActiveMessage() override;

  // continue decode
  // void continueDecoding();

  void sendLocalReply(DirectResponse const& response, bool end_stream);

  Network::Connection* connection();
  // UpstreamResponseStatus onUpstreamData(Buffer::Instance& data);

  Envoy::Router::RouteConstSharedPtr route();
  // Event::Dispatcher& dispatcher();
  void onSteamDecoded(std::unique_ptr<Buffer::OwnedImpl>);
  void onReset();
  // void onError(std::string const& what);
  StreamInfo::StreamInfo& streamInfo() { return stream_info_; }
  void accessLog();
  TrpcFilterStats& stats();

 public:
  std::shared_ptr<MessageMetadata> metadata_;
  bool one_way_call_{false};

 private:
  ConnectionManager& parent_;
  Stats::TimespanPtr request_timer_;
  std::unique_ptr<Router> router_;
  StreamInfo::StreamInfoImpl stream_info_;
  bool replay_{false};
};

using ActiveMessagePtr = std::unique_ptr<ActiveMessage>;

}  // namespace Envoy::Extensions::NetworkFilters::TrpcProxy
