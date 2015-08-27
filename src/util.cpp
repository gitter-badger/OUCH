#include "util.hpp"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>

namespace OUCH {

cstr_t strerror()
{
  char buf[256];
  return strerror_r(errno, buf, sizeof(buf)); // do not return buf, strerror_r(..) != buf
}

void die(const char* format, ...)
{
  char buf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  throw std::runtime_error(buf);
}

void die(cstr_t& msg) 
{
  throw std::runtime_error(msg);
}

void dieerr(cstr_t& msg)
{
  die(msg + ": " + strerror());
}

void dieerr(const char* format, ...)
{
  char buf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  die(str_t(buf) + ": " + strerror());
}

str_t replace(cstr_t& value, cstr_t& oldValue, cstr_t& newValue)
{
  str_t copy = value;
  for(str_t::size_type pos = copy.find(oldValue);
      pos != str_t::npos;
      pos = copy.find(oldValue, pos)) {
    copy.replace(pos, oldValue.size(), newValue);
    pos += newValue.size();
  }
  return copy;
}

int setSockOpt(int fd, int opt, int optval)
{
  int level = SOL_SOCKET;
  if(opt == TCP_NODELAY) level = IPPROTO_TCP;
  return ::setsockopt(fd, level, opt, &optval, sizeof(optval));
}

int setNonBlocking(int fd)
{
  return fcntl(fd, F_SETFL, O_NONBLOCK);
}

int getSockOpt(int s, int opt)
{ 
  int level = SOL_SOCKET;
  if(opt == TCP_NODELAY) level = IPPROTO_TCP;
  socklen_t length = sizeof(socklen_t);
  int optval = -1;
  if (getsockopt(s, level, opt, (char*)&optval, &length) < 0) perror("failed to getsockopt");
  return optval;
}

short getHostPort(int socket)
{
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  if(getsockname(socket, (struct sockaddr*)&addr, &len) < 0) return 0;
  return ntohs(addr.sin_port);
}

const char* getHostName(int socket)
{ 
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  if(getsockname(socket, (struct sockaddr*)&addr, &len) < 0) return NULL;
  return inet_ntoa(addr.sin_addr);
}

const char* getHostName(const char* name)
{
  auto host_ptr = gethostbyname(name);
  if (!host_ptr) return NULL;
  auto paddr = (struct in_addr **) host_ptr->h_addr_list;
  return inet_ntoa(**paddr);
}

void closeSock(int fd) 
{
  shutdown(fd, 2);
  close(fd);
}

int createClientSock(const char* address, int port)
{
  auto fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0) dieerr("cannot create socket");

  if (setSockOpt(fd, TCP_NODELAY, 1) < 0) 
    perror("cannot set socket option TCP_NODELAY");
 
  auto hostname = getHostName(address);
  if(!hostname) dieerr("failed to get hostname " + str_t(address));

  sockaddr_in addr;
  addr.sin_family = PF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr(hostname);

  if (connect(fd, (sockaddr*)(&addr), sizeof(addr)) < 0) {
    closeSock(fd);
    fd = -1;
  }
  return fd;
}

const char* getPeerName(int socket)
{
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  if(getpeername(socket, (struct sockaddr*)&addr, &len) < 0)
    return "UNKNOWN";
  char* result = inet_ntoa(addr.sin_addr);
  if(result)
    return result;
  else
    return "UNKNOWN";
}

int createAcceptor(int port)
{
  auto fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0) dieerr("cannot create socket");

  if (setSockOpt(fd, TCP_NODELAY, 1) < 0) 
    perror("cannot set socket option TCP_NODELAY");

  if (setSockOpt(fd, SO_REUSEADDR, 1) < 0)
    perror("cannot set socket option SO_REUSEADDR");

  struct sockaddr_in sa;
  bzero(&sa, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = INADDR_ANY;

  if (bind(fd, (const struct sockaddr *)&sa, sizeof(struct sockaddr_in)) < 0)
    dieerr("failed to bind port %d", port);

  if (listen(fd, SOMAXCONN) < 0) dieerr("listen failed");

  return fd;
}

sections_t readSettings(std::istream& stream)
{
  sections_t sections;
  typedef std::multimap<str_t, strmap_t> secmap_t;
  str_t line;
  size_t equals;
  secmap_t::iterator it;
  secmap_t secmap;

  while(std::getline(stream, line)) {
    trim(line);
    if (line.empty()) continue;
    if (line[0] == '#') continue;
    if (line[0] == '[' && line[line.size()-1] == ']') {
      it = secmap.insert(secmap_t::value_type(toLower(trim(line.substr(1, line.size() - 2))), strmap_t()));
    } else if((equals = line.find('=')) != str_t::npos) {
      if (secmap.empty()) continue;
      it->second[toLower(trim(line.substr(0, equals)))] = trim(line.substr(equals + 1));
    }
  }

  auto range = secmap.equal_range("default");
  strmap_t defaults;
  if (range.first != range.second) defaults.swap(range.first->second);
  
  range = secmap.equal_range("session");
  for (auto it = range.first; it != range.second; ++it) {
    auto session = defaults;
    for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) session[it2->first] = it2->second;
    sections.push_back(session);
  }

  return sections;
}

void mkdirs(const std::string& path, bool isfile)
{
  auto p = path.c_str();
  int length = strlen(p);
  std::string createPath;

  for (const char* pos = p; pos - p <= length; ++pos) {
    createPath += *pos;
    if (*pos == '/' || (pos - p == length && !isfile)) {
      // use umask to override rwx for all
      mkdir(createPath.c_str(), 0777);
    }   
  }
}

Queue::Queue()
{
  _head = _tail = new Chunk; _spared = NULL;
  _fd = eventfd(0, EFD_SEMAPHORE);
  _poll.set_pollin(_poll.add_fd(_fd, this));
  _thread = new std::thread([=](){_poll.loop();});
}

void Queue::stop(bool wait)
{
  while (wait) {
    {
      lock_t l(_m);
      if (_head == _tail && _head->tail == _head->head) break;
    }
    usleep(1000);
  }
  
  _poll.stop();
  _thread->join();
}

Queue::~Queue()
{
  delete _thread; // to-do, how to make sure exit safely with avoiding message not dumped
}

static __thread char* tmp;
const char* nowUtcStr()
{
  if (!tmp) tmp = new char[64];
  struct timespec now;
	struct tm tm;

	if (clock_gettime(CLOCK_REALTIME, &now)) return "";
	if (!gmtime_r(&now.tv_sec, &tm)) return "";

  char fmt[64];
	strftime(fmt, sizeof(fmt), "%Y%m%d-%H:%M:%S", &tm);
	snprintf(tmp, 64, "%s.%03ld", fmt, (long)now.tv_nsec / 1000000);
  return tmp;
}

} // namespace OUCH
