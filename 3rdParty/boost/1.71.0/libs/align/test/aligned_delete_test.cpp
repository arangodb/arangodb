/*
Copyright 2014 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/align/aligned_alloc.hpp>
#include <boost/align/aligned_delete.hpp>
#include <boost/align/alignment_of.hpp>
#include <boost/core/lightweight_test.hpp>
#include <new>

class type {
public:
    static unsigned count;

    type() {
        ++count;
    }

    ~type() {
        --count;
    }

private:
    type(const type&);
    type& operator=(const type&);
};

unsigned type::count = 0;

int main()
{
    {
        void* p = boost::alignment::aligned_alloc(1, 1);
        char* q = ::new(p) char;
        boost::alignment::aligned_delete()(q);
    }
    {
        enum {
            N = boost::alignment::alignment_of<type>::value
        };
        void* p = boost::alignment::aligned_alloc(N, sizeof(type));
        type* q = ::new(p) type;
        BOOST_TEST(type::count == 1);
        boost::alignment::aligned_delete()(q);
        BOOST_TEST(type::count == 0);
    }
    return boost::report_errors();
}
