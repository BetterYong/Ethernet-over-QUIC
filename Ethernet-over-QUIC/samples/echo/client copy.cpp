/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

#include <string.h>
#include <glog/logging.h>

#include <fizz/crypto/Utils.h>
#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>

#include <quic/samples/echo/EchoClient.h>
#include <quic/samples/echo/EchoServer.h>

DEFINE_string(host, "::1", "Echo server hostname/IP");
DEFINE_int32(port, 6666, "Echo server port");
DEFINE_string(mode, "server", "Mode to run in: 'client' or 'server'");
DEFINE_bool(pr, false, "Enable partially realible mode");

using namespace quic::samples;

int main(int argc, char *argv[])
{
#if FOLLY_HAVE_LIBGFLAGS
  // Enable glog logging to stderr by default.
  gflags::SetCommandLineOptionWithMode(
      "logtostderr", "1", gflags::SET_FLAGS_DEFAULT);
#endif
  gflags::ParseCommandLineFlags(&argc, &argv, false);
  folly::Init init(&argc, &argv);
  fizz::CryptoUtils::init();

  EchoClient client("127.0.0.1", 6666, false); //创建client对象，并向server端发起连接
  //client.start();
  std::string in;
  //client.send();
  char *chs = "pingcoool client send data test!";
  for (int i = 0; i < 10; i++)
  {
    client.sendBytes(chs, strlen(chs)); //向server端发送数据
  }

  LOG(INFO) << "send结束";

  while (std::getline(std::cin, in))
  {
  }

  return 0;
}
