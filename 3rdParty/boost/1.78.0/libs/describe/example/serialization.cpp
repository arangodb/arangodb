// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define _CRT_SECURE_NO_WARNINGS

#include <boost/describe.hpp>
#include <boost/mp11.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/core/nvp.hpp>
#include <type_traits>
#include <cstdio>
#include <vector>

namespace app
{

template<class Archive, class T,
    class D1 = boost::describe::describe_bases<T, boost::describe::mod_public>,
    class D2 = boost::describe::describe_bases<T,
        boost::describe::mod_protected | boost::describe::mod_private>,
    class D3 = boost::describe::describe_members<T,
        boost::describe::mod_public | boost::describe::mod_protected>,
    class D4 = boost::describe::describe_members<T, boost::describe::mod_private>,
    class En = std::enable_if_t<
        boost::mp11::mp_empty<D2>::value && boost::mp11::mp_empty<D4>::value> >

    void serialize( Archive & ar, T & t, boost::serialization::version_type )
{
    int k = 0;

    // public bases: use base_object

    boost::mp11::mp_for_each<D1>([&](auto D){

        using B = typename decltype(D)::type;

        char name[ 32 ];
        std::sprintf( name, "base.%d", ++k );

        ar & boost::make_nvp( name, boost::serialization::base_object<B>( t ) );

    });

    // public (and protected) members

    boost::mp11::mp_for_each<D3>([&](auto D){

        ar & boost::make_nvp( D.name, t.*D.pointer );

    });
}

struct A1
{
    int x;
};

BOOST_DESCRIBE_STRUCT(A1, (), (x))

struct A2
{
    int y;
};

BOOST_DESCRIBE_STRUCT(A2, (), (y))

struct B: public A1, public A2
{
    // these constructors aren't needed in C++17
    B(): A1(), A2() {}
    B( int x, int y ): A1{ x }, A2{ y } {}
};

BOOST_DESCRIBE_STRUCT(B, (A1, A2), ())

struct C
{
    std::vector<B> v;
};

BOOST_DESCRIBE_STRUCT(C, (), (v))

} // namespace app

#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <string>
#include <sstream>
#include <iostream>

int main()
{
    app::C c1{ { { 1, 2 }, { 3, 4 }, { 5, 6 } } };

    std::ostringstream os;

    {
        boost::archive::xml_oarchive ar( os );
        ar << boost::make_nvp( "c1", c1 );
    }

    std::string s = os.str();

    std::cout << s << std::endl;

    app::C c2;

    {
        std::istringstream is( s );
        boost::archive::xml_iarchive ar( is );
        ar >> boost::make_nvp( "c2", c2 );
    }

    {
        boost::archive::text_oarchive ar( std::cout );
        ar << c2;
    }
}
