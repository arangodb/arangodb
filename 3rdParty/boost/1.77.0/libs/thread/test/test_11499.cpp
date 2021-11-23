// Copyright (C) 2014 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_THREAD_VERSION 4

#include <boost/thread/shared_mutex.hpp>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <vector>

using MutexT = boost::shared_mutex;
using ReaderLockT = std::lock_guard<MutexT>;
using WriterLockT = std::shared_lock<MutexT>;

MutexT gMutex;
std::atomic<bool> running(true);


void myread()
{
  long reads = 0;
   while (running && reads < 100000)
   {
      ReaderLockT lock(gMutex);
      std::this_thread::yield();
      ++reads;
   }
}

int main()
{
   using namespace std;

   vector<thread> threads;
   for (int i = 0; i < 256; ++i)
   {
      threads.emplace_back(thread(myread));
   }

//   string str;
//
//   getline(std::cin, str);
   running = false;

   for (auto& thread : threads)
   {
      thread.join();
   }

   return 0;
}

