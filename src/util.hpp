#ifndef OUCH_UTIL_HPP
#define OUCH_UTIL_HPP

#include "epoll.hpp"

#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
#include <string>
#include <map>
#include <vector>
#include <cstring>
#include <cassert>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <sys/timerfd.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <mutex>
#include <thread>
#include <unistd.h>

namespace OUCH {

typedef std::string str_t;
typedef const str_t cstr_t;
typedef std::vector<str_t> strvec_t;
typedef std::map<str_t, str_t> strmap_t;
typedef std::vector<strmap_t> sections_t;

static inline str_t ltrim(cstr_t& s) 
{
  str_t copy = s;
  copy.erase(copy.begin(), std::find_if(copy.begin(), copy.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
  return copy;
}

static inline str_t rtrim(cstr_t& s) 
{
  str_t copy = s;
  copy.erase(std::find_if(copy.rbegin(), copy.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), copy.end());
  return copy;
}

static inline str_t trim(cstr_t& s) 
{
  return ltrim(rtrim(s));
}

str_t replace(cstr_t& value, cstr_t& oldValue, cstr_t& newValue);

static inline str_t toUpper(cstr_t& value)
{ 
  str_t copy = value;
  std::transform(copy.begin(), copy.end(), copy.begin(), toupper);
  return copy;
}

static inline str_t toLower(cstr_t& value)
{ 
  str_t copy = value;
  std::transform(copy.begin(), copy.end(), copy.begin(), tolower);
  return copy;
}

static inline str_t itoa(long a) 
{
  char tmp[32];
  snprintf(tmp, sizeof(tmp), "%ld", a);
  return tmp;
}

static inline str_t ftoa(double a) 
{
  char tmp[32];
  snprintf(tmp, sizeof(tmp), "%f", a);
  return tmp;
}

static inline bool exists(const std::string& path)
{ 
  std::ifstream stream;
  stream.open(path.c_str(), std::ios_base::in);
  if(stream.is_open()) {
    stream.close();
    return true;
  }
  return false;
}

static cstr_t emptyString;

struct StrMapIgnoreCase
{
  StrMapIgnoreCase(const strmap_t& m) : _m(m) {}

  cstr_t& operator [](cstr_t& k) const 
  { 
    auto it = _m.find(toLower(k)); 
    return it == _m.end() ? emptyString : it->second;
  }

  int get(cstr_t& k, int defaultValue) const 
  { 
    auto it = _m.find(toLower(k)); 
    return it == _m.end() ? defaultValue : atoi(it->second.c_str());
  }
  
  const strmap_t& _m;
};

void die(const char* format, ...);
void die(cstr_t& msg);
void dieerr(cstr_t& msg);
void dieerr(const char* format, ...);

int setSockOpt(int fd, int opt, int optval);
int setNonBlocking(int fd);
int getSockOpt(int s, int opt);
short getHostPort(int socket);
const char* getHostName(int socket);
const char* getHostName(const char* name);
const char* getPeerName(int socket);
int createClientSock(const char* address, int port);
int createAcceptor(int port);
void closeSock(int fd);
void mkdirs(const std::string& path, bool isfile=false);
sections_t readSettings(std::istream& stream);

inline int setTimer(int fd, int seconds, int interval)
{
  struct itimerspec newtime = {{interval, 0}, {seconds, 0}};
  return timerfd_settime(fd, 0, &newtime, NULL); // relative timer
}

static inline void mystrftime(const char* pattern, char* out, int len, struct tm* timeinfo=NULL) 
{
  if (timeinfo) {
    strftime(out, len, pattern, timeinfo);
    return;
  }
  time_t t = time(NULL);
  struct tm today;
  localtime_r(&t, &today);
  strftime(out, len, pattern, &today);
}

static inline std::string mystrftime(const std::string& in, struct tm* tm=NULL)
{
  char out[256];
  OUCH::mystrftime(in.c_str(), out, sizeof(out), tm);
  return out;
}

class noncopyable
{
protected:
  noncopyable() {}
  ~noncopyable() {}

private:  // emphasize the following members are private
  noncopyable(const noncopyable&);
  noncopyable& operator=(const noncopyable&);
};

class SpinMutex
{
public:
  SpinMutex() { pthread_spin_init(&_m, 0); }
  ~SpinMutex() { pthread_spin_destroy(&_m); }
  void lock() { pthread_spin_lock(&_m); }
  void unlock() { pthread_spin_unlock(&_m); }

  class Locker 
  {
  public:
    Locker(SpinMutex& l) : _l(l) { l.lock(); }
    ~Locker() { _l.unlock(); }

  private:
    SpinMutex& _l;
  };

private:
  pthread_spinlock_t _m;
};

static const unsigned CHUNK_SIZE = 1024 * 1024;
struct Chunk
{
  Chunk(unsigned n=CHUNK_SIZE) : head(0), tail(0), capacity(std::max(n, CHUNK_SIZE)), data((char*)malloc(capacity)), next(NULL) {} 
  ~Chunk() { free(data); }
  void reset() { head = 0; tail = 0; next = NULL; }
  void resize(unsigned n) { capacity = n; free(data); data = (char*)malloc(n); }
  uint32_t head;
  uint32_t tail; 
  uint32_t capacity;
  char* data;
  Chunk* next;
};

struct Queue : public i_poll_events
{
  enum {SET, SET_SEQNUM, LOG, EVENT, UNKNOWN};
  struct Head { unsigned type:3; unsigned len:29; Head():type(UNKNOWN),len(0){}; Head(int t, int l):type(t), len(l){} };
  Chunk* getChunk(size_t n)
  {
    auto tail = _tail->tail;
    auto remained = _tail->capacity - tail;
    Chunk* t;
    if (remained >= n) {
      t = _tail;
    } else {
      if (_spared) {
        t = _spared;
        _spared = NULL;
        t->reset();
        if (t->capacity < n) t->resize(n);
      } else {
        t = new Chunk(n);
      }
      _tail->next = t;
      _tail = t;
    }
    return t;
  }
  void getData()
  {
    lock_t l(_m);
    if (_head->tail == _head->head) {
      assert(_head != _tail);
      auto next = _head->next;
      if (_spared) delete _head;
      else _spared = _head;
      _head = next;
    }
    _h = (Head*)(_head->data + _head->head);
  }
  void release()
  {
    lock_t l(_m);
    _head->head += sizeof(*_h) + _h->len;
  }

  Queue();
  void stop(bool wait=true);
  virtual ~Queue();

protected:
  // not sure if std::condition_variable or using semaphore directly is better solution.
  // sem_wait has no timeout
  epoll_t _poll; 
  int _fd; // eventfd 
  SpinMutex _m;
  typedef SpinMutex::Locker lock_t;
  Chunk* _head;
  Chunk* _tail;
  Chunk* _spared; 
  Head* _h;
  std::thread* _thread;
};

const char* nowUtcStr();
} // namespace OUCH

#endif
