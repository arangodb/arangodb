 //////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2011-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTERPROCESS_DETAIL_WINAPI_WRAPPER_COMMON_HPP
#define BOOST_INTERPROCESS_DETAIL_WINAPI_WRAPPER_COMMON_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif
#
#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/interprocess/detail/config_begin.hpp>
#include <boost/interprocess/detail/workaround.hpp>
#include <boost/interprocess/detail/win32_api.hpp>
#include <boost/interprocess/detail/posix_time_types_wrk.hpp>
#include <boost/interprocess/errors.hpp>
#include <boost/interprocess/exceptions.hpp>
#include <limits>

namespace boost {
namespace interprocess {
namespace ipcdetail {

inline bool winapi_wrapper_timed_wait_for_single_object(void *handle, const boost::posix_time::ptime &abs_time);
 
inline void winapi_wrapper_wait_for_single_object(void *handle)
{
   winapi_wrapper_timed_wait_for_single_object(handle, boost::posix_time::pos_infin);
}

inline bool winapi_wrapper_try_wait_for_single_object(void *handle)
{
   return winapi_wrapper_timed_wait_for_single_object(handle, boost::posix_time::min_date_time);
}

inline bool winapi_wrapper_timed_wait_for_single_object(void *handle, const boost::posix_time::ptime &abs_time)
{
   const boost::posix_time::ptime cur_time = microsec_clock::universal_time();
   //Windows uses relative wait times so check for negative waits
   //and implement as 0 wait to allow try-semantics as POSIX mandates.
   unsigned long time = 0u;
   if (abs_time == boost::posix_time::pos_infin){
      time = winapi::infinite_time;
   }
   else if(abs_time > cur_time){
      time = (abs_time - cur_time).total_milliseconds();
   }

   unsigned long ret = winapi::wait_for_single_object(handle, time);
   if(ret == winapi::wait_object_0){
      return true;
   }
   else if(ret == winapi::wait_timeout){
      return false;
   }
   else if(ret == winapi::wait_abandoned){ //Special case for orphaned mutexes
      winapi::release_mutex(handle);
      throw interprocess_exception(owner_dead_error);
   }
   else{
      error_info err = system_error_code();
      throw interprocess_exception(err);
   }
}

}  //namespace ipcdetail {
}  //namespace interprocess {
}  //namespace boost {

#include <boost/interprocess/detail/config_end.hpp>

#endif   //BOOST_INTERPROCESS_DETAIL_WINAPI_WRAPPER_COMMON_HPP
