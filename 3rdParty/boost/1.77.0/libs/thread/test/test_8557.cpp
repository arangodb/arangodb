// Copyright (C) 2013 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// B

#include <malloc.h>
#include <boost/thread/thread.hpp>

#include <boost/thread/mutex.hpp>

#include <boost/bind/bind.hpp>

#include <iostream>

       static void
       display_mallinfo()
       {
           struct mallinfo mi;

           mi = mallinfo();

           printf("Total non-mmapped bytes (arena):       %d\n", mi.arena);
           printf("# of free chunks (ordblks):            %d\n", mi.ordblks);
           printf("# of free fastbin blocks (smblks):     %d\n", mi.smblks);
           printf("# of mapped regions (hblks):           %d\n", mi.hblks);
           printf("Bytes in mapped regions (hblkhd):      %d\n", mi.hblkhd);
           printf("Max. total allocated space (usmblks):  %d\n", mi.usmblks);
           printf("Free bytes held in fastbins (fsmblks): %d\n", mi.fsmblks);
           printf("Total allocated space (uordblks):      %d\n", mi.uordblks);
           printf("Total free space (fordblks):           %d\n", mi.fordblks);
           printf("Topmost releasable block (keepcost):   %d\n", mi.keepcost);
       }

boost::mutex io_mutex;

void count() {

    for (int i = 0; i < 10; ++i) {

        boost::mutex::scoped_lock lock(io_mutex);

        //boost::this_thread::sleep( boost::posix_time::milliseconds( 100 ) );

    }

}
void* count2(void*) {

    for (int i = 0; i < 10; ++i) {

        boost::mutex::scoped_lock lock(io_mutex);

        boost::this_thread::sleep( boost::posix_time::milliseconds( 100 ) );

    }
    return 0;
}

int main() {
  printf("\n============== sizeof(boost::thread) ============== %d\n", sizeof(boost::thread));
  printf("\n============== sizeof(boost::detail::thread_data_base) ============== %d\n", sizeof(boost::detail::thread_data_base));
  printf("\n============== sizeof(boost::detail::thread_data<>) ============== %d\n", sizeof(boost::detail::thread_data<void(*)()>));
  printf("\n============== Before thread creation ==============\n");
  display_mallinfo();
  {
    boost::thread thrd1(&count);

    printf("\n============== After thread creation ==============\n");
    display_mallinfo();

    boost::thread thrd2(&count);
    printf("\n============== After thread creation ==============\n");
    display_mallinfo();
    boost::thread thrd3(&count);
    printf("\n============== After thread creation ==============\n");
    display_mallinfo();

    thrd1.join();
    printf("\n============== After thread join ==============\n");
    display_mallinfo();

    thrd2.join();
    printf("\n============== After thread join ==============\n");
    display_mallinfo();
    thrd3.join();
    printf("\n============== After thread join ==============\n");
    display_mallinfo();
  }
  printf("\n============== After thread destruction ==============\n");
  display_mallinfo();

  {
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    pthread_t thrd1;
    pthread_create(&thrd1, &attr, &count2, 0);
    printf("\n============== After thread creation ==============\n");
    display_mallinfo();

    pthread_t thrd2;
    pthread_create(&thrd2, &attr, &count2, 0);
    printf("\n============== After thread creation ==============\n");
    display_mallinfo();

    pthread_t thrd3;
    pthread_create(&thrd3, &attr, &count2, 0);
    printf("\n============== After thread creation ==============\n");
    display_mallinfo();

    pthread_attr_destroy(&attr);
    printf("\n============== After thread attr destroy ==============\n");
    display_mallinfo();

    void* res;
    pthread_join(thrd3, &res);
    printf("\n============== After thread join ==============\n");
    display_mallinfo();

    pthread_join(thrd2, &res);
    printf("\n============== After thread join ==============\n");
    display_mallinfo();
    pthread_join(thrd1, &res);
    printf("\n============== After thread join ==============\n");
    display_mallinfo();



    //pthread_destroy(&thrd1);
    //pthread_destroy(&thrd2);
    //pthread_destroy(&thrd3);
  }
  printf("\n============== After thread destruction ==============\n");
  display_mallinfo();

    return 1;

}
