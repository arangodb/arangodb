/*
Copyright 2019 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/alloc_construct.hpp>
#include <boost/core/default_allocator.hpp>
#include <boost/core/lightweight_test.hpp>

class type {
public:
    type() {
        if (count == 4) {
            throw true;
        }
        ++count;
    }

    ~type() {
        --count;
    }

    static int count;

private:
    type(const type&);
    type& operator=(const type&);
};

int type::count = 0;

int main()
{
    boost::default_allocator<type> a;
    type* p = a.allocate(5);
    try {
        boost::alloc_construct_n(a, p, 5);
        BOOST_ERROR("construct_n did not throw");
    } catch (...) {
        BOOST_TEST_EQ(type::count, 0);
    }
    a.deallocate(p, 5);
    return boost::report_errors();
}
