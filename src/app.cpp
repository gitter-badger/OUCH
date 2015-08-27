#include "app.hpp"

#include <signal.h>

using namespace OUCH;
  
Log* App::_defaultLog;

static std::map<int, sessions_t> _sharedSessions; // server sessions share the same acceptor socket fd
static std::map<int, int> _port2fd; // acceptor port to acceptor socket fd
static std::mutex _m;
typedef std::lock_guard<std::mutex> lock_t;

namespace OUCH {
struct Acceptor : public i_poll_events
{
  void in_event(int);
};
}

static Acceptor _acceptor;

void App::init(cstr_t& settingsFile)
{ _sessions = Session::createSessions(settingsFile); }

void App::init(std::istream& stream)
{ _sessions = Session::createSessions(stream); }

void App::init(const sessions_t& sessions)
{ _sessions = sessions; }

static void avoidSIGPIP()
{
  static bool set;
  if (set) return;
  set = true;
  // the program exits due to SIGPIPE signal if you write the fd when fd already closed.
  // you can avoid SIGPIPE by use send with MSG_NOSIGNAL option or just signal(SIGPIPE,
  signal(SIGPIPE, SIG_IGN);
}

void App::connect()
{
  avoidSIGPIP();
  if (!_threaded && _polls.empty()) _polls.push_back(new epoll_t);
  int n = 0;
  for (auto it = _sessions.begin(); it != _sessions.end(); ++it) {
    auto s = *it;
    if (!s->isClient()) continue;
    if (s->_app) die("can not assign session '" + s->_id + "' to App twice");
    n++;
    s->_app = this;
    onCreate(*s);
    _activeSessions.push_back(s);
    if (!_defaultSession) _defaultSession = s;
    s->_store = _storeFactory->create(*s);
    s->_log = _logFactory->create(*s);
    epoll_t *poll, *poll2;
    if (_threaded) {
      poll = new epoll_t;
      _polls.push_back(poll);
      poll2 = poll; // in my test, sharing poll faster
      //poll2 = new epoll_t;
      //_polls.push_back(poll2);
    } else
      poll2 = poll = _polls.front();
    s->_poll = poll;
    s->_outpoll = poll2;
    poll->set_pollin(poll->add_fd(s->_tfd, s->_timer));
    s->event(""); // new line
    s->event("Created session");
    s->connect(true);
  }

  if (n == 0) die("no FIX clients found in the settings file");
  
  for (auto i = _threads.size(); i < _polls.size(); ++i) 
    _threads.push_back(new std::thread([=](){_polls[i]->loop();}));
}

void App::listen()
{
  avoidSIGPIP();
  if (!_threaded && _polls.empty()) _polls.push_back(new epoll_t);
  int n = 0;
  for (auto it0 = _sessions.begin(); it0 != _sessions.end(); ++it0) {
    auto s = *it0;
    if (s->isClient()) continue;
    if (s->_app) die("can not assign session '" + s->_id + "' to App twice");
    n++;
    s->_app = this;
    onCreate(*s);
    _activeSessions.push_back(s);
    if (!_defaultSession) _defaultSession = s;
    s->_store = _storeFactory->create(*s);
    s->_log = _logFactory->create(*s);
    if (!_defaultLog) _defaultLog = _logFactory->create();
    auto port = s->get("SocketAcceptPort", 0);
    int fd;
    auto it = _port2fd.find(port);
    epoll_t *poll, *poll2;
    if (_threaded) {
      poll = new epoll_t;
      _polls.push_back(poll);
      poll2 = poll;
      //poll2 = new epoll_t;
      //_polls.push_back(poll2);
    } else
      poll2 = poll = _polls.front();
    if (it == _port2fd.end()) {
      fd = createAcceptor(port);
      auto rsize = s->get("ReceiveBufferSize", 0);
      auto ssize = s->get("SendBufferSize", 0);
      if (rsize > 0) setSockOpt(fd, SO_RCVBUF, rsize);
      if (ssize > 0) setSockOpt(fd, SO_SNDBUF, ssize);
      _port2fd[port] = fd;
      poll->set_pollin(poll->add_fd(fd, &_acceptor));
    } else
      fd = it->second;
    s->_poll = poll;
    s->_outpoll = poll2;
    poll->set_pollin(poll->add_fd(s->_tfd, s->_timer));
    {
      lock_t lock(_m);
      _sharedSessions[fd].push_back(s);
    }
    s->event("Created session");
    s->event("Listening on port %d", port);
  }

  if (n == 0) die("no FIX servers found in the settings file");

  for (auto i = _threads.size(); i < _polls.size(); ++i) 
    _threads.push_back(new std::thread([=](){_polls[i]->loop();}));
}

void Acceptor::in_event(int fd)
{
  auto peer = accept(fd, NULL, NULL);
  if (peer < 0) return;
  setSockOpt(peer, TCP_NODELAY, getSockOpt(peer, TCP_NODELAY));
  setSockOpt(peer, SO_SNDBUF, getSockOpt(peer, SO_SNDBUF));
  setSockOpt(peer, SO_RCVBUF, getSockOpt(peer, SO_RCVBUF));
  Session::event(NULL, "Accepted connection from %s on port %d", getHostName(peer), getHostPort(peer));
  Session::event(NULL, "recv/send_buf=%d/%d tcp_nodelay=%d", 
        getSockOpt(peer, SO_RCVBUF),
        getSockOpt(peer, SO_SNDBUF),
        getSockOpt(peer, TCP_NODELAY));
  
  // a simple server implementation just for test, no username check
  for (auto it = _sharedSessions[fd].begin(); it != _sharedSessions[fd].end(); ++it) {
    auto s = *it;
    if (s->_fd < 0) {
      s->start(peer);
      return;
    }
  }
  closeSock(peer);
}
 
void App::wait()
{
  for (size_t i = 0; i < _threads.size(); ++i) _threads[i]->join(); 
}

void App::stop(bool wait)
{
  for (size_t i = 0; i < _polls.size(); ++i) _polls[i]->stop();
  for (size_t i = 0; i < _activeSessions.size(); ++i) _activeSessions[i]->stop(wait);
  this->wait();
}

App::~App()
{
  for (size_t i = 0; i < _polls.size(); ++i) delete _polls[i];
  for (size_t i = 0; i < _threads.size(); ++i) delete _threads[i];
}

