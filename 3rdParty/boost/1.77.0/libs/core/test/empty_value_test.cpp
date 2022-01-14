/*
Copyright 2018 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/empty_value.hpp>
#include <boost/core/lightweight_test.hpp>

struct empty {
    int value() const {
        return 1;
    }
    int value() {
        return 2;
    }
};

class type {
public:
    explicit type(int count)
        : value_(count) { }
    int value() const {
        return value_ + 1;
    }
    int value() {
        return value_ + 2;
    }
private:
    int value_;
};

void test_int()
{
    const boost::empty_value<int> v1(boost::empty_init_t(), 7);
    BOOST_TEST(v1.get() == 7);
    boost::empty_value<int> v2 = boost::empty_init_t();
    BOOST_TEST(v2.get() == 0);
    v2 = v1;
    BOOST_TEST(v2.get() == 7);
    v2.get() = 8;
    BOOST_TEST(v2.get() == 8);
}

void test_empty()
{
    const boost::empty_value<empty> v1 = boost::empty_init_t();
    BOOST_TEST(v1.get().value() == 1);
    boost::empty_value<empty> v2;
    BOOST_TEST(v2.get().value() == 2);
}

void test_type()
{
    const boost::empty_value<type> v1(boost::empty_init_t(), 2);
    BOOST_TEST(v1.get().value() == 3);
    boost::empty_value<type> v2(boost::empty_init_t(), 3);
    BOOST_TEST(v2.get().value() == 5);
    v2 = v1;
    BOOST_TEST(v2.get().value() == 4);
    v2.get() = type(4);
    BOOST_TEST(v2.get().value() == 6);
}

int main()
{
    test_int();
    test_empty();
    test_type();
    return boost::report_errors();
}
