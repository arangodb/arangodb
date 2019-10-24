//  Copyright (c) 2018 Mike Dev
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  https://www.boost.org/LICENSE_1_0.txt)

#include <boost/atomic.hpp>

struct Dummy
{
    int x[128];
};

int main()
{
    Dummy d = {};
    boost::atomic<Dummy> ad;

    // this operation requires functions from
    // the compiled part of Boost.Atomic
    ad = d;
}
