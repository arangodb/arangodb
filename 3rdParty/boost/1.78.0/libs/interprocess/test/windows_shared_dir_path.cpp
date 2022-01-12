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

//Force user-defined get_shared_dir
#define BOOST_INTERPROCESS_SHARED_DIR_PATH boost::interprocess::ipcdetail::get_temporary_path()
#define BOOST_INTERPROCESS_SHARED_DIR_WPATH boost::interprocess::ipcdetail::get_temporary_wpath()
#include <string>

#include "get_process_id_name.hpp"

#include <boost/interprocess/shared_memory_object.hpp>
#include <iostream>

int main ()
{
   using namespace boost::interprocess;

   int ret = 0;

   {
      std::string pname= boost::interprocess::test::get_process_id_name();
      const char *const shm_name = pname.c_str();

      shared_memory_object::remove(shm_name);
      std::string shared_dir;
      ipcdetail::get_shared_dir(shared_dir);

      std::string shm_path(shared_dir);
      shm_path += "/";
      shm_path += shm_name;
      {
         shared_memory_object shm(create_only, shm_name, read_write);
      }

      ret = ipcdetail::invalid_file() == ipcdetail::open_existing_file(shm_path.c_str(), read_only) ?
               1 : 0;
      if(ret)
      {
         std::cerr << "Error opening user get_shared_dir()/shm file" << std::endl;
         return ret;
      }

      shared_memory_object::remove(shm_name);
   }
   {
      std::wstring wpname= boost::interprocess::test::get_process_id_wname();
      const wchar_t *const wshm_name = wpname.c_str();

      shared_memory_object::remove(wshm_name);
      std::wstring wshared_dir;
      ipcdetail::get_shared_dir(wshared_dir);

      shared_memory_object::remove(wshm_name);
      std::wstring wshm_path(wshared_dir);
      wshm_path += L"/";
      wshm_path += wshm_name;
      {
         shared_memory_object shm(create_only, wshm_name, read_write);
      }
      ret = ipcdetail::invalid_file() == ipcdetail::open_existing_file(wshm_path.c_str(), read_only) ?
               1 : 0;
      if(ret)
      {
         std::cerr << "Error opening user get_shared_dir()/wshm file" << std::endl;
         return ret;
      }
      shared_memory_object::remove(wshm_name);
   }

   return ret;
}

#else

int main()
{
   return 0;
}

#endif   //#ifdef BOOST_WINDOWS
