#ifndef BOOST_METAPARSE_GETTING_STARTED_5_2_2_HPP
#define BOOST_METAPARSE_GETTING_STARTED_5_2_2_HPP

// Automatically generated header file

// Definitions before section 5.2.1
#include "5_2_1.hpp"

// Definitions of section 5.2.1
#include <boost/mpl/fold.hpp>

using vector_of_numbers = 
 boost::mpl::vector< 
   boost::mpl::int_<2>, 
   boost::mpl::int_<5>, 
   boost::mpl::int_<6> 
 >;

template <class Vector> 
 struct sum_vector : 
    boost::mpl::fold< 
      Vector, 
      boost::mpl::int_<0>, 
      boost::mpl::lambda< 
        boost::mpl::plus<boost::mpl::_1, boost::mpl::_2> 
      >::type 
    > 
  {};

// query:
//    sum_vector<vector_of_numbers>::type

template <class Sum, class Item> 
   struct sum_items : 
     boost::mpl::plus< 
       Sum, 
       typename boost::mpl::at_c<Item, 1>::type 
     > 
 {};

// query:
//   sum_items< 
//      mpl_::integral_c<int, 1>, 
//      boost::mpl::vector<mpl_::char_<'+'>, mpl_::integral_c<int, 2>> 
//    >::type

// query:
//    boost::mpl::at_c<temp_result, 1>::type

// query:
//   boost::mpl::fold< 
//      boost::mpl::at_c<temp_result, 1>::type, /* The vector to summarise */ 
//      boost::mpl::int_<0>, /* The value to start the sum from */ 
//      boost::mpl::quote2<sum_items> /* The function to call in each iteration */ 
//    >::type

#endif

