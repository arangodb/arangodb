
// Copyright Robin Linden 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/mpl for documentation.

// $Id$
// $Date$
// $Revision$

// Part of a test to demonstrate a linking error with
// BOOST_MPL_ASSERT_MSG inside of functions under VC++8.

#include "assert_vc8.hpp"

static bool a = func<int>();

int main()
{
}
