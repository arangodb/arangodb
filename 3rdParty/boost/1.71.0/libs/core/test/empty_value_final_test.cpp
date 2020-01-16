/*
Copyright 2018 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/empty_value.hpp>
#include <boost/core/lightweight_test.hpp>

#if !defined(BOOST_NO_CXX11_FINAL)
class type final {
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

struct empty final {
    int value() const {
        return 1;
    }
    int value() {
        return 2;
    }
};

void test_type()
{
    const boost::empty_value<type> v1(boost::empty_init_t(), 3);
    BOOST_TEST(v1.get().value() == 4);
    boost::empty_value<type> v2(boost::empty_init_t(), 3);
    BOOST_TEST(v2.get().value() == 5);
}

void test_empty()
{
    const boost::empty_value<empty> v1 = boost::empty_init_t();
    BOOST_TEST(v1.get().value() == 1);
    boost::empty_value<empty> v2;
    BOOST_TEST(v2.get().value() == 2);
}

int main()
{
    test_type();
    test_empty();
    return boost::report_errors();
}
#else
int main()
{
    return 0;
}
#endif
