#ifndef OUCH_STORE_HPP
#define OUCH_STORE_HPP

#include "datetime.hpp"
#include "util.hpp"

namespace OUCH
{
/**
 * This interface must be implemented to store and retrieve messages and
 * sequence numbers.
 */
class MessageStore
{
public:
  virtual ~MessageStore() {}

  virtual bool set(const void* data, size_t len) = 0;
  virtual void get(int, int, strvec_t&) const = 0;

  virtual int getNextSenderMsgSeqNum() const = 0;
  virtual int getNextTargetMsgSeqNum() const = 0;
  virtual void setNextSenderMsgSeqNum(int) = 0;
  virtual void setNextTargetMsgSeqNum(int) = 0;
  virtual void incrNextSenderMsgSeqNum() = 0;
  virtual void incrNextTargetMsgSeqNum() = 0;

  virtual UtcTimeStamp getCreationTime() const = 0;

  virtual void reset() = 0;
  virtual void refresh() = 0;
  virtual void stop(bool wait) {}
};

/**
 * Memory based implementation of MessageStore.
 *
 * This will lose all data on process terminition. This class should only
 * be used for test applications, never in production.
 */
class MemoryStore : public MessageStore
{
public:
  MemoryStore() : _nextSenderMsgSeqNum(1), _nextTargetMsgSeqNum(1) {}

  bool set(const void* data, size_t len);
  void get(int, int, strvec_t&) const;

  int getNextSenderMsgSeqNum() const
  { return _nextSenderMsgSeqNum; }
  int getNextTargetMsgSeqNum() const
  { return _nextTargetMsgSeqNum; }
  void setNextSenderMsgSeqNum(int value)
  { _nextSenderMsgSeqNum = value; }
  void setNextTargetMsgSeqNum(int value)
  { _nextTargetMsgSeqNum = value; }
  void incrNextSenderMsgSeqNum()
  { ++_nextSenderMsgSeqNum; }
  void incrNextTargetMsgSeqNum()
  { ++_nextTargetMsgSeqNum; }

  void setCreationTime(const UtcTimeStamp& creationTime)
  { _creationTime = creationTime; }
  UtcTimeStamp getCreationTime() const
  { return _creationTime; }

  void reset()
  {
    _nextSenderMsgSeqNum = 1; _nextTargetMsgSeqNum = 1;
    _messages.clear(); _creationTime.setCurrent();
  }
  void refresh() {}

private:
  typedef std::map<int, std::string> Messages;

  Messages _messages;
  int _nextSenderMsgSeqNum;
  int _nextTargetMsgSeqNum;
  UtcTimeStamp _creationTime;
};

class Session;

struct StoreFactory
{
  virtual MessageStore* create(const Session& s) { return new MemoryStore; }
};

template<typename T>
struct StoreFactoryTmpl : public StoreFactory
{
  virtual MessageStore* create(const Session& s) { return new T(s); }
};

/**
 * File based implementation of MessageStore.
 *
 * Four files are created by this implementation.  One for storing outgoing
 * messages, one for indexing message locations, one for storing sequence numbers,
 * and one for storing the session creation time.
 *
 * The formats of the files are:
 *   [path]+[BeginString]-[SenderCompID]-[TargetCompID].body
 *   [path]+[BeginString]-[SenderCompID]-[TargetCompID].header
 *   [path]+[BeginString]-[SenderCompID]-[TargetCompID].seqnums
 *   [path]+[BeginString]-[SenderCompID]-[TargetCompID].session
 *
 *
 * The messages file is a pure stream of FIX messages.
 * The sequence number file is in the format of
 *   [SenderMsgSeqNum] : [TargetMsgSeqNum]
 * The session file is a UTC timestamp in the format of
 *   YYYYMMDD-HH:MM:SS
 */
class FileStore : public MessageStore
{
public:
  FileStore(const Session& s);
  virtual ~FileStore();

  bool set(const void* data, size_t len);
  bool set(int msgSeqNum, const void* msg, size_t len);
  void get(int, int, strvec_t&) const;

  int getNextSenderMsgSeqNum() const;
  int getNextTargetMsgSeqNum() const;
  void setNextSenderMsgSeqNum(int value);
  void setNextTargetMsgSeqNum(int value);
  void incrNextSenderMsgSeqNum();
  void incrNextTargetMsgSeqNum();

  UtcTimeStamp getCreationTime() const;

  void reset();
  void refresh();

protected:
  typedef std::pair<int, int> OffsetSize;
  typedef std::map<int, OffsetSize> NumToOffset;

  void open(bool deleteFile);
  void populateCache();
  bool readFromFile(int offset, int size, std::string& msg);
  virtual void setSeqNum();
  void setSession();

  bool get(int, std::string&) const;

  MemoryStore _cache;
  NumToOffset _offsets;

  std::string _msgFileName;
  std::string _headerFileName;
  std::string _seqNumsFileName;
  std::string _sessionFileName;

  FILE* _msgFile;
  FILE* _headerFile;
  FILE* _seqNumsFile;
  FILE* _sessionFile;
};

class AsyncFileStore : public FileStore, public Queue 
{
public:
  AsyncFileStore(const Session& s) : FileStore(s) {}
  bool set(const void* data, size_t len)
  {
    int seqnum = getNextSenderMsgSeqNum();
    lock_t l(_m);
    Head h(SET, sizeof(seqnum) + len);
    auto t = getChunk(sizeof(h) + h.len);
    memcpy(t->data + t->tail, &h, sizeof(h));
    t->tail += sizeof(h);
    memcpy(t->data + t->tail, &seqnum, sizeof(seqnum));
    t->tail += sizeof(seqnum);
    memcpy(t->data + t->tail, data, len);
    t->tail += len;
    uint64_t value = 1;
    if (write(_fd, &value, 8)) {}
    return true;
  }
  void setSeqNum()
  {
    lock_t l(_m);
    Head h(SET_SEQNUM, 0);
    auto t = getChunk(sizeof(h));
    memcpy(t->data + t->tail, &h, sizeof(h));
    t->tail += sizeof(h);
    uint64_t value = 1;
    if (write(_fd, &value, 8)) {}
  }
  void get(int begin, int end, strvec_t& result) const { lock_t l(_mf); FileStore::get(begin, end, result); }
  void in_event(int fd);
  void stop(bool wait) { Queue::stop(wait); }
  
private:
  mutable SpinMutex _mf; // mutex for get and set, rarely happen at normal runtime 
};

} // OUCH

#endif //OUCH_STORE_HPP
