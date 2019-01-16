#ifndef BOOST_METAPARSE_GETTING_STARTED_12_HPP
#define BOOST_METAPARSE_GETTING_STARTED_12_HPP

// Automatically generated header file

// Definitions before section 11.3.2
#include "11_3_2.hpp"

// Definitions of section 11.3.2
struct plus_exp6;

using paren_exp5 = middle_of<lparen_token, plus_exp6, rparen_token>;

using primary_exp4 = one_of<int_token, paren_exp5, fail<missing_primary_expression>>;

using unary_exp4 = 
 foldr_start_with_parser< 
   minus_token, 
   primary_exp4, 
   boost::mpl::lambda<boost::mpl::negate<boost::mpl::_1>>::type 
 >;

using mult_exp7 = 
 foldl_reject_incomplete_start_with_parser< 
   sequence<one_of<times_token, divides_token>, unary_exp4>, 
   unary_exp4, 
   boost::mpl::quote2<binary_op> 
 >;

struct plus_exp6 : 
 foldl_reject_incomplete_start_with_parser< 
   sequence<one_of<plus_token, minus_token>, mult_exp7>, 
   mult_exp7, 
   boost::mpl::quote2<binary_op> 
 > {};

using exp_parser23 = build_parser<plus_exp6>;

// query:
//    exp_parser23::apply<BOOST_METAPARSE_STRING("1+(2*")>::type

// query:
//    exp_parser23::apply<BOOST_METAPARSE_STRING("1+(2*3")>::type

#endif

