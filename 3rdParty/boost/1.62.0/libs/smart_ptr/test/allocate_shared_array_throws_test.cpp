/*
(c) 2012-2015 Glen Joseph Fernandes
<glenjofe -at- gmail.com>

Distributed under the Boost Software
License, Version 1.0.
http://boost.org/LICENSE_1_0.txt
*/
#include <boost/core/lightweight_test.hpp>
#include <boost/smart_ptr/make_shared.hpp>

class type {
public:
    static unsigned int instances;
    explicit type() {
        if (instances == 5) {
            throw true;
        }
        instances++;
    }
    ~type() {
        instances--;
    }
private:
    type(const type&);
    type& operator=(const type&);
};

unsigned int type::instances = 0;

int main()
{
    try {
        boost::allocate_shared<type[]>(std::allocator<type>(), 6);
        BOOST_ERROR("allocate_shared did not throw");
    } catch (...) {
        BOOST_TEST(type::instances == 0);
    }
    try {
        boost::allocate_shared<type[][2]>(std::allocator<type>(), 3);
        BOOST_ERROR("allocate_shared did not throw");
    } catch (...) {
        BOOST_TEST(type::instances == 0);
    }
    try {
        boost::allocate_shared<type[6]>(std::allocator<type>());
        BOOST_ERROR("allocate_shared did not throw");
    } catch (...) {
        BOOST_TEST(type::instances == 0);
    }
    try {
        boost::allocate_shared<type[3][2]>(std::allocator<type>());
        BOOST_ERROR("allocate_shared did not throw");
    } catch (...) {
        BOOST_TEST(type::instances == 0);
    }
    try {
        boost::allocate_shared_noinit<type[]>(std::allocator<type>(), 6);
        BOOST_ERROR("allocate_shared_noinit did not throw");
    } catch (...) {
        BOOST_TEST(type::instances == 0);
    }
    try {
        boost::allocate_shared_noinit<type[][2]>(std::allocator<type>(), 3);
        BOOST_ERROR("allocate_shared_noinit did not throw");
    } catch (...) {
        BOOST_TEST(type::instances == 0);
    }
    try {
        boost::allocate_shared_noinit<type[6]>(std::allocator<type>());
        BOOST_ERROR("allocate_shared_noinit did not throw");
    } catch (...) {
        BOOST_TEST(type::instances == 0);
    }
    try {
        boost::allocate_shared_noinit<type[3][2]>(std::allocator<type>());
        BOOST_ERROR("allocate_shared_noinit did not throw");
    } catch (...) {
        BOOST_TEST(type::instances == 0);
    }
    return boost::report_errors();
}
