/*
(c) 2015 Glen Joseph Fernandes
<glenjofe -at- gmail.com>

Distributed under the Boost Software
License, Version 1.0.
http://boost.org/LICENSE_1_0.txt
*/
#include <boost/align/assume_aligned.hpp>

void* test1(void* p)
{
    BOOST_ALIGN_ASSUME_ALIGNED(p, 1);
    return p;
}

void* test2(void* p)
{
    BOOST_ALIGN_ASSUME_ALIGNED(p, 2);
    return p;
}

void* test4(void* p)
{
    BOOST_ALIGN_ASSUME_ALIGNED(p, 4);
    return p;
}

void* test8(void* p)
{
    BOOST_ALIGN_ASSUME_ALIGNED(p, 8);
    return p;
}

void* test16(void* p)
{
    BOOST_ALIGN_ASSUME_ALIGNED(p, 16);
    return p;
}

void* test32(void* p)
{
    BOOST_ALIGN_ASSUME_ALIGNED(p, 32);
    return p;
}

void* test64(void* p)
{
    BOOST_ALIGN_ASSUME_ALIGNED(p, 64);
    return p;
}

void* test128(void* p)
{
    BOOST_ALIGN_ASSUME_ALIGNED(p, 128);
    return p;
}

int main()
{
    return 0;
}
