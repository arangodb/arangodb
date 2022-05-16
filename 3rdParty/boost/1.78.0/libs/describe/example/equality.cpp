// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe.hpp>
#include <boost/mp11.hpp>
#include <boost/variant2/variant.hpp>
#include <vector>

using namespace boost::describe;

namespace app
{
template<class T,
    class Bd = describe_bases<T, mod_any_access>,
    class Md = describe_members<T, mod_any_access>>
    bool operator==( T const& t1, T const& t2 )
{
    bool r = true;

    boost::mp11::mp_for_each<Bd>([&](auto D){

        using B = typename decltype(D)::type;
        r = r && (B const&)t1 == (B const&)t2;

    });

    boost::mp11::mp_for_each<Md>([&](auto D){

        r = r && t1.*D.pointer == t2.*D.pointer;

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
    app::C c1, c2, c3;

    c1.v.push_back( app::A{} );
    c2.v.push_back( app::A{} );
    c3.v.push_back( app::B{} );

    std::cout << std::boolalpha
        << ( c1 == c2 ) << ' '
        << ( c1 == c3 ) << std::endl;
}
