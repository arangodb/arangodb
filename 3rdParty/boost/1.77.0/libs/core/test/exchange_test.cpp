/*
Copyright 2018 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/exchange.hpp>
#include <boost/core/lightweight_test.hpp>

void test1()
{
    int i = 1;
    BOOST_TEST(boost::exchange(i, 2) == 1);
    BOOST_TEST(i == 2);
}

class C1 {
public:
    explicit C1(int i)
        : i_(i) { }
    int i() const {
        return i_;
    }
private:
    int i_;
};

void test2()
{
    C1 x(1);
    BOOST_TEST(boost::exchange(x, C1(2)).i() == 1);
    BOOST_TEST(x.i() == 2);
}

class C2 {
public:
    explicit C2(int i)
        : i_(i) { }
    operator C1() const {
        return C1(i_);
    }
    int i() const {
        return i_;
    }
private:
    int i_;
};

void test3()
{
    C1 x(1);
    BOOST_TEST(boost::exchange(x, C2(2)).i() == 1);
    BOOST_TEST(x.i() == 2);
}

class C3 {
public:
    explicit C3(int i)
        : i_(i) { }
    C3(const C3& c)
        : i_(c.i_) { }
    C3& operator=(const C1& c) {
        i_ = c.i();
        return *this;
    }
    int i() const {
        return i_;
    }
private:
    C3& operator=(const C3&);
    int i_;
};

void test4()
{
    C3 x(1);
    BOOST_TEST(boost::exchange(x, C1(2)).i() == 1);
    BOOST_TEST(x.i() == 2);
}

int main()
{
    test1();
    test2();
    test3();
    test4();
    return boost::report_errors();
}
