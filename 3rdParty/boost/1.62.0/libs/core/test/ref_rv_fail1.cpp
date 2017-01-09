//
// Test that a reference_wrapper can't be constructed from an rvalue
//
// Copyright 2014 Agustin Berge
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/ref.hpp>

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)

int main()
{
    boost::reference_wrapper<int const> r( 1 ); // this should produce an ERROR
}

#else
#  error To fail, this test requires rvalue references
#endif
