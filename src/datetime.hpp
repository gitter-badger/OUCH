#ifndef OUCH_DATETIME_H
#define OUCH_DATETIME_H

#include "util.hpp"

#include <sys/time.h>
#include <ctime>

namespace OUCH
{
/// Date and Time stored as a Julian day number and number of
/// milliseconds since midnight.  Does not perform any timezone
/// calculations.  All magic numbers and related calculations
/// have been taken from:
///
/// http://www.faqs.org/faqs/calendars.faq
/// http://scienceworld.wolfram.com/astronomy/JulianDate.html
/// http://scienceworld.wolfram.com/astronomy/GregorianCalendar.html
/// http://scienceworld.wolfram.com/astronomy/Weekday.html
///
struct DateTime 
{
  int m_date;
  int m_time;

  /// Magic numbers
  enum 
  {
    SECONDS_PER_DAY = 86400,
    SECONDS_PER_HOUR = 3600,
    SECONDS_PER_MIN = 60,
    MINUTES_PER_HOUR = 60,

    MILLIS_PER_DAY = 86400000,
    MILLIS_PER_HOUR = 3600000,
    MILLIS_PER_MIN = 60000,
    MILLIS_PER_SEC = 1000,

    // time_t epoch (1970-01-01) as a Julian date
    JULIAN_19700101 = 2440588
  };

  /// Default constructor - initializes to zero
  DateTime () : m_date (0), m_time (0) {}

  /// Construct from a Julian day number and time in millis
  DateTime (int date, int time) : m_date (date), m_time (time) {}

  /// Construct from the specified components
  DateTime(int year, int month, int day,
            int hour, int minute, int second, int millis) 
  {
    m_date = julianDate(year, month, day);
    m_time = makeHMS(hour, minute, second, millis);
  }

  virtual ~DateTime() {}

  /// Return the year portion of the date
  inline int getYear() const 
  {
    int y, m, d;
    getYMD(y, m, d);
    return y;
  }
    
  /// Return the month (1-12) portion of the date
  inline int getMonth() const 
  {
    int y, m, d;
    getYMD(y, m, d);
    return m;
  }

  /// Return the day of the month portion of the date
  inline int getDay() const 
  {
    int y, m, d;
    getYMD(y, m, d);
    return d;
  }

  /// Another name for the day of the month.  Bad name, but used
  /// because of the legacy UtcTimeStamp interface
  inline int getDate() const { return getDay(); }

  /// Return the internal julian date
  inline int getJulianDate() const { return m_date; }

  /// Return the hour portion of the time (0-23)
  inline int getHour() const 
  {
    return m_time / MILLIS_PER_HOUR;
  }

  /// Return the minute portion of the time (0-59)
  inline int getMinute() const 
  {
    return (m_time / MILLIS_PER_MIN) % MINUTES_PER_HOUR;
  }

  /// Return the second portion of the time (0-59)
  inline int getSecond() const 
  {
    return (m_time / MILLIS_PER_SEC) % SECONDS_PER_MIN;
  }

  /// Return the millisecond portion of the time
  inline int getMillisecond() const 
  {
    return m_time % MILLIS_PER_SEC;
  }

  /// Load the referenced values with the year, month and day
  /// portions of the date in a single operation
  inline void getYMD(int& year, int& month, int& day) const 
  {
    getYMD(m_date, year, month, day);
  }

  /// Load the referenced values with the hour, minute, second and
  /// millisecond portions of the time in a single operation
  inline void getHMS(int& hour, int& minute, int& second, int& millis) const 
  {
    int ticks = m_time / MILLIS_PER_SEC;
    hour = ticks / SECONDS_PER_HOUR;
    minute = (ticks / SECONDS_PER_MIN) % MINUTES_PER_HOUR;
    second = ticks % SECONDS_PER_MIN;
    millis = m_time % MILLIS_PER_SEC;
  }

  /// Calculate the weekday of the date (Sunday is 1, Saturday is 7)
  inline int getWeekDay() const 
  {
    int Y, M, D;
    getYMD (Y, M, D);
    int m = M >= 3 ? M - 2 : M + 10;
    int Yprime = M >= 3 ? Y : Y - 1;
    int y = Yprime % 100;
    int c = Yprime / 100;
    int wd = (D + int (2.6 * m - 0.2) + y + int (y / 4) + int (c / 4) -
              (2 * c)) % 7;
    return 1 + (wd < 0 ? 7 + wd : wd);
  }

  /// Convert the DateTime to a time_t.  Note that this operation
  /// can overflow on 32-bit platforms when we go beyond year 2038.
  inline time_t getTimeT() const 
  {
    return (SECONDS_PER_DAY * (m_date - JULIAN_19700101) +
            m_time / MILLIS_PER_SEC);
  }

  /// Convert the DateTime to a struct tm which is in UTC
  tm getTmUtc() const 
  {
    int year, month, day;
    int hour, minute, second, millis;
    tm result = { 0 };

    getYMD(year, month, day);
    getHMS(hour, minute, second, millis);

    result.tm_year = year - 1900;
    result.tm_mon = month - 1;
    result.tm_mday = day;
    result.tm_hour = hour;
    result.tm_min = minute;
    result.tm_sec = second;
    result.tm_isdst = -1;

    return result;
  }

  /// Set the date portion of the DateTime
  void setYMD(int year, int month, int day) 
  {
    m_date = julianDate(year, month, day);
  }

  /// Set the time portion of the DateTime
  void setHMS(int hour, int minute, int second, int millis)
  {
    m_time = makeHMS(hour, minute, second, millis);
  }

  /// Set the hour portion of the time
  void setHour(int hour)
  {
    int old_hour, min, sec, millis;
    getHMS(old_hour, min, sec, millis);
    setHMS(hour, min, sec, millis);
  }

  /// Set the minute portion of the time
  void setMinute(int min)
  {
    int hour, old_min, sec, millis;
    getHMS(hour, old_min, sec, millis);
    setHMS(hour, min, sec, millis);
  }

  /// Set the seconds portion of the time
  void setSecond(int sec)
  {
    int hour, min, old_sec, millis;
    getHMS(hour, min, old_sec, millis);
    setHMS(hour, min, sec, millis);
  }

  /// Set the millisecond portion of the time
  void setMillisecond(int millis)
  {
    int hour, min, sec, old_millis;
    getHMS(hour, min, sec, old_millis);
    setHMS(hour, min, sec, millis);
  }

  /// Clear the date portion of the DateTime
  void clearDate() 
  {
    m_date = 0;
  }

  /// Clear the time portion of the DateTime
  void clearTime() 
  {
    m_time = 0;
  }

  /// Set the internal date and time members
  void set(int date, int time) { m_date = date; m_time = time; }

  /// Initialize from another DateTime
  void set(const DateTime& other) 
  {
    m_date = other.m_date;
    m_time = other.m_time;
  }

  /// Add a number of seconds to this
  void operator+=(int seconds) 
  {
    int d = seconds / SECONDS_PER_DAY;
    int s = seconds % SECONDS_PER_DAY;

    m_date += d;
    m_time += s * MILLIS_PER_SEC;

    if(m_time > MILLIS_PER_DAY)
    {
      m_date++;
      m_time %= MILLIS_PER_DAY;
    }
    else if(m_time < 0)
    {
      m_date--;
      m_time += MILLIS_PER_DAY;
    }
  }

  /// Helper method to convert a broken down time to a number of
  /// milliseconds since midnight
  static int makeHMS(int hour, int minute, int second, int millis)
  {
    return MILLIS_PER_SEC * (SECONDS_PER_HOUR * hour +
                             SECONDS_PER_MIN * minute +
                             second) + millis;
  }

  /// Return the current wall-clock time as a utc DateTime
  static DateTime nowUtc()
  {
    struct timeval tv; 
    gettimeofday (&tv, 0); 
    return fromUtcTimeT(tv.tv_sec, tv.tv_usec / 1000);
  }

  /// Return the current wall-clock time as a local DateTime
  static DateTime nowLocal()
  {
    struct timeval tv; 
    gettimeofday (&tv, 0); 
    return fromLocalTimeT(tv.tv_sec, tv.tv_usec / 1000);
  }

  /// Convert a time_t and optional milliseconds to a DateTime
  static DateTime fromUtcTimeT(time_t t, int millis = 0) 
  {
    struct tm tm;
    gmtime_r(&t, &tm);
    return fromTm(tm, millis);
  }

  static DateTime fromLocalTimeT(time_t t, int millis = 0)
  {
    struct tm tm;
    localtime_r(&t, &tm);
    return fromTm(tm, millis);
  }

  /// Convert a tm and optional milliseconds to a DateTime.  \note
  /// the tm structure is assumed to contain a date specified in UTC
  static DateTime fromTm(const tm& tm, int millis = 0)
  {
    return DateTime (julianDate(tm.tm_year + 1900, tm.tm_mon + 1,
                                 tm.tm_mday),
                     makeHMS(tm.tm_hour, tm.tm_min, tm.tm_sec, millis));
  }

  /// Helper method to calculate a Julian day number.
  static int julianDate(int year, int month, int day)
  {
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    return (day + int ((153 * m + 2) / 5) + y * 365 +
            int (y / 4) - int (y / 100) + int (y / 400) - 32045);
  }

  /// Convert a Julian day number to a year, month and day
  static void getYMD(int jday, int& year, int& month, int& day)
  {
    int a = jday + 32044;
    int b = (4 * a + 3) / 146097;
    int c = a - int ((b * 146097) / 4);
    int d = (4 * c + 3) / 1461;
    int e = c - int ((1461 * d) / 4);
    int m = (5 * e + 2) / 153;
    day = e - int ((153 * m + 2) / 5) + 1;
    month = m + 3 - 12 * int (m / 10);
    year = b * 100 + d - 4800 + int (m / 10);
  }
};

inline bool operator==(const DateTime& lhs, const DateTime& rhs)
{
  return lhs.m_date == rhs.m_date && lhs.m_time == rhs.m_time;
}

inline bool operator!=(const DateTime& lhs, const DateTime& rhs)
{
  return !(lhs == rhs);
}

inline bool operator<(const DateTime& lhs, const DateTime& rhs)
{
  if(lhs.m_date < rhs.m_date)
    return true;
  else if(lhs.m_date > rhs.m_date)
    return false;
  else if(lhs.m_time < rhs.m_time)
    return true;
  return false;
}

inline bool operator>(const DateTime& lhs, const DateTime& rhs)
{
  return !(lhs == rhs || lhs < rhs);
}

inline bool operator<=(const DateTime& lhs, const DateTime& rhs)
{
  return lhs == rhs || lhs < rhs;
}

inline bool operator>=(const DateTime& lhs, const DateTime& rhs)
{
  return lhs == rhs || lhs > rhs;
}

/// Calculate the difference between two DateTime values and return
/// the result as a number of seconds
inline int operator-(const DateTime& lhs, const DateTime& rhs)
{
  return (DateTime::SECONDS_PER_DAY * (lhs.m_date - rhs.m_date) +
          // Truncate the millis before subtracting
          lhs.m_time / 1000 - rhs.m_time / 1000);
}

/// Date and Time represented in UTC.
class UtcTimeStamp : public DateTime
{
public:
  /// Defaults to the current date and time
  UtcTimeStamp()
  : DateTime(DateTime::nowUtc()) {}

  /// Defaults to the current date
  UtcTimeStamp(int hour, int minute, int second, int millisecond = 0)
  : DateTime(DateTime::nowUtc())
  {
    setHMS(hour, minute, second, millisecond);
  }

  UtcTimeStamp(int hour, int minute, int second,
                int date, int month, int year)
  : DateTime(year, month, date, hour, minute, second, 0) {}

  UtcTimeStamp(int hour, int minute, int second, int millisecond,
                int date, int month, int year)
  : DateTime(year, month, date, hour, minute, second, millisecond) {}

  explicit UtcTimeStamp(time_t time, int millisecond = 0)
  : DateTime(fromUtcTimeT (time, millisecond)) {}

  UtcTimeStamp(const tm* time, int millisecond = 0)
  : DateTime(fromTm (*time, millisecond)) {}

  void setCurrent() 
  {
    set(DateTime::nowUtc());
  }
};

/// Date and Time represented in local time.
class LocalTimeStamp : public DateTime
{
public:
  /// Defaults to the current date and time
  LocalTimeStamp()
  : DateTime(DateTime::nowLocal()) {}

  /// Defaults to the current date
  LocalTimeStamp(int hour, int minute, int second, int millisecond = 0)
  : DateTime(DateTime::nowLocal())
  {
    setHMS(hour, minute, second, millisecond);
  }

  LocalTimeStamp(int hour, int minute, int second,
                int date, int month, int year)
  : DateTime(year, month, date, hour, minute, second, 0) {}

  LocalTimeStamp(int hour, int minute, int second, int millisecond,
                int date, int month, int year)
  : DateTime(year, month, date, hour, minute, second, millisecond) {}

  explicit LocalTimeStamp(time_t time, int millisecond = 0)
  : DateTime(fromLocalTimeT (time, millisecond)) {}

  LocalTimeStamp(const tm* time, int millisecond = 0)
  : DateTime(fromTm (*time, millisecond)) {}

  void setCurrent() 
  {
    set(DateTime::nowLocal());
  }
};

/// Time only represented in UTC.
class UtcTimeOnly : public DateTime
{
public:
  /// Defaults to the current time
  UtcTimeOnly()
  {
    setCurrent();
  }

  UtcTimeOnly(const DateTime& val)
  : DateTime(val)
  {
    clearDate();
  }

  UtcTimeOnly(int hour, int minute, int second, int millisecond = 0)
  {
    setHMS(hour, minute, second, millisecond);
  }

  explicit UtcTimeOnly(time_t time, int millisecond = 0)
  : DateTime(fromUtcTimeT (time, millisecond))
  {
    clearDate();
  }

  UtcTimeOnly(const tm* time, int millisecond = 0)
  : DateTime(fromTm (*time, millisecond))
  {
    clearDate();
  }

  /// Set to the current time.
  void setCurrent()
  {
    DateTime d = nowUtc();
    m_time = d.m_time;
  }
};

/// Time only represented in local time.
class LocalTimeOnly : public DateTime
{
public:
  /// Defaults to the current time
  LocalTimeOnly()
  {
    setCurrent();
  }

  LocalTimeOnly(const DateTime& val)
  : DateTime(val)
  {
    clearDate();
  }

  LocalTimeOnly(int hour, int minute, int second, int millisecond = 0)
  {
    setHMS(hour, minute, second, millisecond);
  }

  explicit LocalTimeOnly(time_t time, int millisecond = 0)
  : DateTime(fromLocalTimeT (time, millisecond))
  {
    clearDate();
  }

  LocalTimeOnly(const tm* time, int millisecond = 0)
  : DateTime(fromTm (*time, millisecond))
  {
    clearDate();
  }

  /// Set to the current time.
  void setCurrent()
  {
    DateTime d = nowLocal();
    m_time = d.m_time;
  }
};

/// Date only represented in UTC.
class UtcDate : public DateTime
{
public:
  /// Defaults to the current date
  UtcDate()
  {
    setCurrent();
  }

  UtcDate(const DateTime& val)
  : DateTime(val)
  {
    clearTime();
  }

  UtcDate(int date, int month, int year)
  : DateTime(year, month, date, 0, 0, 0, 0) {}

  UtcDate(long sec)
  : DateTime(sec / DateTime::SECONDS_PER_DAY, 0) {}

  UtcDate(const tm* time)
  : DateTime(fromTm (*time))
  {
    clearTime();
  }

  /// Set to the current time.
  void setCurrent()
  {
    DateTime d = nowUtc();
    m_date = d.m_date;
  }
};

/// Date only represented in local time.
class LocalDate : public DateTime
{
public:
  /// Defaults to the current date
  LocalDate()
  {
    setCurrent();
  }

  LocalDate(const DateTime& val)
  : DateTime(val)
  {
    clearTime();
  }

  LocalDate(int date, int month, int year)
  : DateTime(year, month, date, 0, 0, 0, 0) {}

  LocalDate(long sec)
  : DateTime(sec / DateTime::SECONDS_PER_DAY, 0) {}

  LocalDate(const tm* time)
  : DateTime(fromTm (*time))
  {
    clearTime();
  }

  /// Set to the current time.
  void setCurrent()
  {
    DateTime d = nowLocal();
    m_date = d.m_date;
  }
};

template<class T>
inline char* integer_to_string(char* buf, const size_t len, T t)
{
  const bool isNegative = t < 0;
  char* p = buf + len;

  *--p = '\0';

  if(isNegative) {
    if(t == (std::numeric_limits<T>::min)()) {
      *--p = '0' + (char)((10-t%10)%10);
      t/=10;
    }
    t = -t;
    do {
      *--p = '0' + (char)(t % 10);
      t /= 10;
    } while (t > 0);
    *--p = '-';
  }
  else {
    do {
      *--p = '0' + (char)(t % 10);
      t /= 10;
    } while(t > 0);
  }
  return p;
}

template<class T>
inline char* integer_to_string_padded
(char* buf, const size_t len, T t,
 const size_t width = 0,
 const char paddingChar = '0')
{
  if(!width) return integer_to_string(buf, len, t);

  const bool isNegative = t < 0;
  char* p = buf + len;

  *--p = '\0';

  if(isNegative)
  {
    if(t == (std::numeric_limits<T>::min)()) {
      *--p = '0' + (char)((10 -t % 10) % 10);
      t/=10;
    }
    t=-t;
    do {
      *--p = '0' + (char)(t % 10);
      t /= 10;
    } while (t > 0);
    if(p > buf)
      *--p = '-';
  } else {
    do {
      *--p = '0' + (char)(t % 10);
      t /= 10;
    } while(t > 0);
  }
  const char* stop_p = buf + len - width - 1;
  if(stop_p < buf) stop_p = buf;
  while(p > stop_p)
    *--p = paddingChar;
  return p;
}

/// Converts a UtcTimeStamp to/from a string
struct UtcTimeStampConvertor
{
  static std::string convert(const UtcTimeStamp& value,
                              bool showMilliseconds = false)
  {
    char result[ 18+4 ];
    int year, month, day, hour, minute, second, millis;

    value.getYMD(year, month, day);
    value.getHMS(hour, minute, second, millis);

    integer_to_string_padded(result, 5, year, 4, '0');
    integer_to_string_padded(result + 4, 3, month, 2, '0');
    integer_to_string_padded(result + 6, 3, day, 2, '0');
    result[8]  = '-';
    integer_to_string_padded(result + 9, 3, hour, 2, '0');
    result[11] = ':';
    integer_to_string_padded(result + 12, 3, minute, 2, '0');
    result[14] = ':';
    integer_to_string_padded(result + 15, 3, second, 2, '0');

    if(showMilliseconds) {
      result[17] = '.';
      if(integer_to_string_padded(result + 18, 4, millis, 3, '0')
          != result + 18)
       return "";
    }

    return result;
  }

  static UtcTimeStamp convert(const std::string& value, bool calculateDays = false)
  {
    bool haveMilliseconds = false;

    switch(value.size()) {
      case 21: haveMilliseconds = true;
      case 17: break;
      default: die("wrong timestamp string: " + value);;
    }

    int i = 0;
    int c = 0;
    for(c = 0; c < 8; ++c)
      if(!isdigit(value[i++])) die("wrong timestamp string: " + value);
    if (value[i++] != '-') die("wrong timestamp string: " + value);
    for(c = 0; c < 2; ++c)
      if(!isdigit(value[i++])) die("wrong timestamp string: " + value);
    if(value[i++] != ':') die("wrong timestamp string: " + value);
    for(c = 0; c < 2; ++c)
      if(!isdigit(value[i++])) die("wrong timestamp string: " + value);
    if(value[i++] != ':') die("wrong timestamp string: " + value);
    for(c = 0; c < 2; ++c)
      if(!isdigit(value[i++])) die("wrong timestamp string: " + value);

    if(haveMilliseconds) {
      if(value[i++] != '.') die("wrong timestamp string: " + value);
      for(c = 0; c < 3; ++c)
	      if(!isdigit(value[i++])) die("wrong timestamp string: " + value);
    }

    int year, mon, mday, hour, min, sec, millis;

    i = 0;

    year = value[i++] - '0';
    year = 10 * year + value[i++] - '0';
    year = 10 * year + value[i++] - '0';
    year = 10 * year + value[i++] - '0';

    mon = value[i++] - '0';
    mon = 10 * mon + value[i++] - '0';
    if(mon < 1 || 12 < mon) die("wrong timestamp string: " + value);

    mday = value[i++] - '0';
    mday = 10 * mday + value[i++] - '0';
    if(mday < 1 || 31 < mday) die("wrong timestamp string: " + value);

    ++i; // skip '-'

    hour = value[i++] - '0';
    hour = 10 * hour + value[i++] - '0';
    // No check for >= 0 as no '-' are converted here
    if(23 < hour) die("wrong timestamp string: " + value);

    ++i; // skip ':'

    min = value[i++] - '0';
    min = 10 * min + value[i++] - '0';
    // No check for >= 0 as no '-' are converted here
    if(59 < min) die("wrong timestamp string: " + value);

    ++i; // skip ':'

    sec = value[i++] - '0';
    sec = 10 * sec + value[i++] - '0';

    // No check for >= 0 as no '-' are converted here
    if(60 < sec) die("wrong timestamp string: " + value);

    if(haveMilliseconds) {
      millis = (100 * (value[i+1] - '0')
                + 10 * (value[i+2] - '0')
                + (value[i+3] - '0'));
    } else
      millis = 0;

    return UtcTimeStamp(hour, min, sec, millis, mday, mon, year);
  }
};

}

#endif //OUCH_DATETIME_H
