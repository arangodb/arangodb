#ifndef BOOST_METAPARSE_GETTING_STARTED_11_3_HPP
#define BOOST_METAPARSE_GETTING_STARTED_11_3_HPP

// Automatically generated header file

// Definitions before section 11.2
#include "11_2.hpp"

// Definitions of section 11.2
#include <boost/metaparse/define_error.hpp>

BOOST_METAPARSE_DEFINE_ERROR(missing_primary_expression, "Missing primary expression");

struct plus_exp3;

using paren_exp4 = middle_of<lparen_token, plus_exp3, rparen_token>;

#include <boost/metaparse/fail.hpp>

using primary_exp3 = one_of<int_token, paren_exp4, fail<missing_primary_expression>>;

using unary_exp3 = 
 foldr_start_with_parser< 
   minus_token, 
   primary_exp3, 
   boost::mpl::lambda<boost::mpl::negate<boost::mpl::_1>>::type 
 >;

using mult_exp6 = 
 foldl_start_with_parser< 
   sequence<one_of<times_token, divides_token>, unary_exp3>, 
   unary_exp3, 
   boost::mpl::quote2<binary_op> 
 >;

struct plus_exp3 : 
 foldl_start_with_parser< 
   sequence<one_of<plus_token, minus_token>, mult_exp6>, 
   mult_exp6, 
   boost::mpl::quote2<binary_op> 
 > {};

using exp_parser20 = build_parser<plus_exp3>;

// query:
//    exp_parser20::apply<BOOST_METAPARSE_STRING("hello")>::type

#endif

