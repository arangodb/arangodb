//
// Test that a const rvalue can't be passed to ref()
//
// Copyright 2014 Agustin Berge
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/ref.hpp>

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)

struct X {};

X const crv() { return X(); }

int main()
{
    boost::reference_wrapper<X const> r = boost::ref( crv() ); // this should produce an ERROR
}

#else
#  error To fail, this test requires rvalue references
#endif
