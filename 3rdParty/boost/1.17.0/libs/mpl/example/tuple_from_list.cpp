
// Copyright Aleksey Gurtovoy 2002-2004
//
// Distributed under the Boost Software License, Version 1.0. 
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/mpl for documentation.

// $Id$
// $Date$
// $Revision$

#include <boost/mpl/reverse_fold.hpp>
#include <boost/mpl/list.hpp>
#include <boost/tuple/tuple.hpp>

#include <iostream>

using namespace boost::mpl;

template< typename Types > struct tuple_gen
    : reverse_fold<
          Types
        , boost::tuples::null_type
        , boost::tuples::cons<_2,_1>
        >
{
};

int main()
{
    tuple_gen< list<int,char const*,bool> >::type t;
    
    boost::get<0>(t) = -1;
    boost::get<1>(t) = "text";
    boost::get<2>(t) = false;

    std::cout
        << boost::get<0>(t) << '\n'
        << boost::get<1>(t) << '\n'
        << boost::get<2>(t) << '\n'
        ;

    return 0;
}
