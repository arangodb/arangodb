//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2001-2003
// William E. Kempf
//
// Permission to use, copy, modify, distribute and sell this software
// and its documentation for any purpose is hereby granted without fee,
// provided that the above copyright notice appear in all copies and
// that both that copyright notice and this permission notice appear
// in supporting documentation.  William E. Kempf makes no representations
// about the suitability of this software for any purpose.
// It is provided "as is" without express or implied warranty.

#ifndef BOOST_INTERPROCESS_TEST_UTIL_HEADER
#define BOOST_INTERPROCESS_TEST_UTIL_HEADER

#include <boost/interprocess/detail/config_begin.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/detail/os_thread_functions.hpp>

#include <boost/date_time/posix_time/posix_time_types.hpp>
#define BOOST_CHRONO_HEADER_ONLY
#include <boost/chrono/system_clocks.hpp>
#include <boost/version.hpp>

#if !defined(BOOST_NO_CXX11_HDR_CHRONO)
#include <chrono>
#endif

namespace boost {
namespace interprocess {
namespace test {

inline boost::posix_time::ptime ptime_delay(int secs, int msecs=0, int nsecs = 0)
{
   (void)msecs;
   using namespace boost::posix_time;
   int count = static_cast<int>(double(nsecs)*
               (double(time_duration::ticks_per_second())/double(1000000000.0)));
   count += static_cast<int>(double(msecs)*
               (double(time_duration::ticks_per_second())/double(1000.0)));
   boost::posix_time::ptime cur = boost::posix_time::microsec_clock::universal_time();
   return cur +=  boost::posix_time::time_duration(0, 0, secs, count);
}

inline boost::chrono::system_clock::time_point boost_systemclock_delay(int secs)
{  return boost::chrono::system_clock::now() + boost::chrono::seconds(secs);  }

#if !defined(BOOST_NO_CXX11_HDR_CHRONO)
//Use std chrono if available
inline std::chrono::system_clock::time_point std_systemclock_delay(int secs)
{  return std::chrono::system_clock::now() + std::chrono::seconds(secs);  }

#else
//Otherwise use boost chrono
inline boost::chrono::system_clock::time_point std_systemclock_delay(int secs)
{  return boost_systemclock_delay(secs);  }

#endif


template <typename P>
class thread_adapter
{
   public:
   thread_adapter(void (*func)(void*, P &), void* param1, P &param2)
      : func_(func), param1_(param1) ,param2_(param2){ }
   void operator()() const { func_(param1_, param2_); }

   private:
   void (*func_)(void*, P &);
   void* param1_;
   P& param2_;
};

template <typename P>
struct data
{
   explicit data(int id, int secs=0)
      : m_id(id), m_value(-1), m_secs(secs), m_error(no_error)
   {}
   int            m_id;
   int            m_value;
   int            m_secs;
   error_code_t   m_error;
};

int shared_val = 0;
static const int BaseSeconds = 1;

}  //namespace test {
}  //namespace interprocess {
}  //namespace boost {

#include <boost/interprocess/detail/config_end.hpp>

#endif   //#ifndef BOOST_INTERPROCESS_TEST_UTIL_HEADER
