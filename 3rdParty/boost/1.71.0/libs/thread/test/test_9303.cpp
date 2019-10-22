// Copyright (C) 2014 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_THREAD_VERSION 4
    #include <iostream>
    #include <fstream>
    #include <stdio.h>
    #include <boost/function.hpp>
    #include <boost/make_shared.hpp>
    #include <boost/shared_ptr.hpp>
    #include <boost/bind.hpp>
    #include <boost/asio.hpp>
    #include <boost/thread.hpp>
    #include <boost/thread/future.hpp>

#if defined(BOOST_THREAD_PROVIDES_VARIADIC_THREAD)
    #define EXAMPLE_1
    #define EXAMPLE_2
    #define EXAMPLE_3
    #define EXAMPLE_4
    #define EXAMPLE_5
    #define EXAMPLE_6
    #define EXAMPLE_7
#else
    #define EXAMPLE_1
    #define EXAMPLE_2
    //#define EXAMPLE_3
    //#define EXAMPLE_4
    //#define EXAMPLE_5
    //#define EXAMPLE_6
    //#define EXAMPLE_7
#endif

    // Test functions

    int int_no_params()
    {
        return 42;
    }

    int int_with_params(int i)
    {
        return i;
    }

    std::string string_no_params()
    {
        return std::string("forty two");
    }

    std::string string_with_params(std::string& ans)
    {
        return ans;
    }

    int main(int /*argc*/, char ** /*argv[]*/)
    {
        std::string ans("forty two");

    #if defined EXAMPLE_1
        //! Compiles and produces correct result.
        {
            boost::packaged_task<int()> example(int_no_params);
            boost::future<int> f = example.get_future();
            boost::thread task(boost::move(example));
            int answer = f.get();
            std::cout << "Answer to life and whatnot, in English: " << answer << std::endl;
            task.join();
        }
    #endif

    #if defined EXAMPLE_2
        //! Compiles and produces correct result.
        {
            boost::packaged_task<std::string()> example(string_no_params);
            boost::future<std::string> f = example.get_future();
            boost::thread task(boost::move(example));
            std::string answer = f.get();
            std::cout << "string_no_params: " << answer << std::endl;
            task.join();
        }

    #endif

    #if defined EXAMPLE_3
        //! Doesn't compile in C++03.
        //! error: variable �boost::packaged_task<std::basic_string<char>(std::basic_string<char>&)> example� has initializer but incomplete type

        {
            boost::packaged_task<std::string(std::string&)> example(string_with_params);
            boost::future<std::string> f = example.get_future();
            example(ans);
            std::string answer = f.get();
            std::cout << "string_with_params: " << answer << std::endl;
        }

    #endif

    #if defined EXAMPLE_4
        //! Doesn't compile in C++11
//        In file included from test_9303.cpp:10:
//        In file included from ../../../boost/thread.hpp:13:
//        In file included from ../../../boost/thread/thread.hpp:12:
//        In file included from ../../../boost/thread/thread_only.hpp:22:
//        ../../../boost/thread/detail/thread.hpp:76:15: error: no matching function for call to 'invoke'
//                      invoke(std::move(std::get<0>(fp)), std::move(std::get<Indices>(fp))...);
//                      ^~~~~~

        {
            boost::packaged_task<std::string(std::string&)> example(string_with_params);
            boost::future<std::string> f = example.get_future();
            boost::thread task(boost::move(example), boost::ref(ans));
            std::string answer = f.get();
            std::cout << "string_with_params: " << answer << std::endl;
            task.join();
        }
    #endif

    #if defined EXAMPLE_5
        //! Doesn't compile in C++03, C++11 only.
        //! error: extended initializer lists only available with -std=c++11 or -std=gnu++11 [-Werror]
        {
            boost::packaged_task<std::string(std::string&)> example
            { boost::bind(&string_with_params, ans) };
            boost::future<std::string> f = example.get_future();
            boost::thread task(boost::move(example), boost::ref(ans));
            std::string answer = f.get();
            std::cout << "string_with_params: " << answer << std::endl;
            task.join();
        }
    #endif

    #if defined EXAMPLE_6
        //! Doesn't compile in C++03, C++11 only.
        // packagedTestTest.cpp:94:43: error: invalid use of incomplete type ‘class boost::packaged_task<std::basic_string<char>(std::basic_string<char>&)>’
        // packagedTestTest.cpp:95:37: error: incomplete type ‘task_t {aka boost::packaged_task<std::basic_string<char>(std::basic_string<char>&)>}’ used in nested name specifier
        // boost/thread/future.hpp:1320:11: error: declaration of ‘class boost::packaged_task<std::basic_string<char>(std::basic_string<char>&)>’
        {
          //typedef boost::packaged_task<std::string(std::string&)> task_t;
          typedef boost::packaged_task<std::string()> task_t;
            boost::shared_ptr<task_t> example = boost::make_shared<task_t>(boost::bind(&string_with_params, boost::ref(ans)));
            boost::future<std::string> f = example->get_future();
            boost::thread task(boost::bind(&task_t::operator(), example));
            std::string answer = f.get();
            std::cout << "string_with_params: " << answer << std::endl;
            task.join();
        }
    #endif

    #if defined EXAMPLE_7
        //! Doesn't compile in C++03, C++11 only.
        // packagedTestTest.cpp:94:43: error: invalid use of incomplete type ‘class boost::packaged_task<std::basic_string<char>(std::basic_string<char>&)>’
        // packagedTestTest.cpp:95:37: error: incomplete type ‘task_t {aka boost::packaged_task<std::basic_string<char>(std::basic_string<char>&)>}’ used in nested name specifier
        // boost/thread/future.hpp:1320:11: error: declaration of ‘class boost::packaged_task<std::basic_string<char>(std::basic_string<char>&)>’
        {
            boost::asio::io_service io_service;
            boost::thread_group threads;
            boost::asio::io_service::work work(io_service);

            for (int i = 0; i < 3; ++i)
            {
                threads.create_thread(boost::bind(&boost::asio::io_service::run,
                    &io_service));
            }
            typedef boost::packaged_task<std::string()> task_t;
            boost::shared_ptr<task_t> example = boost::make_shared<task_t>(boost::bind(&string_with_params, ans));
            boost::future<std::string> f = example->get_future();
            io_service.post(boost::bind(&task_t::operator(), example));
            std::string answer = f.get();
            std::cout << "string_with_params: " << answer << std::endl;
            threads.join_all();
        }
    #endif
        return 0;
    }
