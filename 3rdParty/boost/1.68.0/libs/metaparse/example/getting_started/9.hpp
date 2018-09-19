#ifndef BOOST_METAPARSE_GETTING_STARTED_9_HPP
#define BOOST_METAPARSE_GETTING_STARTED_9_HPP

// Automatically generated header file

// Definitions before section 8.2
#include "8_2.hpp"

// Definitions of section 8.2
template <class S, class Item> 
 struct reverse_binary_op : 
   eval_binary_op< 
     typename boost::mpl::at_c<Item, 0>::type, 
     boost::mpl::at_c<Item, 1>::type::value, 
     S 
   > 
   {};

#include <boost/metaparse/foldr_start_with_parser.hpp>

using mult_exp3 = 
 foldr_start_with_parser< 
   sequence<int_token, one_of<times_token, divides_token>>, /* The parser applied repeatedly */ 
   int_token, /* The parser parsing the last number */ 
   boost::mpl::quote2<reverse_binary_op> /* The function called for every result */ 
                                         /* of applying the above parser */ 
 >;

using exp_parser17 = 
 build_parser< 
   foldl_start_with_parser< 
     sequence<one_of<plus_token, minus_token>, mult_exp3>, 
     mult_exp3, 
     boost::mpl::quote2<binary_op> 
   > 
 >;

// query:
//    exp_parser17::apply<BOOST_METAPARSE_STRING("8 / 4 / 2")>::type

#endif

