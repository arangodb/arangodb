/*=============================================================================
    Copyright (c) 2016 Lee Clagett
    Copyright (c) 2018 Kohei Takahashi
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/config.hpp>

#ifdef BOOST_NO_CXX11_VARIADIC_TEMPLATES
#   error "does not meet requirements"
#endif

#include <boost/fusion/support/detail/and.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/mpl/assert.hpp>

using namespace boost;
using namespace boost::fusion::detail;

BOOST_MPL_ASSERT((and_<>));
BOOST_MPL_ASSERT_NOT((and_<false_type>));
BOOST_MPL_ASSERT((and_<true_type>));
BOOST_MPL_ASSERT_NOT((and_<true_type, false_type>));
BOOST_MPL_ASSERT((and_<true_type, true_type>));
BOOST_MPL_ASSERT_NOT((and_<true_type, true_type, false_type>));
BOOST_MPL_ASSERT((and_<true_type, true_type, true_type>));
BOOST_MPL_ASSERT((and_<true_type, mpl::true_>));
