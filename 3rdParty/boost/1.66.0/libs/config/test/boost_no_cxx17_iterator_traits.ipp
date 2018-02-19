//  Copyright (c) Andrey Semashev 2017.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/config for most recent version.

//  MACRO:         BOOST_NO_CXX17_ITERATOR_TRAITS
//  TITLE:         C++17 std::iterator_traits
//  DESCRIPTION:   The compiler does not support SFINAE-friendly std::iterator_traits defined in C++17.

#include <iterator>

namespace boost_no_cxx17_iterator_traits {

struct iterator :
    public std::iterator< std::random_access_iterator_tag, char >
{
};

struct non_iterator {};

template< typename T >
struct void_type { typedef void type; };

template< typename Traits, typename Void = void >
struct has_iterator_category
{
    enum { value = false };
};

template< typename Traits >
struct has_iterator_category< Traits, typename void_type< typename Traits::iterator_category >::type >
{
    enum { value = true };
};

int test()
{
    if (!has_iterator_category< std::iterator_traits< boost_no_cxx17_iterator_traits::iterator > >::value)
        return 1;

    if (has_iterator_category< std::iterator_traits< boost_no_cxx17_iterator_traits::non_iterator > >::value)
        return 2;

    return 0;
}

}
