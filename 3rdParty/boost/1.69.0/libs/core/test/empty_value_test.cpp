/*
Copyright 2018 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/empty_value.hpp>
#include <boost/core/lightweight_test.hpp>

struct empty {
    operator bool() const {
        return false;
    }
    operator bool() {
        return true;
    }
};

class type {
public:
    type()
        : value_(false) { }
    explicit type(bool value)
        : value_(value) { }
    operator bool() const {
        return value_;
    }
private:
    bool value_;
};

void test_bool()
{
    const boost::empty_value<bool> v1(boost::empty_init_t(), true);
    BOOST_TEST(v1.get());
    boost::empty_value<bool> v2 = boost::empty_init_t();
    BOOST_TEST(!v2.get());
    v2 = v1;
    BOOST_TEST(v2.get());
    v2.get() = false;
    BOOST_TEST(!v2.get());
}

void test_empty()
{
    empty e;
    const boost::empty_value<empty> v1(boost::empty_init_t(), e);
    BOOST_TEST(!v1.get());
    boost::empty_value<empty> v2;
    BOOST_TEST(v2.get());
    v2 = v1;
    BOOST_TEST(v2.get());
    v2.get() = empty();
    BOOST_TEST(v2.get());
}

void test_type()
{
    const boost::empty_value<type> v1(boost::empty_init_t(), true);
    BOOST_TEST(v1.get());
    boost::empty_value<type> v2;
    BOOST_TEST(!v2.get());
    v2 = v1;
    BOOST_TEST(v2.get());
    v2.get() = type();
    BOOST_TEST(!v2.get());
}

int main()
{
    test_bool();
    test_empty();
    test_type();
    return boost::report_errors();
}
