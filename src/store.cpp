#include "store.hpp"

#include "session.hpp"

using namespace OUCH;

bool MemoryStore::set(const void* data, size_t len)
{
  auto& m = _messages[getNextSenderMsgSeqNum()];
  m.assign((char*)data, len);
  return true;
}

void MemoryStore::get(int begin, int end, strvec_t& messages) const
{
  messages.clear();
  Messages::const_iterator find = _messages.find(begin);
  for (; find != _messages.end() && find->first <= end; ++find)
    messages.push_back(find->second);
}

FileStore::FileStore(const Session& s)
: _msgFile(0), _headerFile(0), _seqNumsFile(0), _sessionFile(0)
{
  auto path = mystrftime(s.get("FileStorePath"));
  if (path == s.get("FileStorePath")) path = mystrftime(path + "/%Y%m%d");
  mkdirs(path);

  if (path.empty()) path = ".";

  auto sessionid = s.senderCompId() + "-" + s.targetCompId();

  std::string prefix = path + '/' + sessionid + ".";

  _msgFileName = prefix + "body";
  _headerFileName = prefix + "header";
  _seqNumsFileName = prefix + "seqnums";
  _sessionFileName = prefix + "session";

  open(false);
}

FileStore::~FileStore()
{
  if (_msgFile) fclose(_msgFile);
  if (_headerFile) fclose(_headerFile);
  if (_seqNumsFile) fclose(_seqNumsFile);
  if (_sessionFile) fclose(_sessionFile);
}

void FileStore::open(bool deleteFile)
{ 
  if (_msgFile) fclose(_msgFile);
  if (_headerFile) fclose(_headerFile);
  if (_seqNumsFile) fclose(_seqNumsFile);
  if (_sessionFile) fclose(_sessionFile);

  _msgFile = 0;
  _headerFile = 0;
  _seqNumsFile = 0;
  _sessionFile = 0;

  if (deleteFile) {
    unlink(_msgFileName.c_str());
    unlink(_headerFileName.c_str());
    unlink(_seqNumsFileName.c_str());
    unlink(_sessionFileName.c_str());
  }

  populateCache();
  _msgFile = fopen(_msgFileName.c_str(), "r+");
  if (!_msgFile) _msgFile = fopen(_msgFileName.c_str(), "w+");
  if (!_msgFile) die("Could not open body file: " + _msgFileName);

  _headerFile = fopen(_headerFileName.c_str(), "r+");
  if (!_headerFile) _headerFile = fopen(_headerFileName.c_str(), "w+");
  if (!_headerFile) die("Could not open header file: " + _headerFileName);

  _seqNumsFile = fopen(_seqNumsFileName.c_str(), "r+");
  if (!_seqNumsFile) _seqNumsFile = fopen(_seqNumsFileName.c_str(), "w+");
  if (!_seqNumsFile) die("Could not open seqnums file: " + _seqNumsFileName);

  bool setCreationTime = false;
  _sessionFile = fopen(_sessionFileName.c_str(), "r");
  if (!_sessionFile) setCreationTime = true;
  else fclose(_sessionFile);

  _sessionFile = fopen(_sessionFileName.c_str(), "r+");
  if (!_sessionFile) _sessionFile = fopen(_sessionFileName.c_str(), "w+");
  if (!_sessionFile) die("Could not open session file");
  if (setCreationTime) setSession();

  setNextSenderMsgSeqNum(getNextSenderMsgSeqNum());
  setNextTargetMsgSeqNum(getNextTargetMsgSeqNum());
}

void FileStore::populateCache()
{ 
  std::string msg;

  FILE* headerFile;
  headerFile = fopen(_headerFileName.c_str(), "r+");
  if (headerFile) {
    int num, offset, size;
    while (fscanf(headerFile, "%d,%d,%d ", &num, &offset, &size) == 3)
      _offsets[num] = std::make_pair(offset, size);
    fclose(headerFile);
  }

  FILE* seqNumsFile;
  seqNumsFile = fopen(_seqNumsFileName.c_str(), "r+");
  if (seqNumsFile) {
    int sender, target;
    if (fscanf(seqNumsFile, "%d : %d", &sender, &target) == 2) {
      _cache.setNextSenderMsgSeqNum(sender);
      _cache.setNextTargetMsgSeqNum(target);
    }
    fclose(seqNumsFile);
  }

  FILE* sessionFile;
  sessionFile = fopen(_sessionFileName.c_str(), "r+");
  if (sessionFile) {
    char time[22];
    int result = fscanf(sessionFile, "%s", time);
    if (result == 1)
      _cache.setCreationTime(UtcTimeStampConvertor::convert(time, true));
    fclose(sessionFile);
  }
}

inline bool FileStore::set(int msgSeqNum, const void* data, size_t len)
{ 
  if (fseek(_msgFile, 0, SEEK_END)) 
    die("Cannot seek to end of " + _msgFileName);
  if (fseek(_headerFile, 0, SEEK_END)) 
    die("Cannot seek to end of " + _headerFileName);

  int offset = ftell(_msgFile);
  if (offset<0) 
    die("Unable to get file pointer position from " + _msgFileName);

  if (fprintf(_headerFile, "%d,%d,%ld ", msgSeqNum, offset, len)<0)
    die("Unable to write to file " + _headerFileName);
  _offsets[msgSeqNum] = std::make_pair(offset, len);
  fwrite(data, sizeof(char), len, _msgFile);
  if (ferror(_msgFile)) 
    die("Unable to write to file " + _msgFileName);
  if (fflush(_msgFile) == EOF) 
    die("Unable to flush file " + _msgFileName);
  if (fflush(_headerFile) == EOF) 
    die("Unable to flush file " + _headerFileName);
  return true;
}

bool FileStore::set(const void* data, size_t len)
{
  return set(getNextSenderMsgSeqNum(), data, len);
}

void FileStore::get(int begin, int end, strvec_t& result) const
{ 
  result.clear();
  std::string msg;
  for (int i = begin; i <= end; ++i)
  {
    if (get(i, msg))
      result.push_back(msg);
  }
}

int FileStore::getNextSenderMsgSeqNum() const
{ 
  return _cache.getNextSenderMsgSeqNum();
}

int FileStore::getNextTargetMsgSeqNum() const
{
  return _cache.getNextTargetMsgSeqNum();
}

void FileStore::setNextSenderMsgSeqNum(int value)
{
  _cache.setNextSenderMsgSeqNum(value);
  setSeqNum();
}

void FileStore::setNextTargetMsgSeqNum(int value)
{
  _cache.setNextTargetMsgSeqNum(value);
  setSeqNum();
}

void FileStore::incrNextSenderMsgSeqNum()
{
  _cache.incrNextSenderMsgSeqNum();
  setSeqNum();
}

void FileStore::incrNextTargetMsgSeqNum()
{
  _cache.incrNextTargetMsgSeqNum();
  setSeqNum();
}

UtcTimeStamp FileStore::getCreationTime() const
{
  return _cache.getCreationTime();
}

void FileStore::reset()
{
  _cache.reset();
  open(true);
  setSession();
}

void FileStore::refresh()
{
  _cache.reset();
  open(false);
}

void FileStore::setSeqNum()
{
  rewind(_seqNumsFile);
  fprintf(_seqNumsFile, "%10.10d : %10.10d",
           getNextSenderMsgSeqNum(), getNextTargetMsgSeqNum());
  if (ferror(_seqNumsFile)) 
    die("Unable to write to file " + _seqNumsFileName);
  if (fflush(_seqNumsFile)) 
    die("Unable to flush file " + _seqNumsFileName);
}

void FileStore::setSession()
{
  rewind(_sessionFile);
  fprintf(_sessionFile, "%s",
           UtcTimeStampConvertor::convert(_cache.getCreationTime()).c_str());
  if (ferror(_sessionFile)) 
    die("Unable to write to file " + _sessionFileName);
  if (fflush(_sessionFile)) 
    die("Unable to flush file " + _sessionFileName);
}

bool FileStore::get(int msgSeqNum, std::string& msg) const
{
  NumToOffset::const_iterator find = _offsets.find(msgSeqNum);
  if (find == _offsets.end()) return false;
  const OffsetSize& offset = find->second;
  if (fseek(_msgFile, offset.first, SEEK_SET)) 
    die("Unable to seek in file " + _msgFileName);
  char* buffer = new char[offset.second + 1];
  if (fread(buffer, sizeof(char), offset.second, _msgFile) == 0) {}
  if (ferror(_msgFile)) 
    die("Unable to read from file " + _msgFileName);
  buffer[offset.second] = 0;
  msg = buffer;
  delete [] buffer;
  return true;
}

void AsyncFileStore::in_event(int fd)
{
  uint64_t value;
  if (!read(fd, &value, 8)) {assert(0);} // have to read because we are not using EPOLLET mode
  getData();
  auto _data = (char*)(_h + 1);
  switch (_h->type) {
    case SET:
      {
        lock_t l(_mf);
        auto n = (int*)_data;
        FileStore::set(*n, _data+4, _h->len - 4);
      }
      break;
    case SET_SEQNUM:
      FileStore::setSeqNum();
      break;
    default:
      assert(0);
  }
  release();
}

