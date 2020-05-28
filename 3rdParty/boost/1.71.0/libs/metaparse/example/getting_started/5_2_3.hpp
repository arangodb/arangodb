#ifndef BOOST_METAPARSE_GETTING_STARTED_5_2_3_HPP
#define BOOST_METAPARSE_GETTING_STARTED_5_2_3_HPP

// Automatically generated header file

// Definitions before section 5.2.2
#include "5_2_2.hpp"

// Definitions of section 5.2.2
using exp_parser8 = 
 build_parser< 
   sequence< 
     int_token, /* parse the first <number> */ 
     transform< 
       repeated<sequence<plus_token, int_token>>, /* parse the "+ <number>" elements */ 
       /* lambda expression summarising the "+ <number>" elements using fold */ 
       boost::mpl::lambda< 
         /* The folding expression we have just created */ 
         boost::mpl::fold< 
           boost::mpl::_1, /* the argument of the lambda expression, the result */ 
                           /* of the repeated<...> parser */ 
           boost::mpl::int_<0>, 
           boost::mpl::quote2<sum_items> 
         > 
       >::type 
     > 
   > 
 >;

// query:
//    exp_parser8::apply<BOOST_METAPARSE_STRING("1 + 2 + 3 + 4")>::type

using exp_parser9 = 
 build_parser< 
   transform< 
     /* What we had so far */ 
     sequence< 
       int_token, 
       transform< 
         repeated<sequence<plus_token, int_token>>, 
         boost::mpl::lambda< 
           boost::mpl::fold< 
             boost::mpl::_1, 
             boost::mpl::int_<0>, 
             boost::mpl::quote2<sum_items> 
           > 
         >::type 
       > 
     >, 
     boost::mpl::quote1<sum_vector> /* summarise the vector of numbers */ 
   > 
 >;

// query:
//    exp_parser9::apply<BOOST_METAPARSE_STRING("1 + 2 + 3 + 4")>::type

#endif

