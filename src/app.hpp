#ifndef OUCH_APP_HPP
#define OUCH_APP_HPP

#include "session.hpp"
#include "store.hpp"
#include "log.hpp"
#include "ouch.hpp"

#include <thread>

namespace OUCH {

class App : public noncopyable
{
public:
  App() : _threaded(true), _storeFactory(new StoreFactoryTmpl<AsyncFileStore>), _logFactory(new LogFactoryTmpl<AsyncFileLog>), _defaultSession(NULL) {}
  App(StoreFactory* storeFactory, LogFactory* logFactory) : _threaded(true), _storeFactory(storeFactory), _logFactory(logFactory), _defaultSession(NULL) {}
  virtual ~App();
  void setLogFactory(LogFactory* logFactory) { _logFactory = logFactory; }
  void init(cstr_t& settingsFile);
  void init(std::istream& stream);
  void init(const sessions_t& sessions);
  void connect();
  void startClients() { connect(); }
  void listen();
  void startServers() { listen(); }
  void setThreaded(bool v) { _threaded = v; }
  void wait();
  void stop(bool wait=true);
  bool isLoggedOn() { return _defaultSession->isLoggedOn(); }
  bool resendRequested() { return _defaultSession->resendRequested(); }

  virtual void onLogon(Session& session) {}
  virtual void onLogout(Session& session) {}
  virtual void onCreate(Session& session) {}
  virtual void fromApp(Message& msg, Session& session) {}

protected:
  sessions_t _sessions;
  sessions_t _activeSessions;
  bool _threaded;
  std::vector<epoll_t*> _polls;
  std::vector<std::thread*> _threads;
  StoreFactory* _storeFactory;
  LogFactory* _logFactory;
  static Log* _defaultLog;
  Session* _defaultSession;
  friend class Session;
};

} // namespace OUCH

#endif
