
// Copyright Aleksey Gurtovoy 2000-2004
//
// Distributed under the Boost Software License, Version 1.0. 
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/mpl for documentation.

// $Id$
// $Date$
// $Revision$

#include <boost/mpl/set_c.hpp>
#include <boost/mpl/at.hpp>
#include <boost/mpl/size.hpp>
#include <boost/mpl/begin_end.hpp>

#include <boost/mpl/aux_/test.hpp>

namespace test { namespace {
#if !BOOST_WORKAROUND(BOOST_MSVC, <= 1300)
template< typename S, typename S::value_type k > 
struct at_c
    : at< S, integral_c<typename S::value_type,k> >::type
{
};
#else
template< typename S, long k > 
struct at_c
    : aux::msvc_eti_base<
          at< S, integral_c<typename S::value_type,k> >
        >
{
};
#endif
}}

#if !BOOST_WORKAROUND(BOOST_MSVC, <= 1200)
MPL_TEST_CASE()
{
    typedef set_c<bool,true>::type s1;
    typedef set_c<bool,false>::type s2;
    typedef set_c<bool,true,false>::type s3;

    MPL_ASSERT_RELATION( size<s1>::value, ==, 1 );
    MPL_ASSERT_RELATION( size<s2>::value, ==, 1 );
    MPL_ASSERT_RELATION( size<s3>::value, ==, 2 );

    MPL_ASSERT(( is_same< s1::value_type, bool > ));
    MPL_ASSERT(( is_same< s3::value_type, bool > ));
    MPL_ASSERT(( is_same< s2::value_type, bool > ));

#if !BOOST_WORKAROUND(BOOST_MSVC, <= 1300)
    MPL_ASSERT_RELATION( ( test::at_c<s1,true>::value ), ==, true );
    MPL_ASSERT_RELATION( ( test::at_c<s2,false>::value ), ==, false );
    MPL_ASSERT_RELATION( ( test::at_c<s3,true>::value ), ==, true );
    MPL_ASSERT_RELATION( ( test::at_c<s3,false>::value ), ==, false );

    MPL_ASSERT(( is_same< test::at_c<s1,false>::type, void_ > ));
    MPL_ASSERT(( is_same< test::at_c<s2,true>::type, void_ > ));
#endif

    typedef begin<s1>::type first1;
    typedef end<s1>::type last1;
    MPL_ASSERT_RELATION( (distance<first1, last1>::value), ==, 1 );

    typedef begin<s2>::type first2;
    typedef end<s2>::type last2;
    MPL_ASSERT_RELATION( (distance<first2, last2>::value), ==, 1 );
    
    typedef begin<s3>::type first3;
    typedef end<s3>::type last3;
    MPL_ASSERT_RELATION( (distance<first3, last3>::value), ==, 2 );
}
#endif

MPL_TEST_CASE()
{
    typedef set_c<char,'a'>::type s1;
    typedef set_c<char,'a','b','c','d','e','f','g','h'>::type s2;

    MPL_ASSERT_RELATION( size<s1>::value, ==, 1 );
    MPL_ASSERT_RELATION( size<s2>::value, ==, 8 );

    MPL_ASSERT(( is_same< s1::value_type, char > ));
    MPL_ASSERT(( is_same< s2::value_type, char > ));

#if !BOOST_WORKAROUND(BOOST_MSVC, <= 1300)
    MPL_ASSERT_RELATION( ( test::at_c<s1,'a'>::value ), ==, 'a' );
    MPL_ASSERT_RELATION( ( test::at_c<s2,'a'>::value ), ==, 'a' );
    MPL_ASSERT_RELATION( ( test::at_c<s2,'d'>::value ), ==, 'd'  );
    MPL_ASSERT_RELATION( ( test::at_c<s2,'h'>::value ), ==, 'h' );

    MPL_ASSERT(( is_same< test::at_c<s1,'z'>::type, void_ > ));
    MPL_ASSERT(( is_same< test::at_c<s2,'k'>::type, void_ > ));
#endif
    
    typedef begin<s1>::type first1;
    typedef end<s1>::type last1;
    MPL_ASSERT_RELATION( (distance<first1, last1>::value), ==, 1 );

    typedef begin<s2>::type first2;
    typedef end<s2>::type last2;
    MPL_ASSERT_RELATION( (distance<first2, last2>::value), ==, 8 );
}
