//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/interprocess/detail/config_begin.hpp>
#include <boost/interprocess/detail/workaround.hpp>

#ifdef BOOST_INTERPROCESS_WINDOWS

#define BOOST_INTERPROCESS_WINDOWS

//Force user-defined get_shared_dir
#define BOOST_INTERPROCESS_SHARED_DIR_FUNC
#include <boost/interprocess/detail/shared_dir_helpers.hpp>
#include <string>

#include "get_process_id_name.hpp"

namespace boost {
namespace interprocess {
namespace ipcdetail {

static bool dir_created = false;

inline void get_shared_dir(std::string &shared_dir)
{
   shared_dir = boost::interprocess::ipcdetail::get_temporary_path();
   shared_dir += "/boostipctest_";
   shared_dir += boost::interprocess::test::get_process_id_name();
   if(!dir_created)
      ipcdetail::create_directory(shared_dir.c_str());
   dir_created = true;
}

}}}   //namespace boost::interprocess::ipcdetail

#include <boost/interprocess/shared_memory_object.hpp>
#include <iostream>

int main ()
{
   using namespace boost::interprocess;
   const char *const shm_name = "test_shm";
   std::string shared_dir;
   ipcdetail::get_shared_dir(shared_dir);

   std::string shm_path(shared_dir);
   shm_path += shm_name;

   int ret = 0;
   shared_memory_object::remove(shm_name);
   {
      shared_memory_object shm(create_only, shm_name, read_write);

      shm_path += shm_name;
      int ret = ipcdetail::invalid_file() == ipcdetail::open_existing_file(shm_path.c_str(), read_only) ?
               1 : 0;
      if(ret)
      {
         std::cerr << "Error opening user get_shared_dir()/shm file" << std::endl;
      }
   }
   shared_memory_object::remove(shm_name);
   ipcdetail::remove_directory(shared_dir.c_str());

   return ret;
}

#else

int main()
{
   return 0;
}

#endif   //#ifdef BOOST_INTERPROCESS_WINDOWS

#include <boost/interprocess/detail/config_end.hpp>
