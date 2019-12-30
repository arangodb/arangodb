//
// Test that an rvalue can't be passed to cref()
//
// Copyright 2014 Agustin Berge
// Copyright 2014 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/ref.hpp>

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)

int main()
{
    boost::reference_wrapper<int const> r = boost::cref( 2 ); // should fail
    (void)r;
}

#else
#  error To fail, this test requires rvalue references.
#endif
