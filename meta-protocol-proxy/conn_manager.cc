// Copyright (c) 2020, Tencent Inc.
// All rights reserve

#include "trpc/conn_manager.h"

#include <memory>
#include <utility>

#include "envoy/network/filter.h"
#include "envoy/server/filter_config.h"

#include "trpc/router.h"

namespace Envoy::Extensions::NetworkFilters::TrpcProxy {

ConnectionManager::ConnectionManager(Config& config, Server::Configuration::FactoryContext& context)
    : config_(config),
      time_system_(context.dispatcher().timeSource()),
      cluster_manager_(context.clusterManager()),
      request_checker_(std::make_unique<RequestChecker>(*this)) {}

ConnectionManager::~ConnectionManager() = default;

Network::FilterStatus ConnectionManager::onData(Buffer::Instance& data, bool end_stream) {
  ENVOY_CONN_LOG(debug, "trpc filter: onData {} bytes, stream {}", read_callbacks_->connection(),
                 data.length(), end_stream);

  request_buffer_.move(data);
  dispatch();

  if (end_stream) {
    resetAllMessage(false);
    ENVOY_CONN_LOG(trace, "downstream half-closed", read_callbacks_->connection());
    read_callbacks_->connection().close(Network::ConnectionCloseType::FlushWrite);
  }
  return Network::FilterStatus::StopIteration;
}

Network::Connection* ConnectionManager::connection() { return &read_callbacks_->connection(); }

uint64_t ConnectionManager::sendLocalReply(MessageMetadata const& meta, DirectResponse const& resp,
                                           bool end_stream) {
  uint64_t resp_len{0};
  if (read_callbacks_->connection().state() != Network::Connection::State::Open) {
    return resp_len;
  }
  try {
    Buffer::OwnedImpl buffer;
    resp.encode(meta, buffer);
    resp_len = buffer.length();
    read_callbacks_->connection().write(buffer, end_stream);
  } catch (EnvoyException const& ex) {
    ENVOY_CONN_LOG(error, "trpc filter: error: {}", read_callbacks_->connection(), ex.what());
  }

  if (end_stream) {
    read_callbacks_->connection().close(Network::ConnectionCloseType::FlushWrite);
  }
  return resp_len;
}

void ConnectionManager::resetAllMessage(bool local_reset) {
  while (!active_message_list_.empty()) {
    if (local_reset) {
      ENVOY_CONN_LOG(debug, "local close with active request", read_callbacks_->connection());
      stats().cx_destroy_local_with_active_rq_.inc();
    } else {
      ENVOY_CONN_LOG(debug, "remote close with active request", read_callbacks_->connection());
      stats().cx_destroy_remote_with_active_rq_.inc();
    }
    active_message_list_.front()->onReset();
  }
}

void ConnectionManager::deferredMessage(ActiveMessage& message) {
  if (!message.inserted()) {
    return;
  }
  read_callbacks_->connection().dispatcher().deferredDelete(
      message.removeFromList(active_message_list_));
}

void ConnectionManager::dispatch() {
  if (request_buffer_.length() == 0) {
    return;
  }

  try {
    request_checker_->onData(request_buffer_);
  } catch (EnvoyException const& ex) {
    ENVOY_CONN_LOG(error, "trpc filter: request decode error: {}", read_callbacks_->connection(),
                   ex.what());
    read_callbacks_->connection().close(Network::ConnectionCloseType::NoFlush);
    stats().request_decoding_error_.inc();
    resetAllMessage(true);
  }
}

void ConnectionManager::initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) {
  ENVOY_LOG(debug, "trpc filter: initializeReadFilterCallbacks");
  read_callbacks_ = &callbacks;
  read_callbacks_->connection().addConnectionCallbacks(*this);
  read_callbacks_->connection().enableHalfClose(true);
}

void ConnectionManager::onEvent(Network::ConnectionEvent event) {
  ENVOY_LOG(debug, "trpc filter: onEvent {}", event);

  if (event != Network::ConnectionEvent::Connected) {
    resetAllMessage(event == Network::ConnectionEvent::LocalClose);
  }
}

void ConnectionManager::onAboveWriteBufferHighWatermark() {
  ENVOY_CONN_LOG(debug, "trpc filter: onAboveWriteBufferHighWatermark",
                 read_callbacks_->connection());
  read_callbacks_->connection().readDisable(true);
}

void ConnectionManager::onBelowWriteBufferLowWatermark() {
  ENVOY_CONN_LOG(debug, "trpc filter: onBelowWriteBufferLowWatermark",
                 read_callbacks_->connection());
  read_callbacks_->connection().readDisable(false);
}

ActiveMessage* ConnectionManager::newMessage() {
  ENVOY_LOG(debug, "trpc filter: newMessage");

  auto new_message = std::make_unique<ActiveMessage>(*this);
  LinkedList::moveIntoList(std::move(new_message), active_message_list_);
  return (*active_message_list_.begin()).get();
}

}  // namespace Envoy::Extensions::NetworkFilters::TrpcProxy
