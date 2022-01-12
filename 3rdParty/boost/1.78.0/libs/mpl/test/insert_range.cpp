
// Copyright Aleksey Gurtovoy 2001-2004
//
// Distributed under the Boost Software License, Version 1.0. 
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/mpl for documentation.

// $Id$
// $Date$
// $Revision$

#include <boost/mpl/insert_range.hpp>
#include <boost/mpl/find.hpp>
#include <boost/mpl/vector_c.hpp>
#include <boost/mpl/list.hpp>
#include <boost/mpl/set.hpp>
#include <boost/mpl/set_c.hpp>
#include <boost/mpl/map.hpp>
#include <boost/mpl/size.hpp>
#include <boost/mpl/range_c.hpp>
#include <boost/mpl/equal.hpp>
#include <boost/mpl/fold.hpp>
#include <boost/mpl/placeholders.hpp>
#include <boost/mpl/logical.hpp>
#include <boost/mpl/contains.hpp>
#include <boost/mpl/joint_view.hpp>

#include <boost/mpl/aux_/test.hpp>

MPL_TEST_CASE()
{
    typedef vector_c<int,0,1,7,8,9> numbers;
    typedef find< numbers,integral_c<int,7> >::type pos;
    typedef insert_range< numbers,pos,range_c<int,2,7> >::type range;

    MPL_ASSERT_RELATION( size<range>::value, ==, 10 );
    MPL_ASSERT(( equal< range,range_c<int,0,10> > ));

    typedef insert_range< list0<>,end< list0<> >::type,list1<int> >::type result2;
    MPL_ASSERT_RELATION( size<result2>::value, ==, 1 );
}

template<typename A, typename B>
void test_associative()
{
    typedef typename insert_range< A,typename end< A >::type,B >::type C;

    MPL_ASSERT_RELATION( size<C>::value, <=, (size<A>::value + size<B>::value) );
    MPL_ASSERT(( fold< joint_view< A,B >,true_,and_< _1,contains< C,_2 > > > ));
}

MPL_TEST_CASE()
{
    typedef set3< short,int,long > signed_integers;
    typedef set3< unsigned short,unsigned int,unsigned long > unsigned_integers;
    test_associative<signed_integers, unsigned_integers>();

    typedef set_c< int,1,3,5,7,9 > odds;
    typedef set_c< int,0,2,4,6,8 > evens;
    test_associative<odds, evens>();

    typedef map2<
              pair< void,void* >
            , pair< int,int* >
            > pointers;
    typedef map2<
              pair< void const,void const* >
            , pair< int const,int const* >
            > pointers_to_const;
    test_associative<pointers, pointers_to_const>();
}
