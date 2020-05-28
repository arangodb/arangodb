#ifndef BOOST_METAPARSE_GETTING_STARTED_11_HPP
#define BOOST_METAPARSE_GETTING_STARTED_11_HPP

// Automatically generated header file

// Definitions before section 10
#include "10.hpp"

// Definitions of section 10
using lparen_token = token<lit_c<'('>>;

using rparen_token = token<lit_c<')'>>;

using plus_exp1 = 
 foldl_start_with_parser< 
   sequence<one_of<plus_token, minus_token>, mult_exp4>, 
   mult_exp4, 
   boost::mpl::quote2<binary_op> 
 >;

using paren_exp1 = sequence<lparen_token, plus_exp1, rparen_token>;

#include <boost/metaparse/middle_of.hpp>

using paren_exp2 = middle_of<lparen_token, plus_exp1, rparen_token>;

using primary_exp1 = one_of<int_token, paren_exp2>;

struct plus_exp2;

using paren_exp3 = middle_of<lparen_token, plus_exp2, rparen_token>;

using primary_exp2 = one_of<int_token, paren_exp2>;

using unary_exp2 = 
 foldr_start_with_parser< 
   minus_token, 
   primary_exp2, 
   boost::mpl::lambda<boost::mpl::negate<boost::mpl::_1>>::type 
 >;

using mult_exp5 = 
 foldl_start_with_parser< 
   sequence<one_of<times_token, divides_token>, unary_exp2>, 
   unary_exp2, 
   boost::mpl::quote2<binary_op> 
 >;

struct plus_exp2 : 
 foldl_start_with_parser< 
   sequence<one_of<plus_token, minus_token>, mult_exp5>, 
   mult_exp5, 
   boost::mpl::quote2<binary_op> 
 > {};

using exp_parser19 = build_parser<plus_exp2>;

// query:
//    exp_parser19::apply<BOOST_METAPARSE_STRING("(1 + 2) * 3")>::type

#endif

