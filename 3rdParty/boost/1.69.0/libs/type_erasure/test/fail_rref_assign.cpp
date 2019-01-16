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
#include <boost/mpl/vector.hpp>

using namespace boost::type_erasure;

int main()
{
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    int i;
    any<typeid_<>, _self&&> x(std::move(i));
    x = x;
#else
#   error "No support for rvalue-reference is enabled. ==> Test fails (as expected) by default!"
#endif
}
