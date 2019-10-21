// Boost.TypeErasure library
//
// Copyright 2011 Steven Watanabe
//
// Distributed under the Boost Software License Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// $Id$

#include <boost/type_erasure/any.hpp>
#include <boost/type_erasure/builtin.hpp>
#include <boost/type_traits/is_constructible.hpp>
#include <boost/type_traits/is_copy_constructible.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/assert.hpp>

using namespace boost::type_erasure;

#if defined(BOOST_TYPE_ERASURE_SFINAE_FRIENDLY_CONSTRUCTORS)

using boost::is_copy_constructible;

template<class T>
using is_move_constructible = boost::is_constructible<T, T&&>;

template<class T>
using is_mutable_constructible = boost::is_constructible<T, T&>;

using move_constructible = boost::mpl::vector<constructible<_self(_self&&)>, destructible<> >;
using mutable_copy = boost::mpl::vector<constructible<_self(_self&)>, destructible<> >;

BOOST_MPL_ASSERT_NOT((is_copy_constructible<any<destructible<> > >));
BOOST_MPL_ASSERT((is_copy_constructible<any<copy_constructible<> > >));
BOOST_MPL_ASSERT_NOT((is_copy_constructible<any<move_constructible> >));

// only is_copy_constructible seems to work on msvc and
// even that breaks when we add a non-const copy constructor.
#if !BOOST_WORKAROUND(BOOST_MSVC, BOOST_TESTED_AT(1912)) && \
    !BOOST_WORKAROUND(BOOST_GCC, < 70000) && \
    !BOOST_WORKAROUND(__clang__major__, < 4)

BOOST_MPL_ASSERT_NOT((is_mutable_constructible<any<destructible<> > >));
BOOST_MPL_ASSERT_NOT((is_move_constructible<any<destructible<> > >));

BOOST_MPL_ASSERT((is_mutable_constructible<any<copy_constructible<> > >));
BOOST_MPL_ASSERT((is_move_constructible<any<copy_constructible<> > >));

BOOST_MPL_ASSERT_NOT((is_mutable_constructible<any<move_constructible> >));
BOOST_MPL_ASSERT((is_move_constructible<any<move_constructible> >));

BOOST_MPL_ASSERT((is_mutable_constructible<any<mutable_copy> >));
BOOST_MPL_ASSERT_NOT((is_copy_constructible<any<mutable_copy> >));
BOOST_MPL_ASSERT_NOT((is_move_constructible<any<mutable_copy> >));

#endif

#endif
