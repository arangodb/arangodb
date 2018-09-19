// Copyright (C) 2014 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>

#define BOOST_THREAD_VERSION 4
#define BOOST_THREAD_PROVIDES_EXECUTORS
#define BOOST_THREAD_USES_LOG_THREAD_ID
#define BOOST_THREAD_QUEUE_DEPRECATE_OLD
#if ! defined  BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif

#include <boost/thread/executors/basic_thread_pool.hpp>
#include <boost/thread/future.hpp>

#include <numeric>
#include <algorithm>
#include <functional>
#include <iostream>
#include <vector>

#if defined(BOOST_THREAD_PROVIDES_VARIADIC_THREAD)

template<typename Iterator,typename T>
struct accumulate_block
{
    //typedef T result_type;
    T operator()(Iterator first,Iterator last)
    {
        return std::accumulate(first,last,T());
    }
};

template<typename Iterator,typename T>
T parallel_accumulate(Iterator first,Iterator last,T init)
{
    unsigned long const length=static_cast<unsigned long>(std::distance(first,last));

    if(!length)
        return init;

    unsigned long const block_size=25;
    unsigned long const num_blocks=(length+block_size-1)/block_size;

    boost::csbl::vector<boost::future<T> > futures(num_blocks-1);
    boost::basic_thread_pool pool;

    Iterator block_start=first;
    for(unsigned long i=0;i<(num_blocks-1);++i)
    {
        Iterator block_end=block_start;
        std::advance(block_end,block_size);
        futures[i]=boost::async(pool, accumulate_block<Iterator,T>(), block_start, block_end);
        block_start=block_end;
    }
    T last_result=accumulate_block<Iterator,T>()(block_start,last);
    T result=init;
    for(unsigned long i=0;i<(num_blocks-1);++i)
    {
        result+=futures[i].get();
    }
    result += last_result;
    return result;
}


int main()
{
  try
  {
    const int s = 1001;
    std::vector<int> vec;
    vec.reserve(s);
    for (int i=0; i<s;++i)
      vec.push_back(1);
    int r = parallel_accumulate(vec.begin(), vec.end(),0);
    std::cout << r << std::endl;

  }
  catch (std::exception& ex)
  {
    std::cout << "ERROR= " << ex.what() << "" << std::endl;
    return 1;
  }
  catch (...)
  {
    std::cout << " ERROR= exception thrown" << std::endl;
    return 2;
  }
  return 0;
}

#else
///#warning "This compiler doesn't supports variadics"
int main()
{
  return 0;
}
#endif
