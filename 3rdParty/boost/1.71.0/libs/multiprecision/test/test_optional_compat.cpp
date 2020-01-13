/*
 *  (c) Copyright Andrey Semashev 2018.
 *
 *  Use, modification and distribution are subject to the
 *  Boost Software License, Version 1.0. (See accompanying file
 *  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */
/*
 * The test verifies that Boost.Multiprecision does not cause conflict with Boost.Optional
 * because of its restricted conversion constructors and operators. See comments in:
 *
 * https://github.com/boostorg/integer/pull/11
 */

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/optional/optional.hpp>
#include <boost/core/lightweight_test.hpp>

inline boost::optional< boost::multiprecision::int128_t > foo()
{
    return boost::optional< boost::multiprecision::int128_t >(10);
}

int main()
{
    boost::optional< boost::multiprecision::int128_t > num = foo();
    BOOST_TEST(!!num);
    BOOST_TEST(*num == 10);

    return boost::report_errors();
}
