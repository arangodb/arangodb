// Copyright (c) 2018 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr.hpp>
#include <boost/core/lightweight_test.hpp>

template <class T>
struct optional_like {
    T val_;

    optional_like() = default;
    template <class U> optional_like(U&& /*v*/){}
};

struct Foo {
    optional_like<int> a;
};

int main() {
    Foo f{0};
    f.a.val_ = 5;

    BOOST_TEST_EQ(boost::pfr::get<0>(f).val_, 5);
    return boost::report_errors();
}
