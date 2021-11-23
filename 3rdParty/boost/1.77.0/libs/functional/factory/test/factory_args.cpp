/*
Copyright 2019 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/config.hpp>
#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES) && \
    !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
#include <boost/functional/factory.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/smart_ptr/scoped_ptr.hpp>

class sum {
public:
    explicit sum(int a = 0, int b = 0, int c = 0, int d = 0,
                 int e = 0, int f = 0, int g = 0, int h = 0,
                 int i = 0, int j = 0, int k = 0, int l = 0)
        : value_(a + b + c + d + e + f + g + h + i + j + k + l) { }

    int get() const {
        return value_;
    }

private:
    int value_;
};

int main()
{
    boost::scoped_ptr<sum> s(boost::factory<sum*>()(1, 2, 3, 4, 5, 6, 7, 8, 9,
        10, 11, 12));
    BOOST_TEST(s->get() == 78);
    return boost::report_errors();
}
#else
int main()
{
    return 0;
}
#endif
