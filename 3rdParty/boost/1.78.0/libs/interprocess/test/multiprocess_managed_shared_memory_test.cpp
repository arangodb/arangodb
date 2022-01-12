//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2021-2021. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/interprocess/containers/list.hpp>
#include <cstring>
#include <cstdlib>
#include <string>
#include <iostream>
#include <cstdlib>
#include "../test/get_process_id_name.hpp"

using namespace boost::interprocess;

//Remove shared memory on construction and destruction
struct shm_remove
{
   shm_remove() { shared_memory_object::remove(test::get_process_id_name()); }
   ~shm_remove(){ shared_memory_object::remove(test::get_process_id_name()); }
};

const std::size_t MemSize = 64u*1024u;

typedef list<int, allocator<int, managed_shared_memory::segment_manager> >
   MyList;

int main(int argc, char *argv[])
{
   std::string p_or_c = argc == 1 ? "parent" : "child";
   BOOST_TRY {
      if(argc == 1){  //Parent process
         shm_remove remover; (void)remover;
         shared_memory_object::remove(test::get_process_id_name());
         shared_memory_object::remove(test::get_process_id_name());

         //Create a shared memory object.
         managed_shared_memory shm (create_only, test::get_process_id_name(), MemSize);

         interprocess_semaphore *my_sem = shm.construct<interprocess_semaphore>("MySem")(0U);

         //Launch child process
         std::string s;
         #ifdef BOOST_INTERPROCESS_WINDOWS
         s += "START /B ";
         #endif
         s += argv[0];
         s += " child ";
         s += test::get_process_id_name();
         #ifndef BOOST_INTERPROCESS_WINDOWS
         s += " &";
         #endif
         if(0 != std::system(s.c_str()))
            return 1;

         //Wait for the other process
         my_sem->wait();

         for (unsigned i = 0; i != 10000; ++i) {
            MyList *mylist = shm.construct<MyList>("MyList", std::nothrow)
                                 (shm.get_segment_manager());
            if(mylist){
               shm.destroy_ptr(mylist);
            }
         }

         //Wait for the other process
         my_sem->wait();
      }
      else{
         managed_shared_memory shm (open_only, argv[2]);

         interprocess_semaphore *my_sem = shm.find<interprocess_semaphore>("MySem").first;
         if (!my_sem)
            return 1;

         my_sem->post();

         for (unsigned i = 0; i != 10000; ++i) {
            MyList *mylist = shm.construct<MyList>("MyList", std::nothrow)
                                 (shm.get_segment_manager());

            if(mylist){
               shm.destroy_ptr(mylist);
            }
         }

         my_sem->post();
      }
   }
   BOOST_CATCH(interprocess_exception &e){
      std::cerr << p_or_c << " -> interprocess_exception::what(): " << e.what()
                << " native error: " << e.get_native_error()
                << " error code: " << e.get_error_code() << '\n';
      return 2;
   }
   BOOST_CATCH(std::exception &e){
      std::cerr << p_or_c << " -> std::exception::what(): " << e.what() << '\n';
      return 3;
   }
   BOOST_CATCH_END

   std::cerr << p_or_c << " -> Normal termination\n";

   return 0;
}

