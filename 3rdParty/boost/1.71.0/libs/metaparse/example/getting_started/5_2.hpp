#ifndef BOOST_METAPARSE_GETTING_STARTED_5_2_HPP
#define BOOST_METAPARSE_GETTING_STARTED_5_2_HPP

// Automatically generated header file

// Definitions before section 5.1
#include "5_1.hpp"

// Definitions of section 5.1
#include <boost/metaparse/any.hpp>

using exp_parser7 = 
 build_parser< 
   sequence< 
     int_token,                                /* The first <number> */ 
     repeated<sequence<plus_token, int_token>> /* The "+ <number>" elements */ 
   > 
 >;

// query:
//    exp_parser7::apply<BOOST_METAPARSE_STRING("1 + 2 + 3 + 4")>::type

#endif

