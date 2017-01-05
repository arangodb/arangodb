#ifndef BOOST_METAPARSE_GETTING_STARTED_5_HPP
#define BOOST_METAPARSE_GETTING_STARTED_5_HPP

// Automatically generated header file

// Definitions before section 4.2
#include "4_2.hpp"

// Definitions of section 4.2
#include <boost/metaparse/transform.hpp>

#include <boost/mpl/plus.hpp>

#include <boost/mpl/at.hpp>

template <class Vector> 
 struct eval_plus : 
   boost::mpl::plus< 
     typename boost::mpl::at_c<Vector, 0>::type, 
     typename boost::mpl::at_c<Vector, 2>::type 
   > {};

// query:
//   eval_plus< 
//     boost::mpl::vector< 
//       mpl_::integral_c<int, 11>, 
//       mpl_::char_<'+'>, 
//       mpl_::integral_c<int, 2> 
//     >>::type

#include <boost/mpl/quote.hpp>

using exp_parser6 = 
 build_parser< 
   transform< 
     sequence<int_token, plus_token, int_token>, 
     boost::mpl::quote1<eval_plus> 
   > 
 >;

// query:
//    exp_parser6::apply<BOOST_METAPARSE_STRING("11 + 2")>::type

#endif

