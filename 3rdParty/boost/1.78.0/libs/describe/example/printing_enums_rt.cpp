// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe.hpp>
#include <cstdio>
#include <array>

template<class E> struct enum_descriptor
{
    E value;
    char const * name;
};

template<class E, template<class... T> class L, class... T>
  constexpr std::array<enum_descriptor<E>, sizeof...(T)>
    describe_enumerators_as_array_impl( L<T...> )
{
    return { { { T::value, T::name }... } };
}

template<class E> constexpr auto describe_enumerators_as_array()
{
    return describe_enumerators_as_array_impl<E>( boost::describe::describe_enumerators<E>() );
}

BOOST_DEFINE_ENUM(E, v1, v2, v3, v4, v5, v6)

int main()
{
    constexpr auto D = describe_enumerators_as_array<E>();

    for( auto const& x: D )
    {
        std::printf( "%s: %d\n", x.name, x.value );
    }
}
