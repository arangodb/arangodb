#ifndef BOOST_METAPARSE_GETTING_STARTED_6_2_HPP
#define BOOST_METAPARSE_GETTING_STARTED_6_2_HPP

// Automatically generated header file

// Definitions before section 6.1
#include "6_1.hpp"

// Definitions of section 6.1
using minus_token = token<lit_c<'-'>>;

#include <boost/metaparse/one_of.hpp>

using exp_parser12 = 
 build_parser< 
   foldl_start_with_parser< 
     sequence<one_of<plus_token, minus_token>, int_token>, 
     int_token, 
     boost::mpl::quote2<sum_items> 
   > 
 >;

// query:
//    exp_parser12::apply<BOOST_METAPARSE_STRING("1 + 2 - 3")>::type

#endif

