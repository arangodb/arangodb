// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe.hpp>
#include <boost/mp11.hpp>
#include <stdexcept>
#include <typeinfo>
#include <string>
#include <cstring>

[[noreturn]] void throw_invalid_name( char const * name, char const * type )
{
    throw std::runtime_error(
        std::string( "Invalid enumerator name '" ) + name
        + "' for enum type '" + type + "'" );
}

template<class E> E string_to_enum( char const * name )
{
    bool found = false;
    E r = {};

    boost::mp11::mp_for_each< boost::describe::describe_enumerators<E> >([&](auto D){

        if( !found && std::strcmp( D.name, name ) == 0 )
        {
            found = true;
            r = D.value;
        }

    });

    if( found )
    {
        return r;
    }
    else
    {
        throw_invalid_name( name, typeid( E ).name() );
    }
}

#include <iostream>

BOOST_DEFINE_ENUM(E, v1, v2, v3)

int main()
{
    try
    {
        std::cout << "v1: " << string_to_enum<E>( "v1" ) << std::endl;
        std::cout << "v2: " << string_to_enum<E>( "v2" ) << std::endl;
        std::cout << "v3: " << string_to_enum<E>( "v3" ) << std::endl;
        std::cout << "v4: " << string_to_enum<E>( "v4" ) << std::endl;
    }
    catch( std::exception const & x )
    {
        std::cout << x.what() << std::endl;
    }
}
