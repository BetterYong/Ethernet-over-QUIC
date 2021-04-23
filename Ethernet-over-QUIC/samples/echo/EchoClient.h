/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

#pragma once

#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <glog/logging.h>

#include <folly/io/async/ScopedEventBaseThread.h>

#include <quic/api/QuicSocket.h>
#include <quic/client/QuicClientTransport.h>
#include <quic/common/test/TestUtils.h>

namespace quic
{
namespace samples
{
class EchoClient : public quic::QuicSocket::ConnectionCallback,
                   public quic::QuicSocket::ReadCallback,
                   public quic::QuicSocket::WriteCallback,
                   public quic::QuicSocket::DataExpiredCallback
{
public:
  EchoClient(const std::string &host, uint16_t port, bool prEnabled = false)
      : host_(host), port_(port), prEnabled_(prEnabled), networkThread_("EchoClientThread"), transportReady_(false)
  {
    start();
  }
  void onTransportReady() noexcept override
  {
    LOG(INFO) << "Transport Ready";
    transportReady_ = true;
  }

  void readAvailable(quic::StreamId streamId) noexcept override
  { //client read message [pingcoool]
    auto readData = quicClient_->read(streamId, 0);
    if (readData.hasError())
    {
      LOG(ERROR) << "EchoClient failed read from stream=" << streamId
                 << ", error=" << (uint32_t)readData.error();
    }
    auto copy = readData->first->clone();
    if (recvOffsets_.find(streamId) == recvOffsets_.end())
    {
      recvOffsets_[streamId] = copy->length();
    }
    else
    {
      recvOffsets_[streamId] += copy->length();
    }
    LOG(INFO) << "Client received data=" << copy->moveToFbString().toStdString()
              << " on stream=" << streamId;
  }

  void readError(
      quic::StreamId streamId,
      std::pair<quic::QuicErrorCode, folly::Optional<folly::StringPiece>>
          error) noexcept override
  {
    LOG(ERROR) << "EchoClient failed read from stream=" << streamId
               << ", error=" << toString(error);
    // A read error only terminates the ingress portion of the stream state.
    // Your application should probably terminate the egress portion via
    // resetStream
  }

  void onNewBidirectionalStream(quic::StreamId id) noexcept override
  {
    LOG(INFO) << "EchoClient: new bidirectional stream=" << id;
    quicClient_->setReadCallback(id, this);
  }

  void onNewUnidirectionalStream(quic::StreamId id) noexcept override
  {
    LOG(INFO) << "EchoClient: new unidirectional stream=" << id;
    quicClient_->setReadCallback(id, this);
  }

  void onStopSending(
      quic::StreamId id,
      quic::ApplicationErrorCode /*error*/) noexcept override
  {
    VLOG(10) << "EchoClient got StopSending stream id=" << id;
  }

  void onConnectionEnd() noexcept override
  {
    transportReady_ = false;
    LOG(INFO) << "EchoClient connection end";
  }

  void onConnectionError(
      std::pair<quic::QuicErrorCode, std::string> error) noexcept override
  {
    LOG(ERROR) << "EchoClient error: " << toString(error.first);
  }

  void onStreamWriteReady(
      quic::StreamId id,
      uint64_t maxToSend) noexcept override
  {
    LOG(INFO) << "EchoClient socket is write ready with maxToSend="
              << maxToSend;
    sendMessage(id, pendingOutput_[id]);
  }

  void onStreamWriteError(
      quic::StreamId id,
      std::pair<quic::QuicErrorCode, folly::Optional<folly::StringPiece>>
          error) noexcept override
  {
    LOG(ERROR) << "EchoClient write error with stream=" << id
               << " error=" << toString(error);
  }

  void onDataExpired(StreamId streamId, uint64_t newOffset) noexcept override
  {
    LOG(INFO) << "Client received skipData; "
              << newOffset - recvOffsets_[streamId]
              << " bytes skipped on stream=" << streamId;
  }

  void start()
  {
    //networkThread_ = new folly::ScopedEventBaseThread("EchoClientThread"); //该对象被释放后，连接就被释放了
    //auto evb = networkThread.getEventBase();

    evb_ = (networkThread_).getEventBase(); //换成非auto指针
    folly::SocketAddress addr(host_.c_str(), port_);

    (evb_)->runInEventBaseThreadAndWait([&] {
      auto sock = std::make_unique<folly::AsyncUDPSocket>(evb_);
      quicClient_ =
          std::make_shared<quic::QuicClientTransport>(evb_, std::move(sock)); //
      quicClient_->setHostname("echo.com");
      quicClient_->setCertificateVerifier(
          test::createTestCertificateVerifier());
      quicClient_->addNewPeerAddress(addr);
      LOG(INFO) << "before TransportSettings";
      TransportSettings settings;
      settings.idleTimeout=60000000ms;//pingcoool+//暂时保持短连接 TODO：改成长连接
      if (prEnabled_)
      {
        settings.partialReliabilityEnabled = true;
      }
      LOG(INFO) << "before setTransportSettings";

      quicClient_->setCongestionControllerFactory(std::make_shared<DefaultCongestionControllerFactory>());//zsy+
      settings.defaultCongestionController=CongestionControlType::BBR;//zsy+

      quicClient_->setTransportSettings(settings);

      LOG(INFO) << "EchoClient connecting to " << addr.describe();
      quicClient_->start(this);
    });
    //↑对quicClient进行初始化
  }

  void sendBytes(char *buffer, int len) //将char数组中，len个数据发送到服务端
  {
    //↓构造string对象：message进行传输
    std::string message(buffer, len);
    if (message.empty())
    {
      return;
    }
    auto client = quicClient_;
    while (!transportReady_)
    {
      usleep(100000);
      LOG(INFO) << "等待transportReady_";
    }
    evb_->runInEventBaseThreadAndWait([=] {
      // create new stream for each message
      auto streamId = client->createBidirectionalStream().value();
      client->setReadCallback(streamId, this);
      if (prEnabled_)
      {
        client->setDataExpiredCallback(streamId, this);
      }
      pendingOutput_[streamId].append(folly::IOBuf::copyBuffer(message));
      sendMessage(streamId, pendingOutput_[streamId]);
    });
    //LOG(INFO) << "EchoClient data sent!";
  }

  //~EchoClient() override = default;
  ~EchoClient()
  { //pingcoool
    LOG(INFO) << "~EchoClient()";
    //delete evb_;
  }

private:
  void sendMessage(quic::StreamId id, folly::IOBufQueue &data)
  {
    auto message = data.move();
    auto res = quicClient_->writeChain(id, message->clone(), true, false); //client send message[pingcoool]
    //
    if (res.value()){
      LOG(INFO) << "EchoClient socket did not accept all data, buffering】】】";
      assert(0);
    }
    //
    if (res.hasError())
    {
      LOG(ERROR) << "EchoClient writeChain error=" << uint32_t(res.error());
    }
    else if (res.value())
    {
      LOG(INFO) << "EchoClient socket did not accept all data, buffering len="
                << res.value()->computeChainDataLength();
      data.append(std::move(res.value()));
      quicClient_->notifyPendingWriteOnStream(id, this);
    }
    else
    {
      auto str = message->moveToFbString().toStdString();
      //LOG(INFO) << "EchoClient wrote \"" //<< str << "\""
                //<< ", len=" << str.size() << " on stream=" << id;
      // sent whole message
      pendingOutput_.erase(id);
    }
  }

  std::string host_;
  uint16_t port_;
  bool prEnabled_;
  std::shared_ptr<quic::QuicClientTransport> quicClient_;
  std::map<quic::StreamId, folly::IOBufQueue> pendingOutput_;
  std::map<quic::StreamId, uint64_t> recvOffsets_;
  folly::ScopedEventBaseThread networkThread_;
  folly::EventBase *evb_;
  volatile bool transportReady_;
};
} // namespace samples
} // namespace quic
