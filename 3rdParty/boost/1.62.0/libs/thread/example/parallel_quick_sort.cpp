// Copyright (C) 2012-2013 Vicente Botet
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
#if defined(BOOST_THREAD_PROVIDES_VARIADIC_THREAD)

#include <numeric>
#include <algorithm>
#include <functional>
#include <iostream>
#include <list>

template<typename T>
struct sorter
{
    boost::basic_thread_pool pool;
    typedef std::list<T> return_type;

    std::list<T> do_sort(std::list<T> chunk_data)
    {
        if(chunk_data.empty())
        {
            return chunk_data;
        }

        std::list<T> result;
        result.splice(result.begin(),chunk_data, chunk_data.begin());
        T const& partition_val=*result.begin();

        typename std::list<T>::iterator divide_point=
            std::partition(chunk_data.begin(), chunk_data.end(), [&](T const& val){return val<partition_val;});

        std::list<T> new_lower_chunk;
        new_lower_chunk.splice(new_lower_chunk.end(), chunk_data, chunk_data.begin(), divide_point);

        boost::future<std::list<T> > new_lower = boost::async(pool, &sorter::do_sort, this, std::move(new_lower_chunk));
        //boost::future<std::list<T> > new_lower = boost::async<return_type>(pool, &sorter::do_sort, this, std::move(new_lower_chunk));

        std::list<T> new_higher(do_sort(chunk_data));

        result.splice(result.end(),new_higher);
        while(!new_lower.is_ready())
        {
            pool.schedule_one_or_yield();
        }

        result.splice(result.begin(),new_lower.get());
        return result;
    }
};


template<typename T>
std::list<T> parallel_quick_sort(std::list<T>& input)
{
    if(input.empty())
    {
        return input;
    }
    sorter<T> s;

    return s.do_sort(input);
}


int main()
{
  try
  {
    const int s = 101;
    std::list<int> lst;
    for (int i=0; i<s;++i)
      lst.push_back(100-i);
    std::list<int> r = parallel_quick_sort(lst);
    for (std::list<int>::const_iterator it=r.begin(); it != r.end(); ++it)
      std::cout << *it << std::endl;

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
//#warning "This compiler doesn't supports variadics and move semantics"
int main()
{
  return 0;
}
#endif
