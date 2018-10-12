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
#include <boost/type_erasure/any_cast.hpp>
#include <boost/mpl/vector.hpp>

using namespace boost::type_erasure;

int main()
{
    any< ::boost::mpl::vector<copy_constructible<>, typeid_<> > > y(2);
    int i = any_cast<int&>(&y);
}
