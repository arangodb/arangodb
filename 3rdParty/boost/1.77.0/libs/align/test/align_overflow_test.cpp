/*
Copyright 2020 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/align/align.hpp>
#include <boost/core/lightweight_test.hpp>

int main()
{
    static const std::size_t n = -1;
    {
        void* p = reinterpret_cast<void*>(5);
        std::size_t s = 3072;
        BOOST_TEST(!boost::alignment::align(1024, n, p, s));
    }
    {
        void* p = reinterpret_cast<void*>(1);
        std::size_t s = -1;
        BOOST_TEST(!boost::alignment::align(2, n, p, s));
    }
    return boost::report_errors();
}
