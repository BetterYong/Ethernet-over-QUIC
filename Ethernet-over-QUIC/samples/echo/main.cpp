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

int main(int argc, char* argv[]) {
#if FOLLY_HAVE_LIBGFLAGS
  // Enable glog logging to stderr by default.
  gflags::SetCommandLineOptionWithMode(
      "logtostderr", "1", gflags::SET_FLAGS_DEFAULT);
#endif
  gflags::ParseCommandLineFlags(&argc, &argv, false);
  folly::Init init(&argc, &argv);
  fizz::CryptoUtils::init();

  if (FLAGS_mode == "server") { //server 模式
    EchoServer server(FLAGS_host, FLAGS_port, FLAGS_pr);
    server.start();//在host主机的port端口上监听
  } else if (FLAGS_mode == "client") {  //client 模式
    if (FLAGS_host.empty() || FLAGS_port == 0) {
      LOG(ERROR) << "EchoClient expected --host and --port";
      return -2;
    }
    EchoClient client(FLAGS_host, FLAGS_port, FLAGS_pr);//创建client对象，并向server端发起连接
    client.send();
    // char* chs="pingcoool client send data test!";
    // client.sendBytes(chs,strlen(chs));//向server端发送数据

    std::string in;
    while (std::getline(std::cin, in))
    {
    }
    
  } else {
    LOG(ERROR) << "Unknown mode specified: " << FLAGS_mode;
    return -1;
  }
  return 0;
}

