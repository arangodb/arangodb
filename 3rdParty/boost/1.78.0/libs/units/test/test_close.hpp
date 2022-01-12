/*
Copyright 2021 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef BOOST_UNITS_TEST_CLOSE_HPP
#define BOOST_UNITS_TEST_CLOSE_HPP

#include <boost/core/lightweight_test.hpp>
#include <algorithm>
#include <limits>
#include <cmath>

/*
Provide a predicate for BOOST_TEST_WITH that is equivalent to
what was provided by the previous test framework.
*/
class close_to {
public:
    explicit close_to(double f)
        : f_(f) { }

    bool operator()(double l, double r) const {
        return std::abs(l - r) <=
            (std::max)(f_ * (std::max)(std::abs(l), std::abs(r)), 0.);
    }

private:
    double f_;
};

#define BOOST_UNITS_TEST_CLOSE(l,r,f) BOOST_TEST_WITH((l),(r),close_to((f)))

#endif
