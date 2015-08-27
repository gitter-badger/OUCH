#ifndef OUCH_LOG_HPP
#define OUCH_LOG_HPP

#include "util.hpp"

namespace OUCH {

struct Log
{
  virtual ~Log() {}

  virtual void onIncoming(const void* msg, size_t len) = 0;
  virtual void onOutgoing(const void* msg, size_t len) = 0;
  virtual void onEvent(const char* msg) = 0;
  virtual void stop(bool wait) {}

  void write(std::ostream& out, const void* msg, size_t len);
};

struct NullLog : public Log
{
  void onIncoming(const void* msg, size_t len) {}
  void onOutgoing(const void* msg, size_t len) {}
  void onEvent(const char* msg) {}
};

class Session;

struct LogFactory
{
  virtual Log* create(const Session&) { return new NullLog; }
  virtual Log* create()  { return new NullLog; }
};

template<typename T>
struct LogFactoryTmpl : public LogFactory
{
  virtual Log* create(const Session& s) { return new T(s); }
  virtual Log* create()  { return new T; }
};

struct ScreenLog : public Log
{
  ScreenLog(const Session& s) {}
  ScreenLog() {}
  void onIncoming(const void* msg, size_t len)
  {
    std::cout << nowUtcStr() << " in ";
    std::cout << '<';
    write(std::cout, msg, len);
    std::cout << ">\n";
  }
  void onOutgoing(const void* msg, size_t len)
  {
    std::cout << nowUtcStr() << " out ";
    std::cout << '<';
    write(std::cout, msg, len);
    std::cout << ">\n";
  }
  void onEvent(const char* msg)
  {
    std::cout << nowUtcStr() << " evt " << msg << "\n";
  }
};

struct FileLog : public Log
{
  FileLog(const Session& s);
  FileLog();
  void onIncoming(const void* msg, size_t len)
  {
    write(_messages, msg, len);
    _messages << std::endl;
  }
  void onOutgoing(const void* msg, size_t len)
  {
    write(_messages, msg, len);
    _messages << std::endl;
  }
  void onEvent(const char* msg)
  {
    _events << msg << std::endl;
  }

protected:
  std::ofstream _messages;
  std::ofstream _events;
};

struct AsyncFileLog: public FileLog, public Queue
{
  AsyncFileLog(const Session& s) : FileLog(s) {}
  AsyncFileLog() {}
  void onIncoming(const void* msg, size_t len)
  {
    lock_t l(_m);
    Head h(LOG, len);
    auto t = getChunk(sizeof(h) + h.len);
    memcpy(t->data + t->tail, &h, sizeof(h));
    t->tail += sizeof(h);
    memcpy(t->data + t->tail, msg, len);
    t->tail += len;
    uint64_t value = 1;
    if (::write(_fd, &value, 8)) {}
  }
  void onOutgoing(const void* msg, size_t len)
  {
    lock_t l(_m);
    Head h(LOG, len);
    auto t = getChunk(sizeof(h) + h.len);
    memcpy(t->data + t->tail, &h, sizeof(h));
    t->tail += sizeof(h);
    memcpy(t->data + t->tail, msg, len);
    t->tail += len;
    uint64_t value = 1;
    if (::write(_fd, &value, 8)) {}
  }
  void onEvent(const char* msg)
  {
    auto len = strlen(msg);
    lock_t l(_m);
    Head h(EVENT, len);
    auto t = getChunk(sizeof(h) + h.len);
    memcpy(t->data + t->tail, &h, sizeof(h));
    t->tail += sizeof(h);
    memcpy(t->data + t->tail, msg, len);
    t->tail += len;
    uint64_t value = 1;
    if (::write(_fd, &value, 8)) {}
  }
  void in_event(int fd);
  void stop(bool wait) { Queue::stop(wait); }
};

} // OUCH

#endif //OUCH_LOG_HPP
