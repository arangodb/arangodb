// Copyright (C) 2019 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at:
//  akrzemi1@gmail.com

#include "boost/optional/optional.hpp"

#ifdef BOOST_BORLANDC
#pragma hdrstop
#endif

#ifndef BOOST_OPTIONAL_DETAIL_NO_IS_CONSTRUCTIBLE_TRAIT
#ifndef BOOST_OPTIONAL_DETAIL_NO_SFINAE_FRIENDLY_CONSTRUCTORS
template <typename, typename>
struct void_t
{
  typedef void type;
};


template <typename T, typename = void>
struct trait
{
};

// the following trait emulates properties std::iterator_traits
template <typename T>
struct trait<T, BOOST_DEDUCED_TYPENAME void_t<BOOST_DEDUCED_TYPENAME T::value_type,
                                              BOOST_DEDUCED_TYPENAME boost::enable_if<boost::is_constructible<T, T&> >::type
                                             >::type>
{
  typedef BOOST_DEDUCED_TYPENAME T::value_type value_type;
};

// This class emulates the properties of std::filesystem::path
struct Path
{

#if __cplusplus >= 201103
    template <typename T, typename = BOOST_DEDUCED_TYPENAME trait<T>::value_type>
        Path(T const&);
#else
  template <typename T>
    Path(T const&, BOOST_DEDUCED_TYPENAME trait<T>::value_type* = 0);
#endif

};
#endif
#endif


int main()
{
#ifndef BOOST_OPTIONAL_DETAIL_NO_IS_CONSTRUCTIBLE_TRAIT
#ifndef BOOST_OPTIONAL_DETAIL_NO_SFINAE_FRIENDLY_CONSTRUCTORS

    boost::optional<Path> optFs1;
    boost::optional<Path> optFs2;

    optFs1 = optFs2;

    // the following still fails although it shouldn't
    //BOOST_STATIC_ASSERT((std::is_copy_constructible<boost::optional<Path>>::value));

#endif
#endif
}
