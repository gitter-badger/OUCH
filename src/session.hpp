#ifndef OUCH_SESSION_HPP
#define OUCH_SESSION_HPP

#include "util.hpp"
#include "epoll.hpp"
#include "store.hpp"
#include "log.hpp"
#include "pipe.hpp"
#include "ouch.hpp"
#include "soupbin3.hpp"

#include <mutex>

namespace OUCH {

class Session;
class App;
typedef std::vector<Session*> sessions_t;
enum SessionState { 
  st_none, st_session_terminated,
  st_logon_sent, st_logon_received, st_logoff_sent,
  st_num_states
};

class Session : public i_poll_events, public noncopyable
{
public:
  Session(strmap_t settings);
  virtual ~Session();
  cstr_t& username() const { return _username; }
  cstr_t& password() const { return _password; }
  cstr_t& firm() const { return _firm; }
  cstr_t& senderCompId() const { return _senderCompId; }
  cstr_t& targetCompId() const { return _targetCompId; }
  cstr_t& id() const { return _id; }
  int reconnectInterval() const { return _reconnectInterval; }
  bool isClient() const { return _isClient; }
  bool isInitiator() const { return isClient(); }
  cstr_t& get(cstr_t& key) const { return _I(_settings)[key]; }
  int get(cstr_t& key, int defaultValue) const { return _I(_settings).get(key, defaultValue); }
  int getExpectedSenderNum() const { return _store->getNextSenderMsgSeqNum(); }
  int getExpectedTargetNum() const { return _store->getNextTargetMsgSeqNum(); }

  static const sessions_t& getSessions();
  static sessions_t createSessions(cstr_t& file);
  static sessions_t createSessions(std::istream& stream);
  static sessions_t createSessions(const sections_t& sections);
  static sections_t readSettings(cstr_t& file);
  static str_t makeId(cstr_t& senderCompId, cstr_t& targetCompId);
  bool isLoggedOn() const { return _state == st_logon_received; }
  bool resendRequested() const { return true; }

  template <typename T>
  bool send(const T& msg)
  {
    char packet[sizeof(soupbin3_packet)+sizeof(T)];
    auto head = (soupbin3_packet*)packet;
    head->PacketLength = htons(sizeof(T)+1);
    head->PacketType = isClient() ? 'U' : 'S';
    auto body = (T*)(head+1);
    *body = msg;
    body->hton();
    send(&packet, sizeof(packet));
    _log->onOutgoing(&msg, sizeof(T));
    return true;
  }

private:
  bool send(void* data, size_t len);
  void event(const char* format, ...);
  static void event(Session* p, const char* format, ...);
  static void event(Session* p, const char* format, va_list args);
  void connect(bool firstTime=false);
  void close();
  void start(int fd);
  void in_event(int fd);
  void out_event(int fd);
  void logon();
  void heartbeat();
  void logout();
  void incrNextTargetMsgSeqNum();
  void setNextTargetMsgSeqNum(int n);
  void stop(bool wait);

private:
  strmap_t _settings;
  str_t _username;
  str_t _password;
  str_t _firm;
  str_t _senderCompId;
  str_t _targetCompId;
  str_t _id;
  int _reconnectInterval;
  bool _isClient;
  friend class App;
  friend class Server;
  friend class Acceptor;
  friend class Timer;
  friend class _C;
  App* _app;
  epoll_t* _poll;
  epoll_t::handle_t _handle;
  epoll_t* _outpoll;
  epoll_t::handle_t _outhandle;
  int _tfd; // timer file id
  i_poll_events* _timer;
  int _fd;
  SessionState _state;
  MessageStore* _store;
  Log* _log;
  MYPIPE::Pipe _outpipe;

  struct Buffer {
    Buffer() : start(0), len(0) {}
    void reset() { start = len = 0; }
    static const size_t FIX_MAX_MESSAGE_SIZE = 1024; // big enough for OUCH
    bool full() const { return start + len + FIX_MAX_MESSAGE_SIZE > cap; }
    size_t remaining() const { return cap - start - len; }
    void advance(size_t n) { start += n; len -= n; }
    void compact() { memmove(data, data + start, len); start = 0; }
    char* begin() { return data + start; }
    char* end() { return begin() + len; }
    static const size_t cap = 1024 * 1024;
    char data[cap];
    size_t start;
    size_t len;
  };
  Buffer _rxbuf;
  struct timespec _rxtm;
  struct timespec _txtm;
 
  typedef std::lock_guard<std::mutex> lock_t;
  std::mutex _m;

  typedef StrMapIgnoreCase _I;
};

} // namespace OUCH

#endif
