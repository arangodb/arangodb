//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include "mutex_test_template.hpp"
#include "sharable_mutex_test_template.hpp"
#include "named_creation_template.hpp"
#include <boost/interprocess/sync/named_upgradable_mutex.hpp>
#include <string>
#include "get_process_id_name.hpp"

using namespace boost::interprocess;

int main ()
{
   int ret = 0;
   try{
      named_upgradable_mutex::remove(test::get_process_id_name());
      test::test_named_creation< test::named_sync_creation_test_wrapper<named_upgradable_mutex> >();
      #if defined(BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES)
      test::test_named_creation< test::named_sync_creation_test_wrapper_w<named_upgradable_mutex> >();
      #endif
      test::test_all_lock< test::named_sync_wrapper<named_upgradable_mutex> >();
      test::test_all_mutex<test::named_sync_wrapper<named_upgradable_mutex> >();
      test::test_all_sharable_mutex<test::named_sync_wrapper<named_upgradable_mutex> >();
   }
   catch(std::exception &ex){
      std::cout << ex.what() << std::endl;
      ret = 1;
   }
   named_upgradable_mutex::remove(test::get_process_id_name());
   return ret;
}
