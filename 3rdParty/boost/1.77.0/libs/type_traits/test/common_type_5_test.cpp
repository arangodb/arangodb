
//  Copyright Peter Dimov 2015
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.tt.org/LICENSE_1_0.txt)

#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/common_type.hpp>
#endif
#include "test.hpp"
#include "check_type.hpp"
#include <iostream>

template<class T> struct X
{
    T t_;

    X(): t_() {}
    template<class U> X( X<U> const & x ): t_( x.t_ ) {}
};

namespace boost
{

    template<class T, class U> struct common_type< X<T>, X<U> >
    {
        typedef X<typename common_type<T, U>::type> type;
    };

} // namespace boost

TT_TEST_BEGIN(common_type_5)
{
    // user specializations, binary

    BOOST_CHECK_TYPE3( tt::common_type< X<char>, X<char> >::type, X<char> );

    BOOST_CHECK_TYPE3( tt::common_type< X<char>&, X<char>& >::type, X<char> );
    BOOST_CHECK_TYPE3( tt::common_type< X<char>&, X<char> const& >::type, X<char> );
    BOOST_CHECK_TYPE3( tt::common_type< X<char> const&, X<char>& >::type, X<char> );
    BOOST_CHECK_TYPE3( tt::common_type< X<char> const&, X<char> const& >::type, X<char> );

    BOOST_CHECK_TYPE3( tt::common_type< X<char>, X<unsigned char> >::type, X<int> );

    BOOST_CHECK_TYPE3( tt::common_type< X<char>&, X<unsigned char>& >::type, X<int> );
    BOOST_CHECK_TYPE3( tt::common_type< X<char>&, X<unsigned char> const& >::type, X<int> );
    BOOST_CHECK_TYPE3( tt::common_type< X<char> const&, X<unsigned char>& >::type, X<int> );
    BOOST_CHECK_TYPE3( tt::common_type< X<char> const&, X<unsigned char> const& >::type, X<int> );

    // ternary

    BOOST_CHECK_TYPE4( tt::common_type< X<char>&, X<long> const&, X<short> volatile& >::type, X<long> );
}
TT_TEST_END
