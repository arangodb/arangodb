#ifndef BOOST_METAPARSE_GETTING_STARTED_11_3_1_HPP
#define BOOST_METAPARSE_GETTING_STARTED_11_3_1_HPP

// Automatically generated header file

// Definitions before section 11.3
#include "11_3.hpp"

// Definitions of section 11.3
// query:
//    exp_parser20::apply<BOOST_METAPARSE_STRING("(1+2")>::type

// query:
//    exp_parser20::apply<BOOST_METAPARSE_STRING("0+(1+2")>::type

#include <boost/metaparse/fail_at_first_char_expected.hpp>

#include <boost/metaparse/first_of.hpp>

struct plus_exp4 : 
 first_of< 
   foldl_start_with_parser< 
     sequence<one_of<plus_token, minus_token>, mult_exp6>, 
     mult_exp6, 
     boost::mpl::quote2<binary_op> 
   >, 
   fail_at_first_char_expected< 
     sequence<one_of<plus_token, minus_token>, mult_exp6> 
   > 
 > {};

using exp_parser21 = build_parser<plus_exp4>;

// query:
//    exp_parser21::apply<BOOST_METAPARSE_STRING("0+(1+2")>::type

#endif

