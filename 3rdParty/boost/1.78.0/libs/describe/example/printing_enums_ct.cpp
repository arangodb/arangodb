// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe.hpp>
#include <boost/mp11.hpp>
#include <cstdio>

enum E
{
    v1 = 11,
    v2,
    v3 = 5
};

BOOST_DESCRIBE_ENUM(E, v1, v2, v3)

int main()
{
    boost::mp11::mp_for_each< boost::describe::describe_enumerators<E> >([](auto D){

        std::printf( "%s: %d\n", D.name, D.value );

    });
}
