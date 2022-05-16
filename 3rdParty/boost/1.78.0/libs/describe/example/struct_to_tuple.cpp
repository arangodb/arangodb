// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe.hpp>
#include <tuple>

namespace desc = boost::describe;

template<class T, template<class...> class L, class... D>
auto struct_to_tuple_impl( T const& t, L<D...> )
{
    return std::make_tuple( t.*D::pointer... );
}

template<class T, class Dm = desc::describe_members<T,
    desc::mod_public | desc::mod_inherited>>
auto struct_to_tuple( T const& t )
{
    return struct_to_tuple_impl( t, Dm() );
}

#include <boost/core/type_name.hpp>
#include <iostream>

struct X
{
    int a = 1;
};

BOOST_DESCRIBE_STRUCT(X, (), (a))

struct Y
{
    float b = 3.14f;
};

BOOST_DESCRIBE_STRUCT(Y, (), (b))

struct Z: X, Y
{
};

BOOST_DESCRIBE_STRUCT(Z, (X, Y), ())

int main()
{
    Z z;

    auto tp = struct_to_tuple( z );

    std::cout <<
        boost::core::type_name<decltype(tp)>() << ": "
        << std::get<0>(tp) << ", " << std::get<1>(tp);
}
