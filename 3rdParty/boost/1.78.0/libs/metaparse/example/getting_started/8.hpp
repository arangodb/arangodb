#ifndef BOOST_METAPARSE_GETTING_STARTED_8_HPP
#define BOOST_METAPARSE_GETTING_STARTED_8_HPP

// Automatically generated header file

// Definitions before section 7.2
#include "7_2.hpp"

// Definitions of section 7.2
using mult_exp1 = foldl_start_with_parser<sequence<times_token, int_token>, int_token, boost::mpl::quote2<binary_op>>;

using exp_parser15 = 
 build_parser< 
   foldl_start_with_parser< 
     sequence<one_of<plus_token, minus_token>, mult_exp1>, 
     mult_exp1, 
     boost::mpl::quote2<binary_op> 
   > 
 >;

// query:
//    exp_parser15::apply<BOOST_METAPARSE_STRING("1 + 2 * 3")>::type

#endif

