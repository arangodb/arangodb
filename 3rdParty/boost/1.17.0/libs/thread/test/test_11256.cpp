// Copyright (C) 2015 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_THREAD_VERSION 4
#define BOOST_THREAD_PROVIDES_EXECUTORS

#include <boost/thread.hpp>
#include <boost/thread/thread_pool.hpp>
#include <cassert>

auto createFuture()
{
    boost::promise<void> promise;
    promise.set_value();
    return promise.get_future();
}

auto stepOne(boost::basic_thread_pool &executor)
{
    auto sendFuture = createFuture();
    auto wrappedFuture = sendFuture.then(executor, [](auto f) mutable {
        return createFuture();
    });

    return wrappedFuture.unwrap();
}

auto stepTwo(boost::basic_thread_pool &executor)
{
    auto future = stepOne(executor);
    return future.then(executor, [](auto f) {
        assert(f.is_ready());
    });
}

int main()
{
    boost::basic_thread_pool executor{1};
    stepTwo(executor).get();
}
