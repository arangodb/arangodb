// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// This is a simple program that demonstrates the use of LEAF to transport error
// objects between threads, without using exception handling. See capture_eh.cpp
// for the version that uses exception handling.

#include <boost/leaf.hpp>
#include <vector>
#include <string>
#include <future>
#include <iterator>
#include <iostream>
#include <algorithm>

namespace leaf = boost::leaf;

// Define several error types.
struct e_thread_id { std::thread::id value; };
struct e_failure_info1 { std::string value; };
struct e_failure_info2 { int value; };

// A type that represents a successfully returned result from a task.
struct task_result { };

 // This is our task function. It produces objects of type task_result, but it
 // may fail.
leaf::result<task_result> task()
{
    bool succeed = (rand()%4) != 0; //...at random.
    if( succeed )
        return { };
    else
        return leaf::new_error(
            e_thread_id{std::this_thread::get_id()},
            e_failure_info1{"info"},
            e_failure_info2{42} );
};

int main()
{
    int const task_count = 42;

    // The error_handlers are used in this thread (see leaf::try_handle_all
    // below). The arguments passed to individual lambdas are transported from
    // the worker thread to the main thread automatically.
    auto error_handlers = std::make_tuple(
        []( e_failure_info1 const & v1, e_failure_info2 const & v2, e_thread_id const & tid )
        {
            std::cerr << "Error in thread " << tid.value << "! failure_info1: " << v1.value << ", failure_info2: " << v2.value << std::endl;
        },
        []( leaf::diagnostic_info const & unmatched )
        {
            std::cerr <<
                "Unknown failure detected" << std::endl <<
                "Cryptic diagnostic information follows" << std::endl <<
                unmatched;
        } );

    // Container to collect the generated std::future objects.
    std::vector<std::future<leaf::result<task_result>>> fut;

    // Launch the tasks, but rather than launching the task function directly,
    // we launch a wrapper function which calls leaf::capture, passing a context
    // object that will hold the error objects reported from the task in case of
    // an error. The error types the context is able to hold statically are
    // automatically deduced from the type of the error_handlers tuple.
    std::generate_n( std::back_inserter(fut), task_count,
        [&]
        {
            return std::async(
                std::launch::async,
                [&]
                {
                    return leaf::capture(leaf::make_shared_context(error_handlers), &task);
                } );
        } );

    // Wait on the futures, get the task results, handle errors.
    for( auto & f : fut )
    {
        f.wait();

        leaf::try_handle_all(
            [&]() -> leaf::result<void>
            {
                BOOST_LEAF_AUTO(r,f.get());

                // Success! Use r to access task_result.
                std::cout << "Success!" << std::endl;
                (void) r; // Presumably we'll somehow use the task_result.
                return { };
            },
            error_handlers );
    }
}

////////////////////////////////////////

#ifdef BOOST_LEAF_NO_EXCEPTIONS

namespace boost
{
    BOOST_LEAF_NORETURN void throw_exception( std::exception const & e )
    {
        std::cerr << "Terminating due to a C++ exception under BOOST_LEAF_NO_EXCEPTIONS: " << e.what();
        std::terminate();
    }

    struct source_location;
    BOOST_LEAF_NORETURN void throw_exception( std::exception const & e, boost::source_location const & )
    {
        throw_exception(e);
    }
}

#endif
