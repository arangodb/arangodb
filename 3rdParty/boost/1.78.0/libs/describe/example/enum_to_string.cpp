// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe.hpp>
#include <boost/mp11.hpp>

template<class E> char const * enum_to_string( E e )
{
    char const * r = "(unnamed)";

    boost::mp11::mp_for_each< boost::describe::describe_enumerators<E> >([&](auto D){

        if( e == D.value ) r = D.name;

    });

    return r;
}

#include <iostream>

enum E
{
    v1 = 3,
    v2,
    v3 = 11
};

BOOST_DESCRIBE_ENUM(E, v1, v2, v3)

int main()
{
    std::cout << "E(" << v1 << "): " << enum_to_string( v1 ) << std::endl;
    std::cout << "E(" << 0 << "): " << enum_to_string( E(0) ) << std::endl;
}
