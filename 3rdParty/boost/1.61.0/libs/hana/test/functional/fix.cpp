// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana.hpp>

#include <boost/hana/functional/fix.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/functional/always.hpp>
#include <boost/hana/integral_constant.hpp>
using namespace boost::hana;


BOOST_HANA_CONSTEXPR_LAMBDA auto fact = fix([](auto fact, auto n) {
    return eval_if(equal(n, ullong_c<0>),
        always(ullong_c<1>),
        [=](auto _) { return mult(n, fact(_(n) - ullong_c<1>)); }
    );
});

constexpr unsigned long long reference(unsigned long long n)
{ return n == 0 ? 1 : n * reference(n - 1); }

template <int n>
void test() {
    BOOST_HANA_CONSTANT_CHECK(equal(
        fact(ullong_c<n>),
        ullong_c<reference(n)>
    ));
    test<n - 1>();
}

template <> void test<-1>() { }

int main() {
    test<15>();
}
