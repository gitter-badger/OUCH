#include "app.hpp"

#include <sstream>
#include <signal.h>

using namespace OUCH;

struct timespec tm;
long total = 0;
long n = 0;
long max = 0;
long min = 9999999999999999L;

bool active = true;
void sigroutine(int signo) 
{
  active = false;
  std::cout << time(NULL) << ": catched signal " << signo << std::endl;
  std::cout << time(NULL) << ": pending to exit" << std::endl;
}

std::string longText(1024*1024*16, 'X');

struct MyApp : public App
{
  //MyApp() : App(new StoreFactoryTmpl<FileStore>, new LogFactoryTmpl<FileLog>) {}
  void fromApp(Message& msg, Session& session)
  {
    if (msg.type == OrderMsg::TYPE) {
      auto& omsg = (OrderMsg&)msg;
      std::cout << "-- fromApp --\n";
      if (session.isClient()) {
        //auto f = msg.get(PossDupFlag);
        //if (f && *f == 'Y') return;
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long diff = (now.tv_sec - tm.tv_sec) * 1000000000 + (now.tv_nsec - tm.tv_nsec);
        n++;
        total += diff;
        if (diff > max) max= diff;
        if (diff < min) min = diff;
        if (n % 10000 == 0) {
          std::cerr << "Messages sent: " << n << "\n";
          std::cerr << "Round-trip time: min/avg/max = " << (min/1000.) << '/' << (total/n/1000.) << '/' << (max/1000.) << "us\n";
        }
        newOrder(session);
      } else {
        std::cout << "-- Ack --\n";
        session.send(AcceptedMsg(omsg));
      }
    } else if (msg.type == CancelMsg::TYPE) {
      session.send(CanceledMsg((CancelMsg&)msg));
    }
  }

  void onLogon(Session& session)
  {
    std::cout << "-- onLogon --\n";
    if (session.isClient()) newOrder(session);
  }

  void newOrder(Session& session)
  {
    clock_gettime(CLOCK_MONOTONIC, &tm);
    session.send(OrderMsg("12345", 'B', 100, "MSFT", 12.34 * 10000));
  }
};

int main(int argc, char** argv)
{
  if (argc < 2 || (strcasecmp(argv[1], "client") && strcasecmp(argv[1], "server"))) {
    std::cerr << "Usage: ./test <client|server> [port=9123]" << std::endl;
    return -1;
  }

  signal(SIGINT, sigroutine);  // for kill command
  bool isClient = !strcasecmp(argv[1], "client");
  int port = 9123;
  if (argc > 2) port = atoi(argv[2]);

  std::stringstream str;
  str << 
    "[DEFAULT]\n"
    "SocketConnectHost=localhost\n"
    "SocketConnectPort=" << port << "\n"
    "SocketAcceptPort=" << port << "\n"
    "FileStorePath=out/test_store\n"
    "FileLogPath=out/test_log\n"
    "[SESSION]\n"
    "Username=zhb\n"
    "Password=xxx\n"
    "ConnectionType=acceptor\n"
    "[SESSION]\n"
    "Username=zhb2\n"
    "Password=xxx\n"
    "ConnectionType=acceptor\n"
    "[SESSION]\n"
    "Username=zhb\n"
    "Password=xxx\n"
    "ConnectionType=initiator\n"
    "[SESSION]\n"
    "Username=zhb2\n"
    "Password=xxx\n"
    "ConnectionType=initiator\n"
    ;
  MyApp app;
  app.init(str);
  if (isClient) app.connect();
  else app.listen();
  while (active) { usleep(1000); }
  app.stop(false);
  return 0;
}
