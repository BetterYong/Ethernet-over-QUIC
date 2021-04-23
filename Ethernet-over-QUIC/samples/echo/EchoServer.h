/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

#pragma once

#include <glog/logging.h>

#include <quic/common/test/TestUtils.h>
#include <quic/samples/echo/EchoHandler.h>
#include <quic/server/QuicServer.h>
#include <quic/server/QuicServerTransport.h>
#include <quic/server/QuicSharedUDPSocketFactory.h>

namespace quic
{
namespace samples
{

class EchoServerTransportFactory : public quic::QuicServerTransportFactory
{
public:
  ~EchoServerTransportFactory() override
  {
    while (!echoHandlers_.empty())
    {
      auto &handler = echoHandlers_.back();
      handler->getEventBase()->runImmediatelyOrRunInEventBaseThreadAndWait(
          [this] {
            // The evb should be performing a sequential consistency atomic
            // operation already, so we can bank on that to make sure the writes
            // propagate to all threads.
            echoHandlers_.pop_back();
          });
    }
  }

  EchoServerTransportFactory(bool prEnabled=false) 
        : prEnabled_(prEnabled) {}

  EchoServerTransportFactory(bool prEnabled,
      std::string** circleArray,
      pthread_cond_t* cond,
      pthread_mutex_t* mutex) 
        : prEnabled_(prEnabled),
          circleArray_(circleArray),
          cond_(cond),
          mutex_(mutex) {}

  quic::QuicServerTransport::Ptr make(
      folly::EventBase *evb,
      std::unique_ptr<folly::AsyncUDPSocket> sock,
      const folly::SocketAddress &,
      std::shared_ptr<const fizz::server::FizzServerContext>
          ctx) noexcept override
  {
    CHECK_EQ(evb, sock->getEventBase());
    auto echoHandler = std::make_unique<EchoHandler>(evb, prEnabled_,circleArray_,cond_,mutex_);
    auto transport = quic::QuicServerTransport::make(
        evb, std::move(sock), *echoHandler, ctx);
    echoHandler->setQuicSocket(transport);
    echoHandlers_.push_back(std::move(echoHandler));
    return transport;
  }

  std::vector<std::unique_ptr<EchoHandler>> echoHandlers_;

private:
  bool prEnabled_;
  std::string** circleArray_;
  pthread_cond_t* cond_;
  pthread_mutex_t* mutex_;
};

class EchoServer
{
public:
  explicit EchoServer(
      const std::string &host,
      uint16_t port,
      bool prEnabled,
      std::string** circleArray,
      pthread_cond_t* cond,
      pthread_mutex_t* mutex)
      : host_(host),
        port_(port),
        prEnabled_(prEnabled),
        server_(QuicServer::createQuicServer()),
        circleArray_(circleArray),
        cond_(cond),
        mutex_(mutex)
  {
    server_->setQuicServerTransportFactory(
        std::make_unique<EchoServerTransportFactory>(prEnabled_,circleArray_,cond_,mutex_));
    server_->setFizzContext(quic::test::createServerCtx());
    TransportSettings settings;
    settings.defaultCongestionController = CongestionControlType::BBR; //zsy+
    settings.idleTimeout = 60000000ms; //pingcoool+//暂时保持短连接 TODO：改成长连接
    if (prEnabled_)
    {
      settings.partialReliabilityEnabled = true;
    }
    server_->setTransportSettings(settings);
  }

  void start()
  {
    // Create a SocketAddress and the default or passed in host.
    folly::SocketAddress addr1(host_.c_str(), port_);
    addr1.setFromHostPort(host_, port_);
    server_->start(addr1, 0);
    LOG(INFO) << "Echo server started at: " << addr1.describe();
    eventbase_.loopForever();
  }

private:
  std::string host_;
  uint16_t port_;
  bool prEnabled_;
  folly::EventBase eventbase_;
  std::shared_ptr<quic::QuicServer> server_;
  std::string** circleArray_;
  pthread_cond_t* cond_;
  pthread_mutex_t* mutex_;
};
} // namespace samples
} // namespace quic
