#include "log.hpp"

#include "session.hpp"
#include "ouch.hpp"

using namespace OUCH;

static std::string fileLogPath;

FileLog::FileLog(const Session& s)
{
  auto path = mystrftime(s.get("FileLogPath"));
  if (path == s.get("FileLogPath")) path = mystrftime(path + "/%Y%m%d");
  mkdirs(path);
  if (path.empty()) path = ".";
  if (fileLogPath.empty()) fileLogPath = path + '/';
  auto sessionid = s.senderCompId() + "-" + s.targetCompId();
  auto prefix = path + '/' + sessionid + ".";

  auto fn = prefix + "messages.current.log";
  _messages.open(fn.c_str(), std::ios::out | std::ios::app);
  if (!_messages.is_open()) die("Could not open messages file: " + fn);
  fn = prefix + "events.current.log";
  _events.open(fn.c_str(), std::ios::out | std::ios::app);
  if (!_events.is_open()) die("Could not open events file: " + fn);
}

FileLog::FileLog()
{
  auto prefix = fileLogPath + "GLOBAL.";

  auto fn = prefix + "messages.current.log";
  _messages.open(fn.c_str(), std::ios::out | std::ios::app);
  if (!_messages.is_open()) die("Could not open messages file: " + fn);
  fn = prefix + "events.current.log";
  _events.open(fn.c_str(), std::ios::out | std::ios::app);
  if (!_events.is_open()) die("Could not open events file: " + fn);
}

void AsyncFileLog::in_event(int fd)
{
  uint64_t value;
  if (!read(fd, &value, 8)) {assert(0);} // have to read because we are not using EPOLLET mode
  getData();
  auto _data = (char*)(_h + 1);
  switch (_h->type) {
    case LOG:
      _messages << nowUtcStr() << " : ";
      write(_messages, _data, _h->len);
      _messages << std::endl;
      break;
    case EVENT:
      _events << nowUtcStr() << " : ";
      _events.write(_data, _h->len);
      _events << std::endl;
      break;
    default:
      assert(0);
  }
  release();
}

void 
Log::write(std::ostream& out, const void* msg, size_t len)
{
  switch (((Message*)msg)->type) {
    case OrderMsg::TYPE:
      ((OrderMsg*)msg)->write(out);
      break;
    case CancelMsg::TYPE:
      ((CancelMsg*)msg)->write(out);
      break;
    case AcceptedMsg::TYPE:
      ((AcceptedMsg*)msg)->write(out);
      break;
    case ReplacedMsg::TYPE:
      if (len == sizeof(ReplacedMsg) + 3)
        ((ReplacedMsg*)msg)->write(out);
      else 
        ((ReplaceMsg*)msg)->write(out);
      break;
    case ModifiedMsg::TYPE:
      if (len == sizeof(ModifiedMsg) + 3)
        ((ModifiedMsg*)msg)->write(out);
      else
        ((ModifyMsg*)msg)->write(out);
    case CanceledMsg::TYPE:
      ((CanceledMsg*)msg)->write(out);
      break;
    case AIQCanceledMsg::TYPE:
      ((AIQCanceledMsg*)msg)->write(out);
      break;
    case ExecMsg::TYPE:
      ((ExecMsg*)msg)->write(out);
      break;
    case BrokenTradeMsg::TYPE:
      ((BrokenTradeMsg*)msg)->write(out);
      break;
    case RejectedMsg::TYPE:
      ((RejectedMsg*)msg)->write(out);
      break;
    case CancelPendingMsg::TYPE:
      ((CancelPendingMsg*)msg)->write(out);
      break;
    case CancelRejectMsg::TYPE:
      ((CancelRejectMsg*)msg)->write(out);
      break;
    case PriorityMsg::TYPE:
      ((PriorityMsg*)msg)->write(out);
      break;
    case SysMsg::TYPE:
      ((SysMsg*)msg)->write(out);
      break;
  }
}

