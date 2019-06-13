
// Copyright Aleksey Gurtovoy 2000-2004
// Copyright David Abrahams 2003
//
// Distributed under the Boost Software License, Version 1.0. 
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/mpl for documentation.

// $Id$
// $Date$
// $Revision$


// This file tests that we have the right value for
// BOOST_MPL_CFG_NO_HAS_XXX, and that 'has_xxx' doesn't just fail to
// compile arbitrarily.

#include <boost/mpl/aux_/config/has_xxx.hpp>

#if defined(BOOST_MPL_CFG_NO_HAS_XXX)
#   define HAS_XXX_ASSERT(x) MPL_ASSERT_NOT(x)
#endif

#if defined(BOOST_MPL_CFG_NO_HAS_XXX_TEMPLATE)
#   define HAS_XXX_TEMPLATE_ASSERT(x) MPL_ASSERT_NOT(x)
#endif

#include "has_xxx.cpp"
