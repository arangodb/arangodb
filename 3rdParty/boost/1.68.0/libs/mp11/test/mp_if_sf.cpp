
//  Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/utility.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

int main()
{
    using boost::mp11::mp_if;
    using boost::mp11::mp_valid;

    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_if, std::false_type, void>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<mp_if, std::false_type, void, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_if, std::false_type, void, void, void>));

    return boost::report_errors();
}
