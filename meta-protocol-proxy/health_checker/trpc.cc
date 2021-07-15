// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include "trpc/health_checker/trpc.h"

#include "common/buffer/buffer_impl.h"
#include "common/common/assert.h"

#include "trpc/trpc.pb.h"

namespace Envoy::Extensions::HealthCheckers::TrpcProxy {

TrpcHealthChecker::TrpcHealthChecker(
    Server::Configuration::HealthCheckerFactoryContext& context,
    envoy::config::core::v3::HealthCheck const& config,
    envoy::config::health_checker::trpc_proxy::v3::Trpc&& trpc_config)
    : HealthCheckerImplBase(context.cluster(), config, context.dispatcher(), context.runtime(),
                            context.api().randomGenerator(), context.eventLogger()),
      config_(std::move(trpc_config)) {}

TrpcHealthChecker::TrpcActiveHealthCheckSession::~TrpcActiveHealthCheckSession() {
  ASSERT(client_ == nullptr);
}

bool TrpcHealthChecker::TrpcActiveHealthCheckSession::shouldClose() const {
  if (client_ == nullptr) {
    return false;
  }

  return !parent_.reuse_connection_;
}

void TrpcHealthChecker::TrpcActiveHealthCheckSession::onData(Buffer::Instance& data) {
  auto append_len = data.length();
  response_buffer_.move(data);
  ENVOY_CONN_LOG(trace, "append_len:{} total_len:{}", *client_, append_len,
                 response_buffer_.length());

  try {
    // 暂时不需要考虑粘包
    bool underflow = false;
    response_checker_->onData(response_buffer_, &underflow);
  } catch (EnvoyException const& ex) {
    ENVOY_CONN_LOG(error, "HealthCheck: response decode error: {}", *client_, ex.what());
    expect_close_ = false;
    client_->close(Network::ConnectionCloseType::NoFlush);
  }
}

void TrpcHealthChecker::TrpcActiveHealthCheckSession::onEvent(Network::ConnectionEvent event) {
  if (event == Network::ConnectionEvent::RemoteClose ||
      event == Network::ConnectionEvent::LocalClose) {
    if (!expect_close_) {
      handleFailure(envoy::data::core::v3::NETWORK);
      ENVOY_LOG(debug, "HealthChecker handleFailure");
    }
    parent_.dispatcher_.deferredDelete(std::move(client_));
  }

  // 只验证socket连接时，连接建立成功即认为是通过了健康检查。
  // 立即close socket，此时reuse_connection配置失效。
  // TODO(chabbyguo): health_checker parent_.config_.only_verify_connect() == true 才立马关闭链接
  // if (event == Network::ConnectionEvent::Connected && parent_.config_.only_verify_connect()) {
  if (event == Network::ConnectionEvent::Connected) {
    expect_close_ = true;
    client_->close(Network::ConnectionCloseType::NoFlush);
    handleSuccess(false);
    ENVOY_LOG(debug, "HealthChecker handleSuccess");
  }
}

void TrpcHealthChecker::TrpcActiveHealthCheckSession::onInterval() {
  if (!client_) {
    client_ =
        host_
            ->createHealthCheckConnection(parent_.dispatcher_, parent_.transportSocketOptions(),
                                          parent_.transportSocketMatchMetadata().get())
            .connection_;
    session_callbacks_ = std::make_shared<TrpcSessionCallbacks>(*this);
    client_->addConnectionCallbacks(*session_callbacks_);
    client_->addReadFilter(session_callbacks_);
    response_buffer_.drain(response_buffer_.length());
    response_checker_ = std::make_unique<NetworkFilters::TrpcProxy::CodecChecker>(*this);
    expect_close_ = false;
    client_->connect();
    client_->noDelay(true);
  }

  // TODO(chabbyguo): health_checker 在这里发包
  // if (!parent_.config_.only_verify_connect()) {
  //  sendHealthCheckRequest();
  // }
}

void TrpcHealthChecker::TrpcActiveHealthCheckSession::sendHealthCheckRequest() {
  // TODO(chabbyguo): health_checker 在这里组包
  NetworkFilters::TrpcProxy::TrpcRequestProtocol request_protocol;
  auto req_id = request_id();
  // protocol_header_
  auto& header = request_protocol.protocol_header_;
  header.set_request_id(req_id);
  // header.set_call_type(trpc::TrpcCallType::TRPC_UNARY_CALL); 特殊类型?
  header.set_content_type(trpc::TrpcContentEncodeType::TRPC_PROTO_ENCODE);
  header.set_timeout(1000);
  header.set_callee(parent_.config_.callee());
  header.set_caller(parent_.config_.caller());
  // header.set_func("/trpc.test.helloworld.Greeter/SayHello"); 特殊函数名?
  // encode
  Buffer::OwnedImpl data;
  request_protocol.encode(data);
  client_->write(data, false);
}

void TrpcHealthChecker::TrpcActiveHealthCheckSession::onTimeout() {
  expect_close_ = true;
  host_->setActiveHealthFailureType(Upstream::Host::ActiveHealthFailureType::TIMEOUT);
  client_->close(Network::ConnectionCloseType::NoFlush);
}

void TrpcHealthChecker::TrpcActiveHealthCheckSession::onDeferredDelete() {
  if (client_) {
    expect_close_ = true;
    client_->close(Network::ConnectionCloseType::NoFlush);
  }
}

void TrpcHealthChecker::TrpcActiveHealthCheckSession::onCompleted(
    std::unique_ptr<Buffer::OwnedImpl> msg) {
  if (pb_header_.func_ret() == trpc::TRPC_INVOKE_SUCCESS &&
      pb_header_.ret() == trpc::TRPC_INVOKE_SUCCESS) {
    handleSuccess(false);
    ENVOY_LOG(debug, "HealthChecker handleSuccess, msg.length:{}", msg->length());
  } else {
    host_->setActiveHealthFailureType(Upstream::Host::ActiveHealthFailureType::UNHEALTHY);
    handleFailure(envoy::data::core::v3::ACTIVE);
    ENVOY_LOG(debug, "HealthChecker handleFailure ret:{} fun_ret:{}", pb_header_.ret(),
              pb_header_.func_ret());
  }

  if (shouldClose()) {
    expect_close_ = true;
    client_->close(Network::ConnectionCloseType::NoFlush);
  }
}

}  // namespace Envoy::Extensions::HealthCheckers::TrpcProxy