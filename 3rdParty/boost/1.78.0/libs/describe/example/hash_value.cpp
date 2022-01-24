// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe.hpp>
#include <boost/mp11.hpp>
#include <boost/container_hash/hash.hpp>
#include <boost/variant2/variant.hpp>
#include <vector>

using namespace boost::describe;

namespace app
{

template<class T,
    class Bd = describe_bases<T, mod_any_access>,
    class Md = describe_members<T, mod_any_access>>
    std::size_t hash_value( T const & t )
{
    std::size_t r = 0;

    boost::mp11::mp_for_each<Bd>([&](auto D){

        using B = typename decltype(D)::type;
        boost::hash_combine( r, (B const&)t );

    });

    boost::mp11::mp_for_each<Md>([&](auto D){

        boost::hash_combine( r, t.*D.pointer );

    });

    return r;
}

struct A
{
    int x = 1;
};

BOOST_DESCRIBE_STRUCT(A, (), (x))

struct B
{
    int y = 2;
};

BOOST_DESCRIBE_STRUCT(B, (), (y))

struct C
{
    std::vector<boost::variant2::variant<A, B>> v;
};

BOOST_DESCRIBE_STRUCT(C, (), (v))

} // namespace app

#include <iostream>

int main()
{
    app::C c;

    c.v.push_back( app::A{} );
    c.v.push_back( app::B{} );

    std::cout << boost::hash<app::C>()( c ) << std::endl;
}
