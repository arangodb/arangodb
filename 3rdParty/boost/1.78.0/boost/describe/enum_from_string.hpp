#ifndef BOOST_DESCRIBE_ENUM_FROM_STRING_HPP_INCLUDED
#define BOOST_DESCRIBE_ENUM_FROM_STRING_HPP_INCLUDED

// Copyright 2020, 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/detail/config.hpp>

#if defined(BOOST_DESCRIBE_CXX14)

#include <boost/describe/enumerators.hpp>
#include <boost/mp11/algorithm.hpp>
#include <cstring>

namespace boost
{
namespace describe
{

template<class E, class De = describe_enumerators<E>>
bool enum_from_string( char const* name, E& e ) noexcept
{
    bool found = false;

    mp11::mp_for_each<De>([&](auto D){

        if( !found && std::strcmp( D.name, name ) == 0 )
        {
            found = true;
            e = D.value;
        }

    });

    return found;
}

} // namespace describe
} // namespace boost

#endif // defined(BOOST_DESCRIBE_CXX14)

#endif // #ifndef BOOST_DESCRIBE_ENUM_FROM_STRING_HPP_INCLUDED
