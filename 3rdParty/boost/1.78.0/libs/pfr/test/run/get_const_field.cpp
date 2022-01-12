// Copyright (c) 2020-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#include <boost/pfr/core.hpp>

#include <boost/type_index.hpp>

#include <boost/core/lightweight_test.hpp>

namespace testing {

namespace {

struct other_anon {
    int data;
};

struct anon {
    other_anon a;
    const other_anon b;
};

void test_get_in_anon_ns_const_field() {
    anon x{{1}, {2}};

    BOOST_TEST_EQ(boost::pfr::get<0>(x).data, 1);
    auto x0_type = boost::typeindex::type_id_with_cvr<decltype(
        boost::pfr::get<0>(x)
    )>();
    // Use runtime check to make sure that Loophole fails to compile structure_tie
    BOOST_TEST_EQ(x0_type, boost::typeindex::type_id_with_cvr<other_anon&>());

    BOOST_TEST_EQ(boost::pfr::get<1>(x).data, 2);
    auto x1_type = boost::typeindex::type_id_with_cvr<decltype(
        boost::pfr::get<1>(x)
    )>();
    // Use runtime check to make sure that Loophole fails to compile structure_tie
    BOOST_TEST_EQ(x1_type, boost::typeindex::type_id_with_cvr<const other_anon&>());
}

} // anonymous namespace


} // namespace testing

int main() {
    testing::test_get_in_anon_ns_const_field();

    return boost::report_errors();
}


