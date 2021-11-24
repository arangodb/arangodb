//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/named_condition.hpp>
#include <boost/interprocess/sync/detail/locks.hpp>
#include "condition_test_template.hpp"
#include "named_creation_template.hpp"
#include "named_condition_test_helpers.hpp"
#include <string>
#include <sstream>
#include "get_process_id_name.hpp"

#if defined(BOOST_INTERPROCESS_WINDOWS)
#include <boost/interprocess/sync/windows/named_condition.hpp>
#include <boost/interprocess/sync/windows/named_mutex.hpp>
#endif

using namespace boost::interprocess;

int main()
{
   int ret;
   #if defined(BOOST_INTERPROCESS_WINDOWS)
   ret = test::test_named_condition<ipcdetail::winapi_named_condition, ipcdetail::winapi_named_mutex>();
   if (ret)
      return ret;
   #endif
   ret = test::test_named_condition<named_condition, named_mutex>();
   
   return ret;
}
