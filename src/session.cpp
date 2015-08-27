#include "session.hpp"

#include "app.hpp"
#include "soupbin3.hpp"

#include <fstream>

using namespace OUCH;

static sessions_t _sessions;
static std::map<str_t, Session*> _sessionMap;

namespace OUCH {
struct Timer : public i_poll_events
{
  Timer(Session& s) : _session(s) {}
  void in_event(int fd);
  
private:
  Session& _session;
};
}


static inline void lpadStr(char* dest, size_t destLen, const char* src, size_t srcLen)
{
  if (srcLen < destLen) {
    auto n = destLen - srcLen;
    memset(dest, ' ', n);
    dest += n;
    strncpy(dest, src, srcLen);
  } else
    strncpy(dest, src, destLen);
}

static inline void lpadStr(char* dest, size_t destLen, cstr_t& src) 
{ 
  lpadStr(dest, destLen, src.c_str(), src.size());
}

#define L_PAD_STR(dest, src) lpadStr(dest, sizeof(dest), src)


Session::Session(strmap_t settings) 
: _settings(settings),
  _username(get("Username")),
  _password(get("Password")),
  _firm(get("Firm")),
  _senderCompId(get("SenderCompId")),
  _targetCompId(get("TargetCompId")),
  _reconnectInterval(15),
  _isClient(get("ConnectionType") == "initiator" || get("ConnectionType") == "client"),
  _app(NULL),
  _poll(NULL),
  _handle(NULL),
  _outpoll(NULL),
  _outhandle(NULL),
  _timer(new Timer(*this)),
  _fd(-1),
  _state(st_none),
  _store(NULL),
  _log(NULL)
{
  if (_senderCompId.empty() && _isClient) _senderCompId = _username;
  if (_targetCompId.empty() && !_isClient) {
    _targetCompId = _username;
    if (_senderCompId.empty())
      _senderCompId = "OUCH";
  }
  _id = makeId(_senderCompId, _targetCompId);
  _tfd = timerfd_create(CLOCK_REALTIME, 0);
  auto n = atoi(get("ReconnectInterval").c_str());
  if (n > 0) _reconnectInterval = n;
}

str_t Session::makeId(cstr_t& senderCompId, cstr_t& targetCompId)
{
  return senderCompId + "->" + targetCompId;
}

const sessions_t& Session::getSessions() { return _sessions; }

sessions_t Session::createSessions(cstr_t& file)
{
  static std::map<str_t, sessions_t> sessionsPerFile;
  if (sessionsPerFile.find(file) != sessionsPerFile.end()) return sessionsPerFile[file];
  return sessionsPerFile[file] = createSessions(readSettings(file));
}

sessions_t Session::createSessions(std::istream& stream)
{
  return createSessions(::readSettings(stream));
}

sessions_t Session::createSessions(const sections_t& sections)
{
  sessions_t ans;
  for (size_t i = 0; i < sections.size(); ++i) {
    StrMapIgnoreCase s(sections[i]);
    auto& username = s["Username"];
    if (username.empty())
      die("Username not given in #" + itoa(i+1) + " session");

    auto& password = s["Password"];
    if (password.empty())
      die("password not given in #" + itoa(i+1) + " session");

    auto& type = s["ConnectionType"];
    if (type != "initiator" && type != "client" && type != "acceptor" && type != "server") 
      die("ConnectionType must be 'initiator', 'client', 'acceptor' or 'server' in #" 
          + itoa(i+1) + " session");
    bool isClient = (type == "initiator" || type == "client");

    if (isClient && s["SocketConnectHost"].empty()) 
      die("SocketConnectHost not given in #" + itoa(i+1) + " session");

    auto port = s[isClient ? "SocketConnectPort" : "SocketAcceptPort"];
    if (port.empty())
      die((isClient ? str_t("SocketConnectPort") : str_t("SocketAcceptPort")) 
          + "not given in #" + itoa(i+1) + " session");
    if (atoi(port.c_str()) <= 0)
      die("invalid port '" + port + "' in #" + itoa(i+1) + " session");

    auto session = new Session(sections[i]);
    if (_sessionMap.find(session->_id) != _sessionMap.end())
      die("duplicate session " + session->_id);
    _sessionMap[session->_id] = session;
    _sessions.push_back(session);
    ans.push_back(session);
  }
  return ans;
}

sections_t Session::readSettings(cstr_t& file)
{
  std::ifstream fs(file.c_str());
  if (!fs.is_open())
    die("File '" + file + "' not found");
  return ::readSettings(fs);
}

void Session::event(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  event(this, format, args);
  va_end(args);
}

void Session::event(Session* p, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  event(p, format, args);
  va_end(args);
}
  
void Session::event(Session* p, const char* format, va_list args)
{
  char buf[256];
  vsnprintf(buf, sizeof(buf), format, args);
  if (p) p->_log->onEvent(buf);
  else {
    if (App::_defaultLog) App::_defaultLog->onEvent(buf);
    else std::cout << buf << '\n';
  }
}

void Session::connect(bool firstTime)
{
  auto host = get("SocketConnectHost");
  auto port = get("SocketConnectPort", 0);
  auto fd = createClientSock(host.c_str(), port);
  event("Connecting to %s on port %d", host.c_str(), port);
  if (fd < 0) {
    event("Connection failed");
    setTimer(_tfd, _reconnectInterval, 0);
    return;
  }
  start(fd);
  auto rsize = get("ReceiveBufferSize", 0);
  auto ssize = get("SendBufferSize", 0);
  if (rsize > 0) setSockOpt(fd, SO_RCVBUF, rsize);
  if (ssize > 0) setSockOpt(fd, SO_SNDBUF, ssize);
  event("Connection succeeded");
  if (firstTime)
    event("recv/send_buf=%d/%d tcp_nodelay=%d", 
        getSockOpt(fd, SO_RCVBUF),
        getSockOpt(fd, SO_SNDBUF),
        getSockOpt(fd, TCP_NODELAY));
  logon();
}

inline void Session::incrNextTargetMsgSeqNum()
{
  _store->incrNextTargetMsgSeqNum();
}

inline void Session::setNextTargetMsgSeqNum(int n)
{
  _store->setNextTargetMsgSeqNum(n);
}

void Session::in_event(int fd)
{
  if (_rxbuf.full()) _rxbuf.compact();
  auto start = _rxbuf.begin();
  auto nr = ::read(fd, _rxbuf.end(), _rxbuf.remaining());
  if (nr > 0) {
    clock_gettime(CLOCK_REALTIME, &_rxtm);
    _rxbuf.len += nr;
    while (_rxbuf.len > 2) {
      unsigned len = 2 + (start[1] | start[0] << 8);
      // std::cout << len << '\n';
      if (len <= _rxbuf.len) {
        if (len > 2) {
          auto msg = (Message*)(start + 3);
          bool countseq = true;
          switch (start[2]) { // type
            case SOUPBIN3_PACKET_SEQ_DATA:
              {
                switch (msg->type) {
                  case AcceptedMsg::TYPE:
                    ((AcceptedMsg*)msg)->ntoh();
                    break;
                  case ReplacedMsg::TYPE:
                    ((ReplacedMsg*)msg)->ntoh();
                    break;
                  case CanceledMsg::TYPE:
                    ((CanceledMsg*)msg)->ntoh();
                    break;
                  case AIQCanceledMsg::TYPE:
                    ((AIQCanceledMsg*)msg)->ntoh();
                    break;
                  case ExecMsg::TYPE:
                    ((ExecMsg*)msg)->ntoh();
                    break;
                  case BrokenTradeMsg::TYPE:
                    ((BrokenTradeMsg*)msg)->ntoh();
                    break;
                  case RejectedMsg::TYPE:
                    {
                      auto m = ((RejectedMsg*)msg);
                      m->ntoh();
                      if (m->reason == 'T')
                        countseq = false; // ignore test-mode rejections when counting seq
                    }
                    break;
                  case CancelPendingMsg::TYPE:
                    ((CancelPendingMsg*)msg)->ntoh();
                    break;
                  case CancelRejectMsg::TYPE:
                    ((CancelRejectMsg*)msg)->ntoh();
                    break;
                  case PriorityMsg::TYPE:
                    ((PriorityMsg*)msg)->ntoh();
                    break;
                  case ModifiedMsg::TYPE:
                    ((ModifiedMsg*)msg)->ntoh();
                    break;
                  case SysMsg::TYPE:
                    ((SysMsg*)msg)->ntoh();
                    break;
                  default:
                    event("unknown OUCH message type %c", msg->type);
                    close();
                    return;
                    break;
                }
                _log->onIncoming(msg,  len);
                _app->fromApp(*msg, *this);
              }
              if (countseq) incrNextTargetMsgSeqNum();
              break;
            case SOUPBIN3_PACKET_LOGIN_ACCEPTED:
              {
                event("Login accepted: %s", 
                    std::string(start+3, sizeof(soupbin3_packet_login_accepted)-3).c_str());
                auto msg = (soupbin3_packet_login_accepted*)start;
                auto n = 0;
                for (auto i = 0u; i < sizeof(msg->SequenceNumber); ++i) {
                  if (msg->SequenceNumber[i] == ' ') continue;
                  else n = n * 10 + (msg->SequenceNumber[i] - '0');
                }
                if (n != getExpectedTargetNum())
                  setNextTargetMsgSeqNum(n);
                _state = st_logon_received;
                _app->onLogon(*this);
              }
              assert(len == sizeof(soupbin3_packet_login_accepted));
              break;
            case SOUPBIN3_PACKET_LOGIN_REJECTED:
              event("Login rejected: %c", start[3]);
              close();
              assert(len == sizeof(soupbin3_packet_login_rejected));
              return;
              break;
            case SOUPBIN3_PACKET_SERVER_HEARTBEAT:
              assert(len == sizeof(soupbin3_packet_server_heartbeat));
              break;
            case SOUPBIN3_PACKET_END_OF_SESSION:
              event("End of session by peer");
              close();
              assert(len == sizeof(soupbin3_packet_end_of_session));
	      return;
              break;
            case SOUPBIN3_PACKET_CLIENT_HEARTBEAT:
              assert(len == sizeof(soupbin3_packet_client_heartbeat));
              break;
            case SOUPBIN3_PACKET_LOGIN_REQUEST:
              {
                event("Received logon request: %s", 
                    std::string(start+3, sizeof(soupbin3_packet_login_request)-3).c_str());
                soupbin3_packet_login_accepted msg;
                msg.PacketLength = htons(sizeof(msg)-2);
                msg.PacketType = SOUPBIN3_PACKET_LOGIN_ACCEPTED;
                memset(msg.Session, ' ', sizeof(msg.Session)); 
                char buf[20];
                snprintf(buf, sizeof(buf), "%d", getExpectedSenderNum());
                L_PAD_STR(msg.SequenceNumber, buf);
                send(&msg, sizeof(msg));
              }
              assert(len == sizeof(soupbin3_packet_login_request));
              break;
            case SOUPBIN3_PACKET_UNSEQ_DATA:
              {
                // for test only
                switch (msg->type) {
                  case OrderMsg::TYPE:
                    ((OrderMsg*)msg)->ntoh();
                    break;
                  case ReplaceMsg::TYPE:
                    ((ReplaceMsg*)msg)->ntoh();
                    break;
                  case CancelMsg::TYPE:
                    ((CancelMsg*)msg)->ntoh();
                    break;
                }
                _log->onIncoming(msg,  len);
                _app->fromApp(*msg, *this);
              }
              break;
          }
        }
        _rxbuf.advance(len);
        start = _rxbuf.begin();
      } else
        break;
    }
  } else if (nr == 0 || (errno != EAGAIN && errno != EINTR)) {
    event("Connection reset by peer: nr=%d errno=%d", nr, errno);
    close();
  }
}

void Session::out_event(int fd)
{
  const char* data;
  size_t n;
  if (_outpipe.data(data, n)) {
    int done = ::write(fd, data, n); 
    if (done > 0) _outpipe.pop(done);
  } else {
    _outpoll->reset_pollout(_outhandle);
  }
}

void Session::close()
{
  event("Disconnecting");
  _app->onLogout(*this);
  _poll->rm_fd(_handle);
  if (_poll != _outpoll) _outpoll->rm_fd(_outhandle);
  closeSock(_fd);
  _rxbuf.reset();
  _outpipe.reset();
  setTimer(_tfd, isClient() ? _reconnectInterval : 0, 0);
  _fd = -1;
  _state = st_session_terminated;
}

void Timer::in_event(int fd)
{
  char buf[256];
  if (read(fd, buf, sizeof(buf))) {} // have to read because we are not using EPOLLET mode

  if (_session._fd < 0) {
    _session.connect();
    return;
  }

  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  auto diff = now.tv_sec + now.tv_nsec / 1.e9 - _session._rxtm.tv_sec - _session._rxtm.tv_nsec / 1.e9;
  if (diff >= _session._reconnectInterval) {
    // for ease, not send log off
    _session.event("Timed out waiting for heartbeat");
    _session.close();
    return;
  } 
  
  diff = now.tv_sec + now.tv_nsec / 1.e9 - _session._txtm.tv_sec - _session._txtm.tv_nsec / 1.e9;
  if (diff >= 1)
    _session.heartbeat();
}

void Session::start(int fd)
{  
  _fd = fd;
  if (setNonBlocking(fd)) event("Failed to set non blocking mode");
  _handle = _poll->add_fd(fd, this);
  _poll->set_pollin(_handle);
  _outhandle = _poll == _outpoll ? _handle : _outpoll->add_fd(fd, this);
  setTimer(_tfd, 1, 1);
}

bool Session::send(void* data, size_t len)
{
  if (_fd < 0) return true;

  clock_gettime(CLOCK_REALTIME, &_txtm);

  lock_t lock(_m);
  
  _outpipe.push((char*)data, len);
  _outpoll->set_pollout(_outhandle);

  return true;
}

void Session::logon()
{
  soupbin3_packet_login_request msg;
  msg.PacketLength = htons(sizeof(msg)-2);
  msg.PacketType = SOUPBIN3_PACKET_LOGIN_REQUEST;
  R_PAD_STR(msg.Username, username());
  R_PAD_STR(msg.Password, password());
  memset(msg.RequestedSession, ' ', sizeof(msg.RequestedSession)); 
  char buf[20];
  snprintf(buf, sizeof(buf), "%d", getExpectedTargetNum());
  L_PAD_STR(msg.RequestedSequenceNumber, buf);
  event("Initiated logon request: %s",
      std::string((char*)&msg+3, sizeof(soupbin3_packet_login_request)-3).c_str());
  send(&msg, sizeof(msg));
  _state = st_logon_sent;
  clock_gettime(CLOCK_REALTIME, &_rxtm);
  _txtm = _rxtm;
}

void Session::logout()
{
  event("Initiated logout request");
  soupbin3_packet_logout_request msg;
  msg.PacketLength = htons(sizeof(msg)-2);
  msg.PacketType = SOUPBIN3_PACKET_LOGOUT_REQUEST;
  send(&msg, sizeof(msg));
  _state = st_logoff_sent;
}

void Session::heartbeat()
{
  soupbin3_packet_server_heartbeat msg;
  msg.PacketLength = htons(sizeof(msg)-2);
  msg.PacketType = isClient() ? SOUPBIN3_PACKET_CLIENT_HEARTBEAT : SOUPBIN3_PACKET_SERVER_HEARTBEAT;
  send(&msg, sizeof(msg));
}

void Session::stop(bool wait)
{
  if (_log) _log->stop(wait);
  if (_store) _store->stop(wait);
}

Session::~Session()
{
  delete _log;
  delete _store;
  delete _timer;
}

