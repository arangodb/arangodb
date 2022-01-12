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
#include <boost/interprocess/sync/scoped_lock.hpp>
#include "mutex_test_template.hpp"
#include "named_creation_template.hpp"
#include <boost/interprocess/detail/interprocess_tester.hpp>
#include <exception>

#if defined(BOOST_INTERPROCESS_WINDOWS)
#include <boost/interprocess/sync/windows/named_mutex.hpp>
#endif

using namespace boost::interprocess;

template<class NamedMutex>
int test_named_mutex()
{
   int ret = 0;
   BOOST_TRY{
      NamedMutex::remove(test::get_process_id_name());
      test::test_named_creation< test::named_sync_creation_test_wrapper<NamedMutex> >();
      #if defined(BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES)
      test::test_named_creation< test::named_sync_creation_test_wrapper_w<NamedMutex> >();
      #endif   //defined(BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES)
      test::test_all_lock< test::named_sync_wrapper<NamedMutex> >();
      test::test_all_mutex<test::named_sync_wrapper<NamedMutex> >();
   }
   BOOST_CATCH(std::exception &ex){
      std::cout << ex.what() << std::endl;
      ret = 1;
   } BOOST_CATCH_END
   NamedMutex::remove(test::get_process_id_name());
   return ret;
}

int main()
{
   int ret;
   #if defined(BOOST_INTERPROCESS_WINDOWS)
   ret = test_named_mutex<ipcdetail::winapi_named_mutex>();
   if (ret)
      return ret;
   #endif
   ret = test_named_mutex<named_mutex>();
   
   return ret;
}
