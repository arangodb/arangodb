/*=============================================================================
    Copyright (c) 2015,2018 Kohei Takahashi

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/config.hpp>

#ifdef BOOST_NO_CXX11_VARIADIC_TEMPLATES
#   error "does not meet requirements"
#endif

#include <boost/fusion/support/detail/index_sequence.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/type_traits/is_same.hpp>

using namespace boost::fusion;

BOOST_MPL_ASSERT((boost::is_same<detail::make_index_sequence<0>::type, detail::index_sequence<> >));
BOOST_MPL_ASSERT((boost::is_same<detail::make_index_sequence<1>::type, detail::index_sequence<0> >));
BOOST_MPL_ASSERT((boost::is_same<detail::make_index_sequence<2>::type, detail::index_sequence<0, 1> >));
BOOST_MPL_ASSERT((boost::is_same<detail::make_index_sequence<3>::type, detail::index_sequence<0, 1, 2> >));
BOOST_MPL_ASSERT((boost::is_same<detail::make_index_sequence<4>::type, detail::index_sequence<0, 1, 2, 3> >));
BOOST_MPL_ASSERT((boost::is_same<detail::make_index_sequence<5>::type, detail::index_sequence<0, 1, 2, 3, 4> >));
BOOST_MPL_ASSERT((boost::is_same<detail::make_index_sequence<6>::type, detail::index_sequence<0, 1, 2, 3, 4, 5> >));
BOOST_MPL_ASSERT((boost::is_same<detail::make_index_sequence<7>::type, detail::index_sequence<0, 1, 2, 3, 4, 5, 6> >));
BOOST_MPL_ASSERT((boost::is_same<detail::make_index_sequence<8>::type, detail::index_sequence<0, 1, 2, 3, 4, 5, 6, 7> >));

BOOST_MPL_ASSERT((boost::is_same<detail::make_index_sequence<15>::type, detail::index_sequence<0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14> >));
BOOST_MPL_ASSERT((boost::is_same<detail::make_index_sequence<16>::type, detail::index_sequence<0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15> >));
BOOST_MPL_ASSERT((boost::is_same<detail::make_index_sequence<17>::type, detail::index_sequence<0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16> >));

