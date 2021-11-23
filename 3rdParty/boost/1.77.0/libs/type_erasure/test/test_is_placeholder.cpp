// Boost.TypeErasure library
//
// Copyright 2012 Steven Watanabe
//
// Distributed under the Boost Software License Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// $Id$

#include <boost/type_erasure/is_placeholder.hpp>
#include <boost/type_erasure/placeholder.hpp>
#include <boost/mpl/assert.hpp>

using namespace boost::type_erasure;

struct a_placeholder : placeholder {};
struct not_placeholder {};
struct incomplete;

template<class T>
struct t_placeholder : placeholder {};

template<class T>
struct t_incomplete;

BOOST_MPL_ASSERT((is_placeholder<a_placeholder>));
BOOST_MPL_ASSERT_NOT((is_placeholder<not_placeholder>));
BOOST_MPL_ASSERT_NOT((is_placeholder<incomplete>));
BOOST_MPL_ASSERT_NOT((is_placeholder<int>));
BOOST_MPL_ASSERT_NOT((is_placeholder<void>));
BOOST_MPL_ASSERT((is_placeholder<t_placeholder<int> >));
BOOST_MPL_ASSERT_NOT((is_placeholder<t_incomplete<int> >));
