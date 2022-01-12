// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/bind/apply.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/core/is_same.hpp>

int main()
{
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<void, boost::apply<void>::result_type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<int&, boost::apply<int&>::result_type>));

    return boost::report_errors();
}
