// Copyright (C) 2014 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/thread.hpp>

using namespace boost::interprocess;

struct item
{
   int i;
};

struct queue
{
   void put( const item& item )
   {
      boost::unique_lock<boost::mutex> lock(mutex);
      while ( item_in )
         cond_full.wait(lock);

      item_ = item;
      item_in = true;
      cond_empty.notify_one();
   }

   void print()
   {
      boost::unique_lock<boost::mutex> lock(mutex);
      while ( !item_in )
         cond_empty.wait(lock);

      item_in = false;
      std::cout << item_.i << std::endl;
      cond_full.notify_one();
   }


private:
   //Mutex to protect access to the queue
   boost::mutex mutex;

   //Condition to wait when the queue is empty
   boost::condition_variable  cond_empty;

   //Condition to wait when the queue is full
   boost::condition_variable  cond_full;

   bool item_in;

   //Items to fill
   item item_;
};

void *addr;

void printThread()
{
   //Erase previous shared memory and schedule erasure on exit
   struct shm_remove
   {
      shm_remove() { shared_memory_object::remove("MySharedMemory"); }
      ~shm_remove(){ shared_memory_object::remove("MySharedMemory"); }
   } remover;

   //Create a shared memory object.
   shared_memory_object shm(create_only,"MySharedMemory",read_write);

   try
   {
//      //Set size
//      shm.truncate(sizeof(queue));
//
//      //Map the whole shared memory in this process
//      mapped_region region(shm,read_write);
//
//      //Get the address of the mapped region
//      void *addr = region.get_address();

      //Construct the shared structure in memory
      queue *q = new (addr) queue;

      do
      {
         q->print();
      } while ( true );
   }
//   catch(interprocess_exception &ex)
//   {
//      std::cout << ex.what() << std::endl;
//   }
   catch(boost::thread_interrupted&)
   {
      std::cout << "interrupted" << std::endl;
   }
   catch(...)
   {
      std::cout << "exception" << std::endl;
   }
}

int main()
{
   addr = new queue();
   boost::thread t(printThread);

   // give the thread time to create the shm
   boost::this_thread::sleep( boost::posix_time::milliseconds( 1000 ) );

//   //Create a shared memory object.
//   shared_memory_object shm(open_only,"MySharedMemory",read_write);

   try
   {
//      //Map the whole shared memory in this process
//      mapped_region region(shm,read_write);
//
//      //Get the address of the mapped region
//      void *addr = region.get_address();

      //Obtain a pointer to the shared structure
      queue *q = static_cast<queue*>(addr);

      item i;
      i.i = 42;
      q->put( i );

      ++i.i;
      q->put( i );

      // give the printThread time to "process" the item
      boost::this_thread::sleep( boost::posix_time::milliseconds( 1000 ) );

      t.interrupt();
      t.join();
   }
   catch(...)
   {
      std::cout << "exception" << std::endl;
      return -1;
   }
}
