/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */
#include <net/if.h>
#include <glog/logging.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <linux/if_tun.h>
#include <stdlib.h>
#include <stdio.h>
#include <fizz/crypto/Utils.h>
#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>
#include <unistd.h>
#include <quic/samples/echo/EchoClient.h>
#include <quic/samples/echo/EchoServer.h>
#include <quic/samples/echo/config_file_parser.h>

DEFINE_string(host, "::1", "Echo server hostname/IP");
DEFINE_int32(port, 6666, "Echo server port");
DEFINE_string(mode, "server", "Mode to run in: 'client' or 'server'");
DEFINE_bool(pr, false, "Enable partially realible mode");

//全局变量
int tun_fd;
std::string *circleArray[65536] = {NULL};
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

using namespace quic::samples;

typedef struct Param
{
  const char *ip;
  int port;
  bool pr;
} info;

void *serverThread(void *arg)
{ //*(int*)arg
  info *d = (info *)arg;
  const char *ip = d->ip;
  int port = d->port;
  bool pr = d->pr;
  EchoServer server(ip, port, pr, circleArray, &cond, &mutex);
  server.start();
  return NULL;
}

void *writeTunThread(void *arg) //从circleArray中读取数据，然后写入tun设备中
{
  LOG(INFO) << "写tun线程";
  char buffer[2000];
  //读circleArray线程
  int32_t index = 0; //1、约定从0开始
  pthread_mutex_lock(&mutex);
  LOG(INFO) << "获得锁";
  while (1)
  {
    while (circleArray[index] == NULL) //2、空则阻塞等待
    {
      //LOG(INFO) << "写tun线程等待条件变量";
      pthread_cond_wait(&cond, &mutex); //即便等到了条件变量也有可能在获得锁之后发现circleArray[index]==NULL，等到了条件变量之后仍需要判断circleArray[index]是否为空，所以只能用while不能用if
      //LOG(INFO) << "写tun线程被唤醒";
    }
    //LOG(INFO) << "server 向tun中写入：[" << index << "] : "; //<< *circleArray[index]; //3、从circleArray中读取数据
    (*circleArray[index]).copy(buffer, (*circleArray[index]).length(), 0);
    int ret = write(tun_fd, buffer, (*circleArray[index]).length()); //4、向tun中写数据
    if (ret == -1)
    {
      LOG(INFO) << "write tun error!!!!";
    }
    //LOG(INFO) << "has write " << ret << "bytes to tun";
    delete circleArray[index];    //5、释放string空间
    circleArray[index] = NULL;    //6、指针置空
    index = (index + 1) & 0xffff; //7、环形
  }
  pthread_mutex_unlock(&mutex);
  return NULL;
}

void createServerThread(const char *ip, int port, bool pr)
{
  pthread_t tid;
  info *p = (info *)malloc(sizeof(info));
  p->ip = ip;
  p->port = port;
  p->pr = pr;
  int ret = pthread_create(&tid, NULL, serverThread, p);
  if (ret != 0)
  {
    fprintf(stderr, "pthread_create error: %s\n", strerror(ret)); //自行解析错误返回值
    exit(1);                                                      //注：这里就不能用perror来输出错误信息了，因为pthread_create错误返回后不设置errno，perror函数无法获知错误信息，只能通过解析ret返回值自行输出错误原因。
  }
}

void createWriteTunThread()
{
  pthread_t tid;
  int ret = pthread_create(&tid, NULL, writeTunThread, NULL);
  if (ret != 0)
  {
    fprintf(stderr, "pthread_create error: %s\n", strerror(ret)); //自行解析错误返回值
    exit(1);                                                      //注：这里就不能用perror来输出错误信息了，因为pthread_create错误返回后不设置errno，perror函数无法获知错误信息，只能通过解析ret返回值自行输出错误原因。
  }
}

//int tun_alloc(int flags)
//设置函数重载，多径
int tun_alloc(int flags,std::string ip1,std::string ip2,std::string source_host,std::string dest_host)
{
  char cmd[100];
  sprintf(cmd, "sudo echo \"已获得root权限\"");
  system(cmd);
  struct ifreq ifr;
  int fd, err;
  char *clonedev =(char *) "/dev/net/tun";

  if ((fd = open(clonedev, O_RDWR)) < 0)
  {
    return fd;
  }

  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = flags;

  if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0)
  {
    LOG(INFO) << "ioctl error，permission denied.";
    close(fd);
    return err;
  }
  //启用tun设备
  sprintf(cmd, "sudo ip link set dev %s up", ifr.ifr_name);
  system(cmd);

  //配置路由
  ip1.replace(ip1.size()-1,1,"0");
  ip2.replace(ip2.size()-1,1,"0");
  source_host.replace(source_host.size()-1,1,"0");
  //配置路由表
  sprintf(cmd, "sudo route add -net %s/24 dev %s",source_host.c_str(), ifr.ifr_name);
  system(cmd);
/**
  sprintf(cmd, "sudo route add -net 10.0.1.0/24 dev r2-eth0");
  system(cmd);
**/
// 搭建网桥
//	brctl addbr br_test
//        brctl addif br_test eth0
 //       brctl addif br_test tap0  
  sprintf(cmd, "brctl addbr br_test");
  system(cmd);
  sprintf(cmd, "brctl addif br_test r2-eth0");
  system(cmd);
  sprintf(cmd, "brctl addif br_test %s",ifr.ifr_name);
  system(cmd);
  sprintf(cmd, "ifconfig br_test up");
  system(cmd);

  sprintf(cmd, "sudo route add -net %s/24 dev r2-eth1",ip1.c_str());
  system(cmd);
  sprintf(cmd, "sudo route add -net %s/24 dev r2-eth2",ip2.c_str());
  system(cmd);

  //重要！！！将tun设备的txqueuelen调 大！！！
  sprintf(cmd, "sudo ifconfig %s txqueuelen 50000", ifr.ifr_name);
  system(cmd);

  LOG(INFO) << "    创建 " << ifr.ifr_name << " 设备并启用";
  return fd;
}
//单径重载
int tun_alloc(int flags,std::string ip1,std::string source_host,std::string dest_host)
{
  char cmd[100];
  sprintf(cmd, "sudo echo \"已获得root权限\"");
  system(cmd);
  struct ifreq ifr;
  int fd, err;
  char *clonedev =(char *) "/dev/net/tun";

  if ((fd = open(clonedev, O_RDWR)) < 0)
  {
    return fd;
  }

  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = flags;

  if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0)
  {
    LOG(INFO) << "ioctl error，permission denied.";
    close(fd);
    return err;
  }
  //启用tun设备
  sprintf(cmd, "sudo ip link set dev %s up", ifr.ifr_name);
  system(cmd);

  //配置路由
  ip1.replace(ip1.size()-1,1,"0");
  //ip2.replace(ip2.size()-1,1,"0");
  source_host.replace(source_host.size()-1,1,"0");
  //配置路由表
  sprintf(cmd, "sudo route add -net %s/24 dev %s",source_host.c_str(), ifr.ifr_name);
  system(cmd);
/**
  sprintf(cmd, "sudo route add -net 10.0.1.0/24 dev r2-eth0");
  system(cmd);
**/
// 搭建网桥
//	brctl addbr br_test
//        brctl addif br_test eth0
 //       brctl addif br_test tap0  
  sprintf(cmd, "brctl addbr br_test");
  system(cmd);
  sprintf(cmd, "brctl addif br_test r2-eth0");
  system(cmd);
  sprintf(cmd, "brctl addif br_test %s",ifr.ifr_name);
  system(cmd);
  sprintf(cmd, "ifconfig br_test up");
  system(cmd);

  sprintf(cmd, "sudo route add -net %s/24 dev r2-eth1",ip1.c_str());
  system(cmd);
  //sprintf(cmd, "sudo route add -net %s/24 dev r2-eth2",ip2.c_str());
  //system(cmd);

  //重要！！！将tun设备的txqueuelen调 大！！！
  sprintf(cmd, "sudo ifconfig %s txqueuelen 50000", ifr.ifr_name);
  system(cmd);

  LOG(INFO) << "    创建 " << ifr.ifr_name << " 设备并启用";
  return fd;
}

int main()
{
  //读取配置文件
  ConfigFileParser cfg("quic_transport.cfg");
  std::string client_host1 = cfg.getvalue<std::string>("client_host1");
  int client_port1 = cfg.getvalue<int>("client_port1");
  std::string client_host2 = cfg.getvalue<std::string>("client_host2");
  int client_port2 = cfg.getvalue<int>("client_port2");

  std::string server_host1 = cfg.getvalue<std::string>("server_host1");
  int server_port1 = cfg.getvalue<int>("server_port1");
  std::string server_host2 = cfg.getvalue<std::string>("server_host2");
  int server_port2 = cfg.getvalue<int>("server_port2");
  std::string source_host = cfg.getvalue<std::string>("source_host");
  std::string dest_host = cfg.getvalue<std::string>("dest_host");
  //单径与多径的选择
  //multipath or not　　１ 多径　　０　单径
  int Multipath = cfg.getvalue<int>("is_multipath");
  std::cout<<"isMultipath is "<<Multipath<<std::endl;

  if(Multipath){ //一、创建tun设备
  LOG(INFO) << "多径模式";
  LOG(INFO) << "一、创建tap设备";
  //tun_fd = tun_alloc(IFF_TAP | IFF_NO_PI);  
  //LOG(INFO) << "一、创建tun设备";
  tun_fd = tun_alloc(IFF_TAP | IFF_NO_PI, server_host1, server_host2, source_host, dest_host);
  //二、创建向tun设备写数据的线程
  LOG(INFO) << "二、创建向tun设备写数据的线程";
  createWriteTunThread();
  //**********************************************************************************
  //创建一个TCP连接  等待Client端连接消息
  //此时就不需要手动输入调度比例 直接等待客户端建立连接即可运行
    struct sockaddr_in     serv_addr;
    struct sockaddr_in     clie_addr;
    int                    sock_id;
    int                    link_id;
    socklen_t                    clie_addr_len;


    if ((sock_id = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Create socket failed\n");
        exit(0);
    }
    /*fill the server sockaddr_in struct*/
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port1);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock_id, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Bind socket failed\n");
        exit(0);
    }

    if (-1 == listen(sock_id, 10)) {
        perror("Listen socket failed\n");
        exit(0);
    }

    clie_addr_len = sizeof(clie_addr);
    link_id = accept(sock_id, (struct sockaddr*)&clie_addr, &clie_addr_len);
    while (-1 == link_id) {
        perror("Accept socket failed\n");
        //	exit(0);
    }
    std::cout << "Accept success!!" << std::endl;


    close(link_id);

    //}
    close(sock_id);

    //***************************************************************************************

  //三、创建server 1,2
  //**********             server              *************//
  //1
  LOG(INFO) << "三、创建server 1,2";
  //createServerThread("11.0.0.2", 6666, false);
  createServerThread(server_host2.c_str(), server_port2, false);
  
  //2
  //createServerThread("12.0.0.2", 7777, false);
  createServerThread(server_host1.c_str(), server_port1, false);
  //四、创建client 1,2
  //**********             client              *************//
  LOG(INFO) << "创建client 1,2";
  LOG(INFO) << "输入字符以开启client";
  LOG(INFO) << "输入调度比例：（0~20）";
/**
  std::string in;
  std::getline(std::cin, in);
  int rate = stoi(in, 0, 10);
*/
//调度比例  目前设置为10
  int rate = 10;
  usleep(1000000);//1s

  //EchoClient client1("11.0.0.1", 6666, false); //创建client对象，并向server端发起连接
  //EchoClient client2("12.0.0.1", 7777, false); //创建client对象，并向server端发起连接
  EchoClient client1(client_host1.c_str(), client_port1, false); //创建client对象，并向server端发起连接
  EchoClient client2(client_host2.c_str(), client_port2, false); //创建client对象，并向server端发起连接

  //五、轮询调度从tun中读出的数据
  LOG(INFO) << "五、轮询调度从tun中读出的数据";
  uint16_t index = 0;
  char buffer[2002];
  long ret;
  int which = 0;
  while (1)
  {
    ((uint16_t *)buffer)[0] = index; //1、写头，头中的两字节是从tun中读出的顺序
    //LOG(INFO)<<"server 等待tun中的数据["<<index<<"]";
    ret = read(tun_fd, (buffer + 2), sizeof(buffer) - 2); //2、从tun中读出数据
                                                          //LOG(INFO)<<"server 从tun中读出"<<ret<<"bytes数据，并构造["<<index<<"]";

    //client1.sendBytes(buffer, ret + 2);

    if (which < rate) //3、轮询写给client1,2
    {
      client1.sendBytes(buffer, ret + 2);
      which++;
    }
    else
    {
      client2.sendBytes(buffer, ret + 2);
      which++;
    }
    which = (which % 20);
    //which=0;
    index = (index + 1) & 0xffff;
  }
  
  }else{
      //单径
       //一、创建tun设备
  LOG(INFO) << "单径模式";
  LOG(INFO) << "一、创建tap设备";
  //tun_fd = tun_alloc(IFF_TAP | IFF_NO_PI);  
  //LOG(INFO) << "一、创建tun设备";
  tun_fd = tun_alloc(IFF_TAP | IFF_NO_PI, server_host1,source_host, dest_host);
  //二、创建向tun设备写数据的线程
  LOG(INFO) << "二、创建向tun设备写数据的线程";
  createWriteTunThread();
  //**********************************************************************************
  //创建一个TCP连接  等待Client端连接消息
  //此时就不需要手动输入调度比例 直接等待客户端建立连接即可运行
    struct sockaddr_in     serv_addr;
    struct sockaddr_in     clie_addr;
    int                    sock_id;
    int                    link_id;
    socklen_t                    clie_addr_len;


    if ((sock_id = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Create socket failed\n");
        exit(0);
    }
    /*fill the server sockaddr_in struct*/
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port1);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock_id, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Bind socket failed\n");
        exit(0);
    }

    if (-1 == listen(sock_id, 10)) {
        perror("Listen socket failed\n");
        exit(0);
    }

    clie_addr_len = sizeof(clie_addr);
    link_id = accept(sock_id, (struct sockaddr*)&clie_addr, &clie_addr_len);
    while (-1 == link_id) {
        perror("Accept socket failed\n");
        //	exit(0);
    }
    std::cout << "Accept success!!" << std::endl;


    close(link_id);

    //}
    close(sock_id);

    //***************************************************************************************

  //三、创建server 1,2
  //**********             server              *************//
  //1
  LOG(INFO) << "三、创建server 1,2";
  //createServerThread("11.0.0.2", 6666, false);
  //createServerThread(server_host2.c_str(), server_port2, false);
  
  //2
  //createServerThread("12.0.0.2", 7777, false);
  createServerThread(server_host1.c_str(), server_port1, false);
  //四、创建client 1,2
  //**********             client              *************//
  //LOG(INFO) << "创建client 1,2";
  //LOG(INFO) << "输入字符以开启client";
  //LOG(INFO) << "输入调度比例：（0~20）";
/**
  std::string in;
  std::getline(std::cin, in);
  int rate = stoi(in, 0, 10);
*/
//调度比例  目前设置为10
  //int rate = 10;
  usleep(1000000);//1s

  //EchoClient client1("11.0.0.1", 6666, false); //创建client对象，并向server端发起连接
  //EchoClient client2("12.0.0.1", 7777, false); //创建client对象，并向server端发起连接
  EchoClient client1(client_host1.c_str(), client_port1, false); //创建client对象，并向server端发起连接
 // EchoClient client2(client_host2.c_str(), client_port2, false); //创建client对象，并向server端发起连接

  //五、轮询调度从tun中读出的数据
  LOG(INFO) << "五、轮询调度从tun中读出的数据";
  uint16_t index = 0;
  char buffer[2002];
  long ret;
  while (1)
  {
    ((uint16_t *)buffer)[0] = index; //1、写头，头中的两字节是从tun中读出的顺序
    //LOG(INFO)<<"server 等待tun中的数据["<<index<<"]";
    ret = read(tun_fd, (buffer + 2), sizeof(buffer) - 2); //2、从tun中读出数据
                                                          //LOG(INFO)<<"server 从tun中读出"<<ret<<"bytes数据，并构造["<<index<<"]"
    client1.sendBytes(buffer, ret + 2);
    index = (index + 1) & 0xffff;
  }
}
  return 0;
}
