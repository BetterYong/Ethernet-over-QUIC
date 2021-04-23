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

void *thrd_func(void *arg)
{ //*(int*)arg
  EchoServer server("127.0.0.1", 6666, false);
  server.start();
  return NULL;
}

int main()
{
  pthread_t tid;
  int ret = pthread_create(&tid, NULL, thrd_func, NULL);
  if (ret != 0)
  {
    fprintf(stderr, "pthread_create error: %s\n", strerror(ret)); //自行解析错误返回值
    exit(1);                                                      //注：这里就不能用perror来输出错误信息了，因为pthread_create错误返回后不设置errno，perror函数无法获知错误信息，只能通过解析ret返回值自行输出错误原因。
  }
  LOG(INFO) << "client";
  std::string in;
  std::getline(std::cin, in);

  EchoClient client("127.0.0.1", 6666, false); //创建client对象，并向server端发起连接
  char *chs = "abcdefgh";
  for (long long i = 0; i < 1; i++)
  {
    client.sendBytes(chs, strlen(chs)); //向server端发送数据
  }
  std::getline(std::cin, in);
  for (long long i = 0; i < 50000; i++)
  {
    client.sendBytes(chs, strlen(chs)); //向server端发送数据
  }

  LOG(INFO) << "send结束";
  std::getline(std::cin, in);
  return 0;
}
