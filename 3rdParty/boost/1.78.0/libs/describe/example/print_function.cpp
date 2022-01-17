// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe.hpp>
#include <boost/mp11.hpp>
#include <ostream>

using namespace boost::describe;

template<class T,
    class Bd = describe_bases<T, mod_any_access>,
    class Md = describe_members<T, mod_any_access>>
    std::ostream& operator<<( std::ostream & os, T const & t )
{
    os << "{";

    bool first = true;

    boost::mp11::mp_for_each<Bd>([&](auto D){

        if( !first ) { os << ", "; } first = false;

        using B = typename decltype(D)::type;
        os << (B const&)t;

    });

    boost::mp11::mp_for_each<Md>([&](auto D){

        if( !first ) { os << ", "; } first = false;

        os << "." << D.name << " = " << t.*D.pointer;

    });

    os << "}";
    return os;
}

struct X
{
    int m1 = 1;
};

BOOST_DESCRIBE_STRUCT(X, (), (m1))

struct Y
{
    int m2 = 2;
};

BOOST_DESCRIBE_STRUCT(Y, (), (m2))

class Z: public X, private Y
{
    int m1 = 3;
    int m2 = 4;

    BOOST_DESCRIBE_CLASS(Z, (X, Y), (), (), (m1, m2))
};

#include <iostream>

int main()
{
    std::cout << Z() << std::endl;
}
