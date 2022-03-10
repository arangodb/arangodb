// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe.hpp>
#include <boost/mp11.hpp>
#include <boost/json.hpp>
#include <type_traits>
#include <vector>
#include <map>

namespace app
{

template<class T,
    class D1 = boost::describe::describe_members<T,
        boost::describe::mod_public | boost::describe::mod_protected>,
    class D2 = boost::describe::describe_members<T, boost::describe::mod_private>,
    class En = std::enable_if_t<boost::mp11::mp_empty<D2>::value> >

    void tag_invoke( boost::json::value_from_tag const&, boost::json::value& v, T const & t )
{
    auto& obj = v.emplace_object();

    boost::mp11::mp_for_each<D1>([&](auto D){

        obj[ D.name ] = boost::json::value_from( t.*D.pointer );

    });
}

struct A
{
    int x;
    int y;
};

BOOST_DESCRIBE_STRUCT(A, (), (x, y))

struct B
{
    std::vector<A> v;
    std::map<std::string, A> m;
};

BOOST_DESCRIBE_STRUCT(B, (), (v, m))

} // namespace app

#include <iostream>

int main()
{
    app::B b{ { { 1, 2 }, { 3, 4 } }, { { "k1", { 5, 6 } }, { "k2", { 7, 8 } } } };

    std::cout << boost::json::value_from( b ) << std::endl;
}
