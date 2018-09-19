
// Copyright 2015, 2017 Peter Dimov.
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
    using boost::mp11::mp_identity;
    using boost::mp11::mp_quote_trait;
    using boost::mp11::mp_invoke;

    {
        using Q = mp_quote_trait<mp_identity>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<Q::fn<void>, void>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<Q::fn<int[]>, int[]>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q, void>, void>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q, int[]>, int[]>));
    }

    {
        using Q = mp_quote_trait<std::add_pointer>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<Q::fn<void>, void*>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<Q::fn<int[]>, int(*)[]>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q, void>, void*>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q, int[]>, int(*)[]>));
    }

    {
        using Q = mp_quote_trait<std::add_const>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<Q::fn<void>, void const>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<Q::fn<int[]>, int const[]>));

#if !BOOST_WORKAROUND( BOOST_GCC, < 40900 )

        // g++ 4.7, 4.8 have difficulties with preserving top-level const

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q, void>, void const>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q, int[]>, int const[]>));

#endif
    }

    return boost::report_errors();
}
