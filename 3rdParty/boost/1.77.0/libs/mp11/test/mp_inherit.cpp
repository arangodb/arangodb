
//  Copyright 2015 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/core/lightweight_test_trait.hpp>
#include <boost/mp11/utility.hpp>
#include <type_traits>

struct X1 {};
struct X2 {};
struct X3 {};

int main()
{
    using boost::mp11::mp_inherit;

    BOOST_TEST_TRAIT_TRUE((std::is_base_of<X1, mp_inherit<X1, X2, X3>>));
    BOOST_TEST_TRAIT_TRUE((std::is_base_of<X2, mp_inherit<X1, X2, X3>>));
    BOOST_TEST_TRAIT_TRUE((std::is_base_of<X3, mp_inherit<X1, X2, X3>>));

    return boost::report_errors();
}
