// Boost.TypeErasure library
//
// Copyright 2012 Steven Watanabe
//
// Distributed under the Boost Software License Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// $Id$

#include <boost/type_erasure/any.hpp>
#include <boost/type_erasure/builtin.hpp>
#include <boost/mpl/map.hpp>

using namespace boost::type_erasure;
namespace mpl = boost::mpl;

int main()
{
    any<copy_constructible<> > x(1, make_binding<mpl::map<mpl::pair<_self, char> > >());
}
