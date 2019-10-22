#ifndef BOOST_METAPARSE_GETTING_STARTED_7_2_HPP
#define BOOST_METAPARSE_GETTING_STARTED_7_2_HPP

// Automatically generated header file

// Definitions before section 7.1
#include "7_1.hpp"

// Definitions of section 7.1
#include <boost/mpl/times.hpp>

template <class L, class R> struct eval_binary_op<L, '*', R> : boost::mpl::times<L, R>::type {};

// query:
//    eval_binary_op<boost::mpl::int_<3>, '*', boost::mpl::int_<4>>::type

using times_token = token<lit_c<'*'>>;

using exp_parser14 = 
 build_parser< 
   foldl_start_with_parser< 
     sequence<one_of<plus_token, minus_token, times_token>, int_token>, 
     int_token, 
     boost::mpl::quote2<binary_op> 
   > 
 >;

// query:
//    exp_parser14::apply<BOOST_METAPARSE_STRING("2 * 3")>::type

// query:
//    exp_parser14::apply<BOOST_METAPARSE_STRING("1 + 2 * 3")>::type

#endif

