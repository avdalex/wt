/*
 * Copyright (C) 2010 Emweb bvba, Kessel-Lo, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#include <map>
#include <boost/lexical_cast.hpp>
#include <stdio.h>

#ifdef WT_THREADED
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#endif // WT_THREADED

#include <boost/date_time/posix_time/posix_time.hpp>

#include "Wt/WBoostAny"
#include "Wt/WDate"
#include "Wt/WDateTime"
#include "Wt/WLocalDateTime"
#include "Wt/WException"
#include "Wt/WLocale"
#include "Wt/WTime"
#include "Wt/WWebWidget"

#include "WebUtils.h"

#ifdef WIN32
#define snprintf _snprintf
#endif

namespace Wt {

LOGGER("WAbstractItemModel");

  namespace Impl {

#ifdef WT_THREADED
boost::mutex registryMutex_;
#endif // WT_THREADED

typedef std::map<const std::type_info *,
		 boost::shared_ptr<AbstractTypeHandler> > TypeRegistryMap;

TypeRegistryMap typeRegistry_;

AbstractTypeHandler::AbstractTypeHandler()
{ }

AbstractTypeHandler::~AbstractTypeHandler()
{ }

void lockTypeRegistry()
{
#ifdef WT_THREADED
  registryMutex_.lock();
#endif // WT_THREADED
}

void unlockTypeRegistry()
{
#ifdef WT_THREADED
  registryMutex_.unlock();
#endif // WT_THREADED
}

AbstractTypeHandler *getRegisteredType(const std::type_info *type,
				       bool takeLock)
{
  if (takeLock)
    lockTypeRegistry();

  TypeRegistryMap::iterator i = typeRegistry_.find(type);

  AbstractTypeHandler *result = 0;

  if (i != typeRegistry_.end())
    result = i->second.get();

  if (takeLock)
    unlockTypeRegistry();

  return result;
}

void registerType(const std::type_info *type, AbstractTypeHandler *handler)
{
  typeRegistry_[type].reset(handler);
}

bool matchValue(const boost::any& value, const boost::any& query,
		WFlags<MatchFlag> flags)
{
  WFlags<MatchFlag> f = flags & MatchTypeMask;

  if ((f & MatchTypeMask) == MatchExactly) {
    if (query.type() == value.type() ||
	(query.type() == typeid(WString) &&
	 value.type() == typeid(std::string)) ||
	(query.type() == typeid(std::string) &&
	 value.type() == typeid(WString)))
      return asString(query) == asString(value);
    else
      return false;
  } else {
    std::string query_str = asString(query).toUTF8();
    std::string value_str = asString(value).toUTF8();

    switch (static_cast<int>(f)) {
    case MatchStringExactly:
      return boost::iequals(value_str, query_str);
    case MatchStringExactly | (int)MatchCaseSensitive:
      return boost::equals(value_str, query_str);

    case MatchStartsWith:
      return boost::istarts_with(value_str, query_str);
    case MatchStartsWith | (int)MatchCaseSensitive:
      return boost::starts_with(value_str, query_str);

    case MatchEndsWith:
      return boost::iends_with(value_str, query_str);
    case MatchEndsWith | (int)MatchCaseSensitive:
      return boost::ends_with(value_str, query_str);

    default:
      throw WException("Not yet implemented: WAbstractItemModel::match with "
		       "MatchFlags = "
		       + boost::lexical_cast<std::string>(flags));
    }
  }
}

std::string asJSLiteral(const boost::any& v, TextFormat textFormat)
{
  if (v.empty())
    return std::string("''");
  else if (v.type() == typeid(WString)) {
    WString s = boost::any_cast<WString>(v);

    bool plainText = false;
    if (textFormat == XHTMLText) {
      if (s.literal())
	plainText = !WWebWidget::removeScript(s);
    } else
      plainText = true;

    if (plainText && textFormat != XHTMLUnsafeText)
      s = WWebWidget::escapeText(s);

    return s.jsStringLiteral();
  } else if (v.type() == typeid(std::string)
	     || v.type() == typeid(const char *)) {
    WString s = v.type() == typeid(std::string) 
      ? WString::fromUTF8(boost::any_cast<std::string>(v))
      : WString::fromUTF8(boost::any_cast<const char *>(v));

    bool plainText;
    if (textFormat == XHTMLText)
      plainText = !WWebWidget::removeScript(s);
    else
      plainText = true;

    if (plainText && textFormat != XHTMLUnsafeText)
      s = WWebWidget::escapeText(s);

    return s.jsStringLiteral();
  } else if (v.type() == typeid(bool)) {
    bool b = boost::any_cast<bool>(v);
    return b ? "true" : "false";
  } else if (v.type() == typeid(WDate)) {
    const WDate& d = boost::any_cast<WDate>(v);

    return "new Date(" + boost::lexical_cast<std::string>(d.year())
      + ',' + boost::lexical_cast<std::string>(d.month() - 1)
      + ',' + boost::lexical_cast<std::string>(d.day())
      + ')';
  } else if (v.type() == typeid(WDateTime)) {
    const WDateTime& dt = boost::any_cast<WDateTime>(v);
    const WDate& d = dt.date();
    const WTime& t = dt.time();

    return "new Date(" + boost::lexical_cast<std::string>(d.year())
      + ',' + boost::lexical_cast<std::string>(d.month() - 1)
      + ',' + boost::lexical_cast<std::string>(d.day())
      + ',' + boost::lexical_cast<std::string>(t.hour())
      + ',' + boost::lexical_cast<std::string>(t.minute())
      + ',' + boost::lexical_cast<std::string>(t.second())
      + ',' + boost::lexical_cast<std::string>(t.msec())
      + ')';
  } else if (v.type() == typeid(WLocalDateTime)) {
    const WLocalDateTime& dt = boost::any_cast<WLocalDateTime>(v);
    const WDate& d = dt.date();
    const WTime& t = dt.time();

    return "new Date(" + boost::lexical_cast<std::string>(d.year())
      + ',' + boost::lexical_cast<std::string>(d.month() - 1)
      + ',' + boost::lexical_cast<std::string>(d.day())
      + ',' + boost::lexical_cast<std::string>(t.hour())
      + ',' + boost::lexical_cast<std::string>(t.minute())
      + ',' + boost::lexical_cast<std::string>(t.second())
      + ',' + boost::lexical_cast<std::string>(t.msec())
      + ')';
  }

#define ELSE_LEXICAL_ANY(TYPE) \
  else if (v.type() == typeid(TYPE)) \
    return boost::lexical_cast<std::string>(boost::any_cast<TYPE>(v))

  ELSE_LEXICAL_ANY(short);
  ELSE_LEXICAL_ANY(unsigned short);
  ELSE_LEXICAL_ANY(int);
  ELSE_LEXICAL_ANY(unsigned int);
  ELSE_LEXICAL_ANY(long);
  ELSE_LEXICAL_ANY(unsigned long);
  ELSE_LEXICAL_ANY(::int64_t);
  ELSE_LEXICAL_ANY(::uint64_t);
  ELSE_LEXICAL_ANY(long long);
  ELSE_LEXICAL_ANY(unsigned long long);
  ELSE_LEXICAL_ANY(float);
  ELSE_LEXICAL_ANY(double);

#undef ELSE_LEXICAL_ANY

  else {
    AbstractTypeHandler *handler = getRegisteredType(&v.type(), true);
    if (handler)
      return handler->asString(v, WString::Empty).jsStringLiteral();
    else {
      LOG_ERROR("unsupported type: '"<< v.type().name() << "'");
      return "''";
    }
  }
}

boost::any updateFromJS(const boost::any& v, std::string s)
{
  if (v.empty())
    return boost::any(s);
  else if (v.type() == typeid(WString))
    return boost::any(WString::fromUTF8(s));
  else if (v.type() == typeid(std::string))
    return boost::any(s);
  else if (v.type() == typeid(const char *))
    return boost::any(s);
  else if (v.type() == typeid(bool))
    return boost::any((s == "true" || s == "1") ? true : false);
  else if (v.type() == typeid(WDate))
    return boost::any(WDate::fromString(WString::fromUTF8(s),
					"ddd MMM d yyyy"));
  else if (v.type() == typeid(WDateTime))
    return boost::any(WDateTime::fromString(WString::fromUTF8(s),
					    "ddd MMM d yyyy HH:mm:ss"));
  else if (v.type() == typeid(WLocalDateTime))
    return boost::any(WLocalDateTime::fromString(WString::fromUTF8(s),
						 "ddd MMM d yyyy HH:mm:ss"));
#define ELSE_LEXICAL_ANY(TYPE) \
  else if (v.type() == typeid(TYPE)) \
    return boost::any(boost::lexical_cast<TYPE>(s))

  ELSE_LEXICAL_ANY(short);
  ELSE_LEXICAL_ANY(unsigned short);
  ELSE_LEXICAL_ANY(int);
  ELSE_LEXICAL_ANY(unsigned int);
  ELSE_LEXICAL_ANY(long);
  ELSE_LEXICAL_ANY(unsigned long);
  ELSE_LEXICAL_ANY(::int64_t);
  ELSE_LEXICAL_ANY(::uint64_t);
  ELSE_LEXICAL_ANY(long long);
  ELSE_LEXICAL_ANY(unsigned long long);
  ELSE_LEXICAL_ANY(float);
  ELSE_LEXICAL_ANY(double);

#undef ELSE_LEXICAL_ANY

  else {
    LOG_ERROR("unsupported type '" << v.type().name() << "'");
    return boost::any();
  }
}

int compare(const boost::any& d1, const boost::any& d2)
{
  const int UNSPECIFIED_RESULT = -1;

  /*
   * If the types are the same then we use std::operator< on that type
   * otherwise we compare lexicographically
   */
  if (!d1.empty())
    if (!d2.empty()) {
      if (d1.type() == d2.type()) {
	if (d1.type() == typeid(bool))
	  return static_cast<int>(boost::any_cast<bool>(d1))
	    - static_cast<int>(boost::any_cast<bool>(d2));

#define ELSE_COMPARE_ANY(TYPE)				\
	else if (d1.type() == typeid(TYPE)) {		\
	  TYPE v1 = boost::any_cast<TYPE>(d1);		\
	  TYPE v2 = boost::any_cast<TYPE>(d2);		\
	  return v1 == v2 ? 0 : (v1 < v2 ? -1 : 1);	\
        }

	ELSE_COMPARE_ANY(WString)
	ELSE_COMPARE_ANY(std::string)
	ELSE_COMPARE_ANY(WDate)
	ELSE_COMPARE_ANY(WDateTime)
	ELSE_COMPARE_ANY(WLocalDateTime)
	ELSE_COMPARE_ANY(boost::posix_time::ptime)
	ELSE_COMPARE_ANY(boost::posix_time::time_duration)
	ELSE_COMPARE_ANY(WTime)
	ELSE_COMPARE_ANY(short)
	ELSE_COMPARE_ANY(unsigned short)
	ELSE_COMPARE_ANY(int)
	ELSE_COMPARE_ANY(unsigned int)
	ELSE_COMPARE_ANY(long)
	ELSE_COMPARE_ANY(unsigned long)
	ELSE_COMPARE_ANY(::int64_t)
	ELSE_COMPARE_ANY(::uint64_t)
	ELSE_COMPARE_ANY(long long)
	ELSE_COMPARE_ANY(unsigned long long)
	ELSE_COMPARE_ANY(float)
	ELSE_COMPARE_ANY(double)

#undef ELSE_COMPARE_ANY
	else {
	  AbstractTypeHandler *handler = getRegisteredType(&d1.type(), true);
	  if (handler)
	    return handler->compare(d1, d2);
	  else {
	    LOG_ERROR("unsupported type '" << d1.type().name() << "'");
	    return 0;
	  }
	}
      } else {
	WString s1 = asString(d1);
	WString s2 = asString(d2);

	return s1 == s2 ? 0 : (s1 < s2 ? -1 : 1);
      }
    } else
      return -UNSPECIFIED_RESULT;
  else
    if (!d2.empty())
      return UNSPECIFIED_RESULT;
    else
      return 0;
}

  }

WString asString(const boost::any& v, const WT_USTRING& format)
{
  if (v.empty())
    return WString();
  else if (v.type() == typeid(WString))
    return boost::any_cast<WString>(v);
  else if (v.type() == typeid(std::string))
    return WString::fromUTF8(boost::any_cast<std::string>(v));
  else if (v.type() == typeid(const char *))
    return WString::fromUTF8(boost::any_cast<const char *>(v));
  else if (v.type() == typeid(bool))
    return WString::tr(boost::any_cast<bool>(v) ? "Wt.true" : "Wt.false");
  else if (v.type() == typeid(WDate)) {
    const WDate& d = boost::any_cast<WDate>(v);
    return d.toString(format.empty() ? 
		      WLocale::currentLocale().dateFormat() :
		      format);
  } else if (v.type() == typeid(WDateTime)) {
    const WDateTime& dt = boost::any_cast<WDateTime>(v);
    return dt.toString(format.empty() ? 
		       "dd/MM/yy HH:mm:ss" :
		       format);
  } else if (v.type() == typeid(WLocalDateTime)) {
    const WLocalDateTime& dt = boost::any_cast<WLocalDateTime>(v);
    return dt.toString();
  } else if (v.type() == typeid(WTime)) {
    const WTime& t = boost::any_cast<WTime>(v);
    return t.toString(format.empty() ? "HH:mm:ss" : format);
  } else if (v.type() == typeid(boost::posix_time::ptime)) {
    const boost::posix_time::ptime& d 
      = boost::any_cast<boost::posix_time::ptime>(v);
    return WDateTime::fromPosixTime(d)
      .toString(format.empty() ? "dd/MM/yy HH:mm:ss" : format);
  } else if (v.type() == typeid(boost::posix_time::time_duration)) {
    const boost::posix_time::time_duration& dt
      = boost::any_cast<boost::posix_time::time_duration>(v);
    int millis = dt.fractional_seconds() *
      ::pow(10.0, 3 - boost::posix_time::time_duration::num_fractional_digits()); 
    return WTime(dt.hours(), dt.minutes(), dt.seconds(), millis)
      .toString(format.empty() ? "HH:mm:ss" : format);
  }

#define ELSE_LEXICAL_ANY(TYPE, CAST_TYPE)				\
  else if (v.type() == typeid(TYPE)) {					\
    if (format.empty())							\
      return WLocale::currentLocale()					\
        .toString((CAST_TYPE)boost::any_cast<TYPE>(v));			\
    else {								\
      char buf[100];							\
      snprintf(buf, 100, format.toUTF8().c_str(),			\
	       (CAST_TYPE)boost::any_cast<TYPE>(v));			\
      return WString::fromUTF8(buf);					\
    }									\
  }

  ELSE_LEXICAL_ANY(short, short)
  ELSE_LEXICAL_ANY(unsigned short, unsigned short)
  ELSE_LEXICAL_ANY(int, int)
  ELSE_LEXICAL_ANY(unsigned int, unsigned int)
  ELSE_LEXICAL_ANY(::int64_t, ::int64_t)
  ELSE_LEXICAL_ANY(::uint64_t, ::uint64_t)
  ELSE_LEXICAL_ANY(long long, ::int64_t)
  ELSE_LEXICAL_ANY(unsigned long long, ::uint64_t)
  ELSE_LEXICAL_ANY(float, float)
  ELSE_LEXICAL_ANY(double, double)

#undef ELSE_LEXICAL_ANY
  else if (v.type() == typeid(long)) {
    if (sizeof(long) == 4) {
      if (format.empty())
	return WLocale::currentLocale().toString
	  ((int)boost::any_cast<long>(v));
      else {
	char buf[100];
	snprintf(buf, 100, format.toUTF8().c_str(),
		 (int)boost::any_cast<long>(v));
	return WString::fromUTF8(buf);
      }
    } else {
      if (format.empty())
	return WLocale::currentLocale()
	  .toString((::int64_t)boost::any_cast<long>(v));
      else {
	char buf[100];
	snprintf(buf, 100, format.toUTF8().c_str(),
		 (::int64_t)boost::any_cast<long>(v));
	return WString::fromUTF8(buf);
      }
    }
  } else if (v.type() == typeid(unsigned long)) {
    if (sizeof(long) == 4) {
      if (format.empty())
	return WLocale::currentLocale().toString
	  ((unsigned)boost::any_cast<long>(v));
      else {
	char buf[100];
	snprintf(buf, 100, format.toUTF8().c_str(),
		 (unsigned)boost::any_cast<long>(v));
	return WString::fromUTF8(buf);
      }
    } else {
      if (format.empty())
	return WLocale::currentLocale()
	  .toString((::uint64_t)boost::any_cast<long>(v));
      else {
	char buf[100];
	snprintf(buf, 100, format.toUTF8().c_str(),
		 (::uint64_t)boost::any_cast<long>(v));
	return WString::fromUTF8(buf);
      }
    }
  }

  else {
    Impl::AbstractTypeHandler *handler = Impl::getRegisteredType(&v.type(),
								 true);
    if (handler)
      return handler->asString(v, format);
    else {
      LOG_ERROR("unsupported type '" << v.type().name() << "'");
      return WString::Empty;
    }
  }
}

double asNumber(const boost::any& v)
{
  if (v.empty())
    return std::numeric_limits<double>::signaling_NaN();
  else if (v.type() == typeid(WString))
    try {
      return WLocale::currentLocale().toDouble(boost::any_cast<WString>(v));
    } catch (boost::bad_lexical_cast& e) {
      return std::numeric_limits<double>::signaling_NaN();
    }
  else if (v.type() == typeid(std::string))
    try {
      return WLocale::currentLocale().toDouble(WT_USTRING::fromUTF8(boost::any_cast<std::string>(v)));
    } catch (boost::bad_lexical_cast& e) {
      return std::numeric_limits<double>::signaling_NaN();
    }
  else if (v.type() == typeid(const char *))
    try {
      return WLocale::currentLocale().toDouble(WT_USTRING::fromUTF8(boost::any_cast<const char *>(v)));
    } catch (boost::bad_lexical_cast&) {
      return std::numeric_limits<double>::signaling_NaN();
    }
  else if (v.type() == typeid(bool))
    return boost::any_cast<bool>(v) ? 1 : 0;
  else if (v.type() == typeid(WDate))
    return boost::any_cast<WDate>(v).toJulianDay();
  else if (v.type() == typeid(WDateTime)) {
    const WDateTime& dt = boost::any_cast<WDateTime>(v);
    return static_cast<double>(dt.toTime_t());
  } else if (v.type() == typeid(WLocalDateTime)) {
    const WLocalDateTime& dt = boost::any_cast<WLocalDateTime>(v);
    return static_cast<double>(dt.toUTC().toTime_t());
  } else if (v.type() == typeid(WTime)) {
    const WTime& t = boost::any_cast<WTime>(v);
    return static_cast<double>(WTime(0, 0).msecsTo(t));
  } else if (v.type() == typeid(boost::posix_time::ptime)) {
    const boost::posix_time::ptime& d 
      = boost::any_cast<boost::posix_time::ptime>(v);
    return static_cast<double>(WDateTime::fromPosixTime(d).toTime_t());
  } else if (v.type() == typeid(boost::posix_time::time_duration)) {
    const boost::posix_time::time_duration& dt
      = boost::any_cast<boost::posix_time::time_duration>(v);
    return dt.total_milliseconds();
  }

#define ELSE_NUMERICAL_ANY(TYPE) \
  else if (v.type() == typeid(TYPE)) \
    return static_cast<double>(boost::any_cast<TYPE>(v))

  ELSE_NUMERICAL_ANY(short);
  ELSE_NUMERICAL_ANY(unsigned short);
  ELSE_NUMERICAL_ANY(int);
  ELSE_NUMERICAL_ANY(unsigned int);
  ELSE_NUMERICAL_ANY(long);
  ELSE_NUMERICAL_ANY(unsigned long);
  ELSE_NUMERICAL_ANY(::int64_t);
  ELSE_NUMERICAL_ANY(::uint64_t);
  ELSE_NUMERICAL_ANY(long long);
  ELSE_NUMERICAL_ANY(float);
  ELSE_NUMERICAL_ANY(double);

#undef ELSE_NUMERICAL_ANY

  else {
    Impl::AbstractTypeHandler *handler = Impl::getRegisteredType(&v.type(),
								 true);
    if (handler)
      return handler->asNumber(v);
    else {
      LOG_ERROR("unsupported type '" << v.type().name() << "'");
      return 0;
    }
  }
}

extern WT_API boost::any convertAnyToAny(const boost::any& v,
					 const std::type_info& type,
					 const WT_USTRING& format)
{
  if (v.empty())
    return boost::any();
  else if (v.type() == type)
    return v;

  WString s = asString(v, format);

  if (type == typeid(WString))
    return s;
  else if (type == typeid(std::string))
    return s.toUTF8();
  else if (type == typeid(const char *))
    return s.toUTF8().c_str();
  else if (type == typeid(WDate)) {
    return WDate::fromString
      (s, format.empty() ? 
       WLocale::currentLocale().dateFormat() :
       format);
  } else if (type == typeid(WDateTime)) {
    return WDateTime::fromString
      (s, format.empty() ? "dd/MM/yy HH:mm:ss" : format);
  } else if (type == typeid(WLocalDateTime)) {
    return WLocalDateTime::fromString(s);
  } else if (type == typeid(WTime)) {
    return WTime::fromString
      (s, format.empty() ? "HH:mm:ss" : format);
  } else if (type == typeid(boost::posix_time::ptime)) {
    return WDateTime::fromString
      (s, format.empty() ? "dd/MM/yy HH:mm:ss" : format).toPosixTime();
  } else if (type == typeid(boost::posix_time::time_duration)) {
    return boost::posix_time::milliseconds
      (WTime(0, 0).msecsTo(WTime::fromString
			   (s, format.empty() ? "HH:mm:ss" : format)));
  } else if (type == typeid(bool)) {
    std::string b = s.toUTF8();
    if (b == "true" || b == "1")
      return true;
    else if (b == "false" || b == "0")
      return false;
    else
      throw std::runtime_error(std::string("Source string cannot be "
					   "converted to a bool value!"));
  }

#define ELSE_LEXICAL_ANY(TYPE)						\
  else if (type == typeid(TYPE)) {					\
    return boost::lexical_cast<TYPE>(s.toUTF8());			\
  }

  ELSE_LEXICAL_ANY(short)
  ELSE_LEXICAL_ANY(unsigned short)
  ELSE_LEXICAL_ANY(int)
  ELSE_LEXICAL_ANY(unsigned int)
  ELSE_LEXICAL_ANY(long)
  ELSE_LEXICAL_ANY(unsigned long)
  ELSE_LEXICAL_ANY(::int64_t)
  ELSE_LEXICAL_ANY(::uint64_t)
  ELSE_LEXICAL_ANY(long long)
  ELSE_LEXICAL_ANY(float)
  ELSE_LEXICAL_ANY(double)

#undef ELSE_LEXICAL_ANY

  else {
    // FIXME, add this to the traits.
    LOG_ERROR("unsupported type '" << v.type().name() << "'");
    return boost::any();
  }
}

}
