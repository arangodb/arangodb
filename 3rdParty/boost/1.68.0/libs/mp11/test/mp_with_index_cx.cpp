
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/config.hpp>

#if defined( BOOST_NO_CXX14_CONSTEXPR )

int main() {}

#else

#include <boost/mp11/algorithm.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>

using boost::mp11::mp_size_t;
using boost::mp11::mp_with_index;

struct F
{
    template<std::size_t I> constexpr std::size_t operator()( mp_size_t<I> ) const
    {
        return I;
    }
};

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

int main()
{
    constexpr std::size_t i = mp_with_index<64>( 57, F{} );
    STATIC_ASSERT( i == 57 );
}

#endif
