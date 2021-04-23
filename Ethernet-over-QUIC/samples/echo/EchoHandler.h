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
#include <quic/api/QuicSocket.h>
#include <pthread.h>
#include <folly/io/IOBufQueue.h>
#include <folly/io/async/EventBase.h>
#include <unistd.h>
#include <stdio.h>

namespace quic
{
namespace samples
{
class EchoHandler : public quic::QuicSocket::ConnectionCallback,
                    public quic::QuicSocket::ReadCallback,
                    public quic::QuicSocket::WriteCallback
{
public:
  using StreamData = std::pair<folly::IOBufQueue, bool>;

  explicit EchoHandler(folly::EventBase *evbIn, bool prEnabled = false)
      : evb(evbIn), prEnabled_(prEnabled), count_(0) {}

  explicit EchoHandler(folly::EventBase *evbIn, bool prEnabled, std::string **circleArray, pthread_cond_t* cond, pthread_mutex_t* mutex)
      : evb(evbIn), prEnabled_(prEnabled), count_(0), circleArray_(circleArray), cond_(cond), mutex_(mutex) {}

  void setQuicSocket(std::shared_ptr<quic::QuicSocket> socket)
  {
    sock = socket;
  }

  void onNewBidirectionalStream(quic::StreamId id) noexcept override
  {
    //LOG(INFO) << "Got bidirectional stream id=" << id;
    sock->setReadCallback(id, this);
  }

  void onNewUnidirectionalStream(quic::StreamId id) noexcept override
  {
    LOG(INFO) << "Got unidirectional stream id=" << id;
    sock->setReadCallback(id, this);
  }

  void onStopSending(
      quic::StreamId id,
      quic::ApplicationErrorCode error) noexcept override
  {
    LOG(INFO) << "Got StopSending stream id=" << id << " error=" << error;
  }

  void onConnectionEnd() noexcept override
  {
    LOG(INFO) << "Socket closed";
  }

  void onConnectionError(
      std::pair<quic::QuicErrorCode, std::string> error) noexcept override
  {
    LOG(ERROR) << "Socket error=" << toString(error.first);
  }

  void readAvailable(quic::StreamId id) noexcept override//
  { //server read message [pingcoool]
    //LOG(INFO) << "read available for stream id=" << id << " count= " << count_;
    count_++;
    auto res = sock->read(id, 0);
    if (res.hasError())
    {
      LOG(ERROR) << "Got error=" << toString(res.error());
      return;
    }
    if (input_.find(id) == input_.end())
    { //?
      input_.emplace(
          id,
          std::make_pair(
              folly::IOBufQueue(folly::IOBufQueue::cacheChainLength()), false));
    }
    quic::Buf data = std::move(res.value().first);
    bool eof = res.value().second;
    //auto dataLen = (data ? data->computeChainDataLength() : 0);
    //LOG(INFO) << "server got len=" << dataLen << " eof=" << uint32_t(eof)
              //<< " total=" << input_[id].first.chainLength() + dataLen;
              //<< " data=" << data->clone()->moveToFbString().toStdString();
    input_[id].first.append(std::move(data));
    input_[id].second = eof;
    if (eof)
    {
      if (prEnabled_)
      { //Enable partially realible mode
        skipSomeEchoSome(id, input_[id]);
      }
      else
      {
        //echo(id, input_[id]);
        auto message = input_[id].first.move();
        input_.erase(id); //释放input_[id]
        auto str = message->moveToFbString().toStdString();
        //LOG(INFO) << "在这里获得str::string"; //<< str; //在这里获得str::string

        /************************************
        for(unsigned int i=0;i<str.length();i++){
          printf("%3x",(unsigned char)(str.at(i)));
        }
        printf("\n");
        /************************************/
        //可以在这里解析包头以及校验包的合法性
        if (str.length() > 2)   //str必须大于2
        {
          std::string *tmp = new std::string(str.substr(2)); //1、在堆中创建对象并赋值
          char head[2];
          head[0] = str.at(0);
          head[1] = str.at(1);
          int32_t index = (int32_t)((uint16_t *)head)[0];
          //LOG(INFO) << "server received ["<<index<<"]";
          while (circleArray_[index] != NULL)
          { //2、非空则自旋等待
            LOG(INFO) << "缓冲区满！！！唤醒读线程，写线程自旋等待";
            if (!pthread_mutex_trylock(mutex_)) //3、唤醒读线程
            {
              pthread_mutex_unlock(mutex_);
              pthread_cond_signal(cond_); //将阻塞等待条件变量的读线程唤醒
            }
            usleep(100000); //0.1秒
          }
          circleArray_[index] = tmp;        //4、修改指针指向
          int ret=pthread_mutex_trylock(mutex_);
          //LOG(INFO) << "唤醒读线程了吗？ "<<ret;
          if (!ret) //5、唤醒读线程!!!没起作用？？？
          {
            //LOG(INFO) << "唤醒写tap线程";
            pthread_mutex_unlock(mutex_);
            pthread_cond_signal(cond_); //将阻塞等待条件变量的读线程唤醒
          }
        }
      }
    }
  }

  void readError(
      quic::StreamId id,
      std::pair<quic::QuicErrorCode, folly::Optional<folly::StringPiece>>
          error) noexcept override
  {
    LOG(ERROR) << "Got read error on stream=" << id
               << " error=" << toString(error) << "count=" << count_;
    // A read error only terminates the ingress portion of the stream state.
    // Your application should probably terminate the egress portion via
    // resetStream
  }

  void echo(quic::StreamId id, StreamData &data)
  {
    if (!data.second)
    {
      // only echo when eof is present
      return;
    }
    auto echoedData = folly::IOBuf::copyBuffer("echo ");
    echoedData->prependChain(data.first.move());
    auto res =
        sock->writeChain(id, std::move(echoedData), true, false, nullptr); //server send message [pingcoool]
    if (res.hasError())
    {
      LOG(ERROR) << "write error=" << toString(res.error());
    }
    else if (res.value())
    {
      LOG(INFO) << "socket did not accept all data, buffering len="
                << res.value()->computeChainDataLength();
      data.first.append(std::move(res.value()));
      sock->notifyPendingWriteOnStream(id, this);
    }
    else
    {
      // echo is done, clear EOF
      data.second = false;
    }
  }

  /**
   * Skips first part, echoes the second part.
   * E.g. "batman" -> "???man"
   */
  void skipSomeEchoSome(quic::StreamId id, StreamData &data)
  {
    if (!data.second)
    {
      // only echo when eof is present
      return;
    }
    auto &originalData = data.first;
    auto dataLen = originalData.chainLength();

    auto toSplit = dataLen / 2;

    if (toSplit > 0)
    {
      auto skipRes = sock->sendDataExpired(id, toSplit);
      if (skipRes.hasError())
      {
        LOG(ERROR) << "skip error=" << toString(skipRes.error());
      }
      else
      {
        auto v = skipRes.value();
        if (v.hasValue())
        {
          LOG(INFO) << "new offset = " << v.value();
        }
        else
        {
          LOG(INFO) << "new offset doesn't have value";
        }
      }
    }

    originalData.split(toSplit);

    auto res = sock->writeChain(id, originalData.move(), true, false, nullptr);
    if (res.hasError())
    {
      LOG(ERROR) << "write error=" << toString(res.error());
    }
    else if (res.value())
    {
      LOG(INFO) << "socket did not accept all data, buffering len="
                << res.value()->computeChainDataLength();
      originalData.append(std::move(res.value()));
      sock->notifyPendingWriteOnStream(id, this);
    }
    else
    {
      // echo is done, clear EOF
      data.second = false;
    }
  }

  void onStreamWriteReady(
      quic::StreamId id,
      uint64_t maxToSend) noexcept override
  {
    LOG(INFO) << "socket is write ready with maxToSend=" << maxToSend;
    echo(id, input_[id]);
  }

  void onStreamWriteError(
      quic::StreamId id,
      std::pair<quic::QuicErrorCode, folly::Optional<folly::StringPiece>>
          error) noexcept override
  {
    LOG(ERROR) << "write error with stream=" << id
               << " error=" << toString(error);
  }

  folly::EventBase *getEventBase()
  {
    return evb;
  }

  folly::EventBase *evb;
  std::shared_ptr<quic::QuicSocket> sock;

private:
  std::map<quic::StreamId, StreamData> input_;
  bool prEnabled_;
  int count_;
  std::string **circleArray_;
  pthread_cond_t* cond_;
  pthread_mutex_t* mutex_;
};
} // namespace samples
} // namespace quic
