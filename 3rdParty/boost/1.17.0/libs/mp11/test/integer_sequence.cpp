
//  Copyright 2015 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/integer_sequence.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

int main()
{
    using boost::mp11::integer_sequence;
    using boost::mp11::make_integer_sequence;
    using boost::mp11::index_sequence;
    using boost::mp11::make_index_sequence;
    using boost::mp11::index_sequence_for;

    BOOST_TEST_TRAIT_TRUE((std::is_same<make_integer_sequence<int, 0>, integer_sequence<int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<make_integer_sequence<int, 1>, integer_sequence<int, 0>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<make_integer_sequence<int, 2>, integer_sequence<int, 0, 1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<make_integer_sequence<int, 3>, integer_sequence<int, 0, 1, 2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<make_integer_sequence<int, 4>, integer_sequence<int, 0, 1, 2, 3>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<make_integer_sequence<char, 0>, integer_sequence<char>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<make_integer_sequence<char, 1>, integer_sequence<char, 0>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<make_integer_sequence<char, 2>, integer_sequence<char, 0, 1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<make_integer_sequence<char, 3>, integer_sequence<char, 0, 1, 2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<make_integer_sequence<char, 4>, integer_sequence<char, 0, 1, 2, 3>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<make_integer_sequence<std::size_t, 0>, integer_sequence<std::size_t>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<make_integer_sequence<std::size_t, 1>, integer_sequence<std::size_t, 0>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<make_integer_sequence<std::size_t, 2>, integer_sequence<std::size_t, 0, 1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<make_integer_sequence<std::size_t, 3>, integer_sequence<std::size_t, 0, 1, 2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<make_integer_sequence<std::size_t, 4>, integer_sequence<std::size_t, 0, 1, 2, 3>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<make_index_sequence<0>, integer_sequence<std::size_t>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<make_index_sequence<1>, integer_sequence<std::size_t, 0>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<make_index_sequence<2>, integer_sequence<std::size_t, 0, 1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<make_index_sequence<3>, integer_sequence<std::size_t, 0, 1, 2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<make_index_sequence<4>, integer_sequence<std::size_t, 0, 1, 2, 3>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<make_index_sequence<0>, index_sequence<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<make_index_sequence<1>, index_sequence<0>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<make_index_sequence<2>, index_sequence<0, 1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<make_index_sequence<3>, index_sequence<0, 1, 2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<make_index_sequence<4>, index_sequence<0, 1, 2, 3>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<index_sequence_for<>, index_sequence<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<index_sequence_for<void>, index_sequence<0>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<index_sequence_for<void, void>, index_sequence<0, 1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<index_sequence_for<void, void, void>, index_sequence<0, 1, 2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<index_sequence_for<void, void, void, void>, index_sequence<0, 1, 2, 3>>));

    return boost::report_errors();
}
