/*
Copyright 2018 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/config.hpp>
#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
#include <boost/core/exchange.hpp>
#include <boost/core/lightweight_test.hpp>

class C1 {
public:
    explicit C1(int i)
        : i_(i) { }
    C1(C1&& c)
        : i_(c.i_) { }
    C1& operator=(C1&& c) {
        i_ = c.i_;
        return *this;
    }
    int i() const {
        return i_;
    }
private:
    C1(const C1&);
    C1& operator=(const C1&);
    int i_;
};

void test1()
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
    C2(const C2&);
    C2& operator=(const C2&);
    int i_;
};

void test2()
{
    C1 x(1);
    BOOST_TEST(boost::exchange(x, C2(2)).i() == 1);
    BOOST_TEST(x.i() == 2);
}

class C3 {
public:
    explicit C3(int i)
        : i_(i) { }
    C3(C3&& c)
        : i_(c.i_) { }
    C3& operator=(C1&& c) {
        i_ = c.i();
        return *this;
    }
    int i() const {
        return i_;
    }
private:
    C3(const C3&);
    C3& operator=(const C3&);
    int i_;
};

void test3()
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
    return boost::report_errors();
}
#else
int main()
{
    return 0;
}
#endif
