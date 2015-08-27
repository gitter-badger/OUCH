#ifndef OUCH_OUCH_H
#define OUCH_OUCH_H

#include <arpa/inet.h>
#include <fstream>
#include <string>
#include <iomanip>

// OUCH 4.2

namespace OUCH {
#ifndef packed
#define packed __attribute__ ((packed))
#endif

#define ntohll(y) (((uint64_t)ntohl(y)) << 32 | ntohl(y>>32)) // ! little-endian platform only
#define htonll(y) (((uint64_t)htonl(y)) << 32 | htonl(y>>32)) // ! little-endian platform only

static inline void rpadStr(char* dest, size_t destLen, const char* src, size_t srcLen)
{
  strncpy(dest, src, destLen);
  if (srcLen < destLen) 
    memset(dest + srcLen, ' ', destLen - srcLen);
}

static inline void rpadStr(char* dest, size_t destLen, const std::string& src) 
{ 
  rpadStr(dest, destLen, src.c_str(), src.size());
}

#define R_PAD_STR(dest, src) rpadStr(dest, sizeof(dest), src)

// all integer are unsigned big-endian
// alpha fileds are left-justified and padded on the right side with spaces

struct Message
{
  Message(char type=' ') : type(type) {}

  char type;
} packed;

static inline size_t lengthRTrim(const char* str, size_t n)
{
  auto p = str + n;
  while (p > str) {
    if (*(--p) == ' ') n--;
    else break;
  }
  return n;
}

static inline char toOuchSide(char side)
{
  switch (side) {
    case '1':
      return 'B';
      break;
    case '2':
      return 'S';
      break;
    case '5':
      return 'T';
      break;
    case '6':
      return 'E';
      break;
    default:
      return side;
  }
}

static inline void writeSide(std::ostream& out, char side)
{
  out << "54=";
  switch (side) {
    case 'B':
      out << '1';
      break;
    case 'S':
      out << '2';
      break;
    case 'T':
      out << '5';
      break;
    case 'E':
      out << '6';
      break;
    default:
      out << side;
      break;
  }
  out << '\1';
}

#define WRITE_RTRIM(out, str) out.write(str, lengthRTrim(str, sizeof(str)))

struct OrderMsg : public Message
{
  static const char TYPE = 'O';
  char id[14]; // clordid
  char side;
  int shares;
  char symbol[8];
  int price; // fixed point format, 4 decimal digits
  int tif; // in seconds, 0: ioc, 99998: until market close, 99999: until end of day
  char firm[4];
  char display;
  char capacity;
  char sweep; // intermarket sweep eligibility
  int minQty;
  char cross; // cross type
  //char customer;  // customer type

  OrderMsg(const std::string& id, char side, int shares, const std::string& symbol, int price, const std::string& firm="", char display= ' ')
    : Message(TYPE), side(side), shares(shares), price(price), tif(99998), 
      display(display), capacity('A'), sweep('N'), minQty(0), cross('N')//, customer(' ')
  {
    R_PAD_STR(this->id, id);
    R_PAD_STR(this->symbol, symbol);
    R_PAD_STR(this->firm, firm);
  }

  void write(std::ostream& out)
  {
    out << "35=" << "D" << '\1';

    out << "11=";
    WRITE_RTRIM(out, id);
    out << '\1';

    writeSide(out, side);
    out << "38=" << shares << '\1';

    out << "55=";
    WRITE_RTRIM(out, symbol);
    out << '\1';

    if (price) out << "44=" << (price/10000) << '.' << std::setfill('0') << std::setw(4) << (price%10000) << '\1';
    out << "59=" << tif << '\1';
    if (*firm != ' ') { // similar to SenderCompID 
      out << "49=";
      WRITE_RTRIM(out, firm);
      out << '\1';
    }
    if (display != ' ') out << "9140=" << display << '\1';
    if (capacity != ' ') out << "47=" << capacity << '\1';
    if (sweep == 'Y') out << "18=f\1"; // FLITE
    if (minQty > 0) out << "110=" << minQty << '\1';
    if (cross != ' ') out << "9355=" << cross << '\1'; // FLITE
    //if (customer != ' ') out << "20006=" << customer << '\1'; // FLITE
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t); 
    out << "0=" << t.tv_sec << '.' << t.tv_nsec << '\1';
  }

  void hton()
  {
    shares = htonl(shares);
    price = htonl(price);
    tif = htonl(tif);
    if (minQty) minQty = htonl(minQty);
  }

  void ntoh()
  {
    shares = ntohl(shares);
    price = ntohl(price);
    tif = ntohl(tif);
    if (minQty) minQty = ntohl(minQty);
  }
} packed;
static_assert(sizeof(OrderMsg)==48, "sizeof(OrderMsg)!=48");

struct ReplaceMsg : public Message
{
  static const char TYPE = 'U';
  char oldid[14];
  char newid[14]; 
  int shares;
  int price;
  int tif;
  char display;
  char sweep; // intermarket sweep eligibility
  int minQty;

  ReplaceMsg(const std::string& oldid, const std::string& newid, int shares, int price, char display= ' ')
    : Message(TYPE), shares(shares), price(price), tif(99998), 
      display(display), sweep('N'), minQty(0)
  {
    R_PAD_STR(this->oldid, oldid);
    R_PAD_STR(this->newid, newid);
  }

  void write(std::ostream& out)
  {
    out << "35=" << "G" << '\1';

    out << "41=";
    WRITE_RTRIM(out, oldid);
    out << '\1';

    out << "11=";
    WRITE_RTRIM(out, newid);
    out << '\1';

    out << "38=" << shares << '\1';
    if (price) out << "44=" << (price/10000) << '.' << std::setfill('0') << std::setw(4) << (price%10000) << '\1';
    out << "59=" << tif << '\1';
    if (display != ' ') out << "9140=" << display << '\1';
    if (sweep == 'Y') out << "18=f\1"; // FLITE
    if (minQty > 0) out << "110=" << minQty << '\1';
  }

  void hton()
  {
    shares = htonl(shares);
    price = htonl(price);
    tif = htonl(tif);
    if (minQty) minQty = htonl(minQty);
  }

  void ntoh()
  {
    shares = ntohl(shares);
    price = ntohl(price);
    tif = ntohl(tif);
    if (minQty) minQty = ntohl(minQty);
  }
} packed;
static_assert(sizeof(ReplaceMsg)==47, "sizeof(ReplaceMsg)!=47");

struct CancelMsg : public Message
{
  static const char TYPE = 'X';
  char id[14];
  int shares; // 0 means cancel all

  CancelMsg(const std::string& id, int shares=0)
    : Message(TYPE), shares(shares)
  {
    R_PAD_STR(this->id, id);
  }

  void write(std::ostream& out)
  {
    out << "35=" << "F" << '\1';

    out << "11=";
    WRITE_RTRIM(out, id);
    out << '\1';

    if (shares) out << "38=" << shares << '\1';
  }

  void hton()
  {
    if (shares) shares = htonl(shares);
  }

  void ntoh()
  {
    if (shares) shares = ntohl(shares);
  }
} packed;
static_assert(sizeof(CancelMsg)==19, "sizeof(CancelMsg)!=19");

struct ModifyMsg : public Message
{
  static const char TYPE = 'M';
  char id[14];
  char side; // Only following transitions allowed: S->T, S->E, E->T, E->S, T->E, T->S
  int shares; // 0 means cancel all

  ModifyMsg(const std::string& id, char side, int shares)
    : Message(TYPE), side(side), shares(shares)
  {
    R_PAD_STR(this->id, id);
  }

  void write(std::ostream& out)
  {
    out << "35=" << "G" << '\1';

    out << "11=";
    WRITE_RTRIM(out, id);
    out << '\1';

    writeSide(out, side);
    out << "38=" << shares << '\1';
  }

  void hton()
  {
    if (shares) shares = htonl(shares);
  }

  void ntoh()
  {
    if (shares) shares = ntohl(shares);
  }
} packed;
static_assert(sizeof(ModifyMsg)==20, "sizeof(ModifyMsg)!=20");

struct SysMsg : public Message
{
  static const char TYPE = 'S';
  uint64_t tm;
  char evt;

  void ntoh()
  {
    tm = ntohll(tm);
  }

  void write(std::ostream& out)
  {
    out << "35=" << TYPE << '\1';
    out << "60=" << tm << '\1'; // TransactTime
    out << "58=" << evt << '\1';
  }
} packed;
static_assert(sizeof(SysMsg)==10, "sizeof(SysMsg)!=10");

struct AcceptedMsg : public Message
{
  static const char TYPE = 'A';
  uint64_t tm;
  char id[14];
  char side; // B: buy, S: sell, T: sell short, E: sell short exempt
  int shares;
  char symbol[8];
  int price;
  int tif;
  char firm[4];
  char display;
  uint64_t ref; // order reference number / order id
  char capacity;
  char sweep; // intermarket sweep eligibility
  int minQty;
  char cross; // cross type
  char state; // L: order Live, D: Order Dead, order dead means accepted but automatically canceled
  char bbo; // BBO Weight indicator

  AcceptedMsg(const OrderMsg& o)
    : Message(TYPE), tm(0), side(o.side), shares(o.shares), price(o.price), tif(o.tif), 
      display(o.display), ref(0), capacity(o.capacity), sweep(o.sweep), 
      minQty(o.minQty), cross(o.cross), state('L'), bbo(' ')
  {
    strncpy(this->id, o.id, sizeof(o.id));
    strncpy(this->symbol, o.symbol, sizeof(o.symbol));
    strncpy(this->firm, o.firm, sizeof(o.firm));
  }

  bool isDead() const { return state == 'D'; }

  void write(std::ostream& out)
  {
    out << "35=" << "8" << '\1';

    out << "60=" << tm << '\1'; // TransactTime

    out << "11=";
    WRITE_RTRIM(out, id);
    out << '\1';
    writeSide(out, side);
    out << "38=" << shares << '\1';

    out << "55=";
    WRITE_RTRIM(out, symbol);
    out << '\1';

    if (price) out << "44=" << (price/10000) << '.' << std::setfill('0') << std::setw(4) << (price%10000) << '\1';
    out << "59=" << tif << '\1';
    if (*firm != ' ') { // similar to SenderCompID 
      out << "49=";
      WRITE_RTRIM(out, firm);
      out << '\1';
    }
    if (display != ' ') out << "9140=" << display << '\1';
    out << "37=" << ref << '\1'; // OrderID
    if (capacity != ' ') out << "47=" << capacity << '\1';
    if (sweep == 'Y') out << "18=f\1"; // FLITE
    if (minQty > 0) out << "110=" << minQty << '\1';
    if (cross != ' ') out << "9355=" << cross << '\1'; // FLITE
    out << "150=" << (state == 'D' ? '4' : '0') << '\1'; // ExecType
    if (bbo != ' ') out << "9883=" << bbo << '\1'; // FLITE
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t); 
    out << "0=" << t.tv_sec << '.' << t.tv_nsec << '\1';
  }

  void ntoh()
  {
    tm = ntohll(tm);
    shares = ntohl(shares);
    price = ntohl(price);
    tif = ntohl(tif);
    ref = ntohll(ref);
    if (minQty) minQty = ntohl(minQty);
  }

  void hton()
  {
    tm = htonll(tm);
    shares = htonl(shares);
    price = htonl(price);
    tif = htonl(tif);
    ref = htonll(ref);
    if (minQty) minQty = htonl(minQty);
  }
} packed;
static_assert(sizeof(AcceptedMsg)==66, "sizeof(AcceptedMsg)!=66");

struct ReplacedMsg : public Message
{
  static const char TYPE = 'U';
  uint64_t tm;
  char newid[14]; 
  char side;
  int shares;
  char symbol[8];
  int price;
  int tif;
  char firm[4];
  char display;
  uint64_t ref; // order reference number / order id
  char capacity;
  char sweep; // intermarket sweep eligibility
  int minQty;
  char cross; // cross type
  char state;
  char oldid[14];
  char bbo;

  bool isDead() const { return state == 'D'; }

  void write(std::ostream& out)
  {
    out << "35=" << "8" << '\1';

    out << "60=" << tm << '\1'; // TransactTime

    out << "11=";
    WRITE_RTRIM(out, newid);
    out << '\1';

    out << "54=" << side << '\1';
    out << "38=" << shares << '\1';

    out << "55=";
    WRITE_RTRIM(out, symbol);
    out << '\1';

    if (price) out << "44=" << (price/10000) << '.' << std::setfill('0') << std::setw(4) << (price%10000) << '\1';
    out << "59=" << tif << '\1';
    if (*firm != ' ') { // similar to SenderCompID
      out << "49=";
      WRITE_RTRIM(out, firm);
      out << '\1';
    }
    if (display != ' ') out << "9140=" << display << '\1';
    out << "37=" << ref << '\1'; // OrderID
    if (capacity != ' ') out << "47=" << capacity << '\1';
    if (sweep == 'Y') out << "18=f\1"; // FLITE
    if (minQty > 0) out << "110=" << minQty << '\1';
    if (cross != ' ') out << "9355=" << cross << '\1'; // FLITE
    out << "150=" << (state == 'D' ? '4' : '5') << '\1'; // ExecType, if should use 35=9 for dead state ?
    
    out << "41=";
    WRITE_RTRIM(out, oldid);
    out << '\1';
    if (bbo != ' ') out << "9883=" << bbo << '\1'; // FLITE
  }

  void ntoh()
  {
    tm = ntohll(tm);
    shares = ntohl(shares);
    price = ntohl(price);
    tif = ntohl(tif);
    ref = ntohll(ref);
    if (minQty) minQty = ntohl(minQty);
  }
} packed;
static_assert(sizeof(ReplacedMsg)==80, "sizeof(ReplacedMsg)!=80");

struct CanceledMsg : public Message
{
  static const char TYPE = 'C';
  uint64_t tm;
  char id[14];
  int canceledShares;
  char reason;

  CanceledMsg(const CancelMsg& o) : Message(TYPE), tm(0), reason(' ') 
  {
    memcpy(id, o.id, sizeof id);
  }

  void write(std::ostream& out)
  {
    out << "35=" << "8" << '\1';

    out << "60=" << tm << '\1'; // TransactTime

    out << "11=";
    WRITE_RTRIM(out, id);
    out << '\1';

    if (canceledShares) out << "38=" << canceledShares << '\1';
    out << "150=4\1";
    if (reason != ' ') out << "58=" << reason << '\1';
  }

  void ntoh()
  {
    tm = ntohll(tm);
    canceledShares = ntohl(canceledShares);
  }

  void hton()
  {
    tm = htonll(tm);
    canceledShares = htonl(canceledShares);
  }

} packed;
static_assert(sizeof(CanceledMsg)==28, "sizeof(CanceledMsg)!=28");

struct AIQCanceledMsg : public Message
{
  static const char TYPE = 'D';
  uint64_t tm;
  char id[14];
  int canceledShares;
  char reason;
  int execShares;
  int execPx;
  char liquidity;

  void write(std::ostream& out)
  {
    out << "35=" << "8" << '\1';

    out << "60=" << tm << '\1'; // TransactTime

    out << "11=";
    WRITE_RTRIM(out, id);
    out << '\1';

    if (canceledShares) out << "38=" << canceledShares << '\1';
    if (reason != ' ') out << "58=" << reason << '\1';
    if (execShares) out << "32=" << execShares << '\1'; // LastQty
    if (execPx) out << "31=" << (execPx/10000) << '.' << std::setfill('0') << std::setw(4) << (execPx%10000) << '\1'; // LastPx
    out << "150=4\1";
    if (liquidity != ' ') out << "9882=" << liquidity << '\1'; // FLITE
  }

  void ntoh()
  {
    tm = ntohll(tm);
    canceledShares = ntohl(canceledShares);
    execShares = ntohl(execShares);
    execPx = ntohl(execPx);
  }
} packed;
static_assert(sizeof(AIQCanceledMsg)==37, "sizeof(AIQCanceledMsg)!=37");

struct ExecMsg : public Message
{
  static const char TYPE = 'E';
  uint64_t tm;
  char id[14];
  int execShares;
  int execPx;
  char liquidity;
  uint64_t matchNum; // buy side and sell side share the same match number

  void write(std::ostream& out)
  {
    out << "35=" << "8" << '\1';

    out << "60=" << tm << '\1'; // TransactTime

    out << "11=";
    WRITE_RTRIM(out, id);
    out << '\1';

    if (execShares) out << "32=" << execShares << '\1'; // LastQty
    if (execPx) out << "31=" << (execPx/10000) << '.' << std::setfill('0') << std::setw(4) << (execPx%10000) << '\1'; // LastPx
    out << "150=1\1"; // partial fill, ouch has no 150=2
    if (liquidity != ' ') out << "9882=" << liquidity << '\1'; // FLITE
    out << "17=" << matchNum << '\1'; // ExecID, need check side to assue unique in case we cross ourself, same as in FLITE
    out << "20=0\1"; // ExecTransType=NEW
  }

  void ntoh()
  {
    tm = ntohll(tm);
    execShares = ntohl(execShares);
    execPx = ntohl(execPx);
    matchNum = ntohll(matchNum);
  }
} packed;
static_assert(sizeof(ExecMsg)==40, "sizeof(ExecMsg)!=40");

struct BrokenTradeMsg : public Message
{
  static const char TYPE = 'B';
  uint64_t tm;
  char id[14];
  uint64_t matchNum;
  char reason;

  void write(std::ostream& out)
  {
    out << "35=" << "8" << '\1';

    out << "60=" << tm << '\1'; // TransactTime

    out << "11=";
    WRITE_RTRIM(out, id);
    out << '\1';

    out << "150=1\1"; // partial fill, ouch has no 150=2
    out << "17=" << matchNum << '\1'; // ExecID, need check side to assue unique in case we cross ourself, same as in FLITE
    if (reason != ' ') out << "58=" << reason << '\1';
    out << "20=1\1"; // ExecTransType=CANCEL
  }

  void ntoh()
  {
    tm = ntohll(tm);
    matchNum = ntohll(matchNum);
  }
} packed;
static_assert(sizeof(BrokenTradeMsg)==32, "sizeof(BrokenTradeMsg)!=32");

struct RejectedMsg : public Message
{
  static const char TYPE = 'J';
  uint64_t tm;
  char id[14];
  char reason;

  void write(std::ostream& out)
  {
    out << "35=" << "8" << '\1';

    out << "60=" << tm << '\1'; // TransactTime

    out << "11=";
    WRITE_RTRIM(out, id);
    out << '\1';

    if (reason != ' ') out << "58=" << reason << '\1';
    out << "150=8\1";  // for replace rejected, FIX use 35=9 and CxlRejResponseTo=2
                       // here we do not know if it is replace reject
  }

  void ntoh()
  {
    tm = ntohll(tm);
  }
} packed;
static_assert(sizeof(RejectedMsg)==24, "sizeof(RejectedMsg)!=24");

struct CancelPendingMsg : public Message
{
  static const char TYPE = 'P';
  uint64_t tm;
  char id[14];

  void write(std::ostream& out)
  {
    out << "35=" << "8" << '\1';

    out << "60=" << tm << '\1'; // TransactTime

    out << "11=";
    WRITE_RTRIM(out, id);
    out << '\1';

    out << "150=6\1";  
  }

  void ntoh()
  {
    tm = ntohll(tm);
  }
} packed;
static_assert(sizeof(CancelPendingMsg)==23, "sizeof(CancelPendingMsg)!=23");

struct CancelRejectMsg : public Message
{
  static const char TYPE = 'I';
  uint64_t tm;
  char id[14];

  void write(std::ostream& out)
  {
    out << "35=" << "9" << '\1';

    out << "60=" << tm << '\1'; // TransactTime

    out << "11=";
    WRITE_RTRIM(out, id);
    out << '\1';

    out << "434=1\1"; // CxlRejResponseTo
  }

  void ntoh()
  {
    tm = ntohll(tm);
  }
} packed;
static_assert(sizeof(CancelRejectMsg)==23, "sizeof(CancelRejectMsg)!=23");

struct PriorityMsg : public Message
{
  static const char TYPE = 'T';
  uint64_t tm;
  char id[14]; 
  int price;
  char display;
  uint64_t ref; // order reference number / order id

  void write(std::ostream& out)
  {
    out << "35=" << TYPE << '\1';

    out << "60=" << tm << '\1'; // TransactTime

    out << "11=";
    WRITE_RTRIM(out, id);
    out << '\1';

    if (price) out << "44=" << (price/10000) << '.' << std::setfill('0') << std::setw(4) << (price%10000) << '\1';
    if (display != ' ') out << "9140=" << display << '\1';
    out << "37=" << ref << '\1'; // OrderID
  }

  void ntoh()
  {
    tm = ntohll(tm);
    price = ntohl(price);
    ref = ntohll(ref);
  }
} packed;
static_assert(sizeof(PriorityMsg)==36, "sizeof(PriorityMsg)!=36");

struct ModifiedMsg : public Message
{
  static const char TYPE = 'M';
  uint64_t tm;
  char id[14];
  char side;
  int shares;

  void write(std::ostream& out)
  {
    out << "35=" << "8" << '\1';

    out << "60=" << tm << '\1'; // TransactTime

    out << "11=";
    WRITE_RTRIM(out, id);
    out << '\1';

    out << "150=5\1"; // ExecType
    writeSide(out, side);
    out << "38=" << shares << '\1';
  }

  void ntoh()
  {
    tm = ntohll(tm);
    shares = ntohl(shares);
  }
} packed;
static_assert(sizeof(ModifiedMsg)==28, "sizeof(ModifiedMsg)!=28");

}

#endif
