// Copyright (C) 2014-2015 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/experimental/parallel/v1/exception_list.hpp>


#define BOOST_THREAD_VERSION 4
#define BOOST_THREAD_PROVIDES_EXECUTORS
#include <boost/config.hpp>
#if ! defined  BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif

#include <boost/thread/experimental/parallel/v2/task_region.hpp>
#include <string>

#include <boost/detail/lightweight_test.hpp>

#if ! defined BOOST_NO_CXX11_LAMBDAS  && defined(BOOST_THREAD_PROVIDES_INVOKE)
using boost::experimental::parallel::v2::task_region;
using boost::experimental::parallel::v2::task_region_handle;
using boost::experimental::parallel::v1::exception_list;

void run_no_exception()
{
  std::string s("test");
  bool parent_flag = false;
  bool task1_flag = false;
  bool task2_flag = false;
  bool task21_flag = false;
  bool task3_flag = false;
  task_region([&](task_region_handle& trh)
      {
        parent_flag = true;
        trh.run([&]()
            {
              task1_flag = true;
              std::cout << "task1: " << s << std::endl;
            });
        trh.run([&]()
            {
              task2_flag = true;
              std::cout << "task2" << std::endl;
              task_region([&](task_region_handle& trh)
                  {
                    trh.run([&]()
                        {
                          task21_flag = true;
                          std::cout << "task2.1" << std::endl;
                        });
                  });
            });
        int i = 0, j = 10, k = 20;
        trh.run([=, &task3_flag]()
            {
              task3_flag = true;
              std::cout << "task3: " << i << " " << j << " " << k << std::endl;
            });
        std::cout << "parent" << std::endl;
      });
  BOOST_TEST(parent_flag);
  BOOST_TEST(task1_flag);
  BOOST_TEST(task2_flag);
  BOOST_TEST(task21_flag);
  BOOST_TEST(task3_flag);
}

void run_no_exception_wait()
{
  std::string s("test");
  bool parent_flag = false;
  bool wait_flag = false;
  bool task1_flag = false;
  bool task2_flag = false;
  bool task21_flag = false;
  bool task3_flag = false;
  task_region([&](task_region_handle& trh)
      {
        parent_flag = true;
        trh.run([&]()
            {
              task1_flag = true;
              std::cout << "task1: " << s << std::endl;
            });
        trh.run([&]()
            {
              task2_flag = true;
              std::cout << "task2" << std::endl;
              task_region([&](task_region_handle& trh)
                  {
                    trh.run([&]()
                        {
                          task21_flag = true;
                          std::cout << "task2.1" << std::endl;
                        });
                  });
            });
        int i = 0, j = 10, k = 20;
        trh.run([=, &task3_flag]()
            {
              task3_flag = true;
              std::cout << "task3: " << i << " " << j << " " << k << std::endl;
            });
        std::cout << "before" << std::endl;
        trh.wait();
        wait_flag = true;
        std::cout << "parent" << std::endl;
      });
  BOOST_TEST(parent_flag);
  BOOST_TEST(wait_flag);
  BOOST_TEST(task1_flag);
  BOOST_TEST(task2_flag);
  BOOST_TEST(task21_flag);
  BOOST_TEST(task3_flag);
}

void run_exception_1()
{
  try
  {
    task_region([](task_region_handle& trh)
        {
          trh.run([]()
              {
                std::cout << "task1" << std::endl;
                throw 1;
              });
          boost::this_thread::sleep_for(boost::chrono::seconds(1));
          trh.run([]()
              {
                std::cout << "task3" << std::endl;
              });
#if defined BOOST_THREAD_TASK_REGION_HAS_SHARED_CANCELED
          BOOST_TEST(false);
#endif
        });
    BOOST_TEST(false);
  }
  catch (exception_list const& e)
  {
    BOOST_TEST_EQ(e.size(), 1u);
  }
  catch (...)
  {
    BOOST_TEST(false);
  }
}

void run_exception()
{
  try
  {
    task_region([](task_region_handle& trh)
        {
          trh.run([]()
              {
                std::cout << "task1" << std::endl;
                throw 1;
              });
          trh.run([]()
              {
                std::cout << "task2" << std::endl;
                throw 2;
              });
          boost::this_thread::sleep_for(boost::chrono::seconds(1));
          trh.run([]()
              {
                std::cout << "task3" << std::endl;
                throw 3;
              });
          std::cout << "parent" << std::endl;
          throw 100;
        });
    BOOST_TEST(false);
  }
  catch (exception_list const& el)
  {
    BOOST_TEST(el.size() >= 1u);
    for (boost::exception_ptr const& e: el)
    {
      try {
        boost::rethrow_exception(e);
      }
      catch (boost::exception&)
      {
        BOOST_TEST(true);
      }
      catch (int i) // this doesn't works yet
      {
        BOOST_TEST((i == 1) || (i == 2) || (i == 3));
      }
      catch (...)
      {
        BOOST_TEST(false);
      }
    }
  }
  catch (...)
  {
    BOOST_TEST(false);
  }
}


void run_nested_exception()
{
  std::string s("test");
  bool parent_flag = false;
  bool task1_flag = false;
  bool task2_flag = false;
  bool task21_flag = false;
  bool task3_flag = false;
  try
  {
  task_region([&](task_region_handle& trh)
      {
        parent_flag = true;
        trh.run([&]()
            {
              task1_flag = true;
              std::cout << "task1: " << s << std::endl;
            });
        trh.run([&]()
            {
              task2_flag = true;
              std::cout << "task2" << std::endl;
              task_region([&](task_region_handle& trh)
                  {
                    trh.run([&]()
                        {
                          task21_flag = true;
                          std::cout << "task2.1" << std::endl;
                          throw 21;
                        });
                  });
            });
        int i = 0, j = 10, k = 20;
        trh.run([=, &task3_flag]()
            {
              task3_flag = true;
              std::cout << "task3: " << i << " " << j << " " << k << std::endl;
            });
        std::cout << "parent" << std::endl;
      });
  }
  catch (exception_list const& el)
  {
    BOOST_TEST(el.size() == 1u);
    for (boost::exception_ptr const& e: el)
    {
      try {
        boost::rethrow_exception(e);
      }
      catch (int i) // this doesn't works yet
      {
        BOOST_TEST_EQ(i, 21);
      }
      catch (boost::exception&)
      {
        BOOST_TEST(true);
      }
      catch (...)
      {
        BOOST_TEST(false);
      }
    }
  }
  catch (...)
  {
    BOOST_TEST(false);
  }
  BOOST_TEST(parent_flag);
  BOOST_TEST(task1_flag);
  BOOST_TEST(task2_flag);
  BOOST_TEST(task21_flag);
}


int main()
{
  run_no_exception();
  run_no_exception_wait();
  run_exception();
  run_exception_1();
  run_nested_exception();
  return boost::report_errors();
}
#else
int main()
{
  return boost::report_errors();
}
#endif
