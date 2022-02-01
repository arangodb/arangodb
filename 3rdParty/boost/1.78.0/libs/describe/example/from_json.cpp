// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe.hpp>
#include <boost/mp11.hpp>
#include <boost/json.hpp>
#include <type_traits>

namespace app
{

template<class T> void extract( boost::json::object const & obj, char const * name, T & value )
{
    value = boost::json::value_to<T>( obj.at( name ) );
}

template<class T,
    class D1 = boost::describe::describe_members<T,
        boost::describe::mod_public | boost::describe::mod_protected>,
    class D2 = boost::describe::describe_members<T, boost::describe::mod_private>,
    class En = std::enable_if_t<boost::mp11::mp_empty<D2>::value> >

    T tag_invoke( boost::json::value_to_tag<T> const&, boost::json::value const& v )
{
    auto const& obj = v.as_object();

    T t{};

    boost::mp11::mp_for_each<D1>([&](auto D){

        extract( obj, D.name, t.*D.pointer );

    });

    return t;
}

struct A
{
    int x;
    int y;
};

BOOST_DESCRIBE_STRUCT(A, (), (x, y))

} // namespace app

#include <iostream>

int main()
{
    boost::json::value jv{ { "x", 1 }, { "y", 2 } };

    std::cout << "jv: " << jv << std::endl;

    auto a = boost::json::value_to<app::A>( jv );

    std::cout << "a: { " << a.x << ", " << a.y << " }" << std::endl;
}
