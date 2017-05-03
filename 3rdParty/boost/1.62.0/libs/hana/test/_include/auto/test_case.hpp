// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_TEST_CASE_HPP
#define BOOST_HANA_TEST_AUTO_TEST_CASE_HPP

struct TestCase {
    template <typename F>
    explicit TestCase(F const& f) { f(); }
};

#endif // !BOOST_HANA_TEST_AUTO_TEST_CASE_HPP
