/*
Copyright 2007 Tobias Schwinger

Copyright 2019 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/functional/value_factory.hpp>
#include <boost/core/lightweight_test.hpp>

class sum {
public:
    explicit sum(int i0 = 0, int i1 = 0, int i2 = 0, int i3 = 0,
                 int i4 = 0, int i5 = 0, int i6 = 0, int i7 = 0,
                 int i8 = 0, int i9 = 0)
        : value_(i0 + i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8 + i9) { }

    int get() const {
        return value_;
    }

private:
    int value_;
};

int main()
{
    boost::value_factory<sum> x;
    int a = 1;
    int b = 2;
    int c = 3;
    int d = 4;
    int e = 5;
    int f = 6;
    int g = 7;
    int h = 8;
    int i = 9;
    int j = 10;
    {
        sum s(x());
        BOOST_TEST(s.get() == 0);
    }
    {
        sum s(x(a));
        BOOST_TEST(s.get() == 1);
    }
    {
        sum s(x(a, b));
        BOOST_TEST(s.get() == 3);
    }
    {
        sum s(x(a, b, c));
        BOOST_TEST(s.get() == 6);
    }
    {
        sum s(x(a, b, c, d));
        BOOST_TEST(s.get() == 10);
    }
    {
        sum s(x(a, b, c, d, e));
        BOOST_TEST(s.get() == 15);
    }
    {
        sum s(x(a, b, c, d, e, f));
        BOOST_TEST(s.get() == 21);
    }
    {
        sum s(x(a, b, c, d, e, f, g));
        BOOST_TEST(s.get() == 28);
    }
    {
        sum s(x(a, b, c, d, e, f, g, h));
        BOOST_TEST(s.get() == 36);
    }
    {
        sum s(x(a, b, c, d, e, f, g, h, i));
        BOOST_TEST(s.get() == 45);
    }
    {
        sum s(x(a, b, c, d, e, f, g, h, i, j));
        BOOST_TEST(s.get() == 55);
    }
    return boost::report_errors();
}
