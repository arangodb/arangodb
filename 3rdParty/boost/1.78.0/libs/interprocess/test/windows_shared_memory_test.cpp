//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/config.hpp>

#ifdef BOOST_WINDOWS

#include <boost/interprocess/windows_shared_memory.hpp>
#include <boost/interprocess/detail/managed_open_or_create_impl.hpp>
#include <boost/interprocess/exceptions.hpp>
#include "named_creation_template.hpp"
#include <cstring>   //for strcmp, memset
#include <iostream>  //for cout
#include <string>  //for string
#include "get_process_id_name.hpp"

using namespace boost::interprocess;

static const char *name_initialization_routine()
{
   static std::string process_name;
   test::get_process_id_name(process_name);
   return process_name.c_str();
}

static const std::size_t ShmSize = 1000;
typedef ipcdetail::managed_open_or_create_impl
   <windows_shared_memory, 0, false, false> windows_shared_memory_t;

//This wrapper is necessary to have a common constructor
//in generic named_creation_template functions
class shared_memory_creation_test_wrapper
   : public windows_shared_memory_t
{
   public:
   shared_memory_creation_test_wrapper(create_only_t)
      :  windows_shared_memory_t(create_only, name_initialization_routine(), ShmSize, read_write, 0, permissions())
   {}

   shared_memory_creation_test_wrapper(open_only_t)
      :  windows_shared_memory_t(open_only, name_initialization_routine(), read_write, 0)
   {}

   shared_memory_creation_test_wrapper(open_or_create_t)
      :  windows_shared_memory_t(open_or_create, name_initialization_routine(), ShmSize, read_write, 0, permissions())
   {}
};

#ifdef BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES

static const wchar_t *wname_initialization_routine()
{
   static std::wstring process_name;
   test::get_process_id_wname(process_name);
   return process_name.c_str();
}

class shared_memory_creation_test_wrapper_w
   : public windows_shared_memory_t
{
   public:
   shared_memory_creation_test_wrapper_w(create_only_t)
      :  windows_shared_memory_t(create_only, wname_initialization_routine(), ShmSize, read_write, 0, permissions())
   {}

   shared_memory_creation_test_wrapper_w(open_only_t)
      :  windows_shared_memory_t(open_only, wname_initialization_routine(), read_write, 0)
   {}

   shared_memory_creation_test_wrapper_w(open_or_create_t)
      :  windows_shared_memory_t(open_or_create, wname_initialization_routine(), ShmSize, read_write, 0, permissions())
   {}
};

#endif   //BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES


int main ()
{
   BOOST_TRY{
      test::test_named_creation<shared_memory_creation_test_wrapper>();
      #ifdef BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES
      test::test_named_creation<shared_memory_creation_test_wrapper_w>();
      #endif
   }
   BOOST_CATCH(std::exception &ex){
      std::cout << ex.what() << std::endl;
      return 1;
   } BOOST_CATCH_END

   return 0;
}

#else

int main()
{
   return 0;
}

#endif   //#ifdef BOOST_WINDOWS
