/*
Copyright 2018 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/align/aligned_allocator.hpp>

struct S;
struct V { };

void value_test()
{
    boost::alignment::aligned_allocator<S> a;
    (void)a;
}

void rebind_test()
{
    boost::alignment::aligned_allocator<V> a;
    boost::alignment::aligned_allocator<V>::rebind<S>::other r(a);
    (void)r;
}
