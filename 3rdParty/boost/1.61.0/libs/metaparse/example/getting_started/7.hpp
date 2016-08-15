#ifndef BOOST_METAPARSE_GETTING_STARTED_7_HPP
#define BOOST_METAPARSE_GETTING_STARTED_7_HPP

// Automatically generated header file

// Definitions before section 6.2
#include "6_2.hpp"

// Definitions of section 6.2
#include <boost/mpl/minus.hpp>

template <class L, char Op, class R> struct eval_binary_op;

template <class L, class R> struct eval_binary_op<L, '+', R> : boost::mpl::plus<L, R>::type {};

template <class L, class R> struct eval_binary_op<L, '-', R> : boost::mpl::minus<L, R>::type {};

// query:
//    eval_binary_op<boost::mpl::int_<11>, '+', boost::mpl::int_<2>>::type

// query:
//    eval_binary_op<boost::mpl::int_<13>, '-', boost::mpl::int_<2>>::type

template <class S, class Item> 
 struct binary_op : 
   eval_binary_op< 
     S, 
     boost::mpl::at_c<Item, 0>::type::value, 
     typename boost::mpl::at_c<Item, 1>::type 
   > 
   {};

// query:
//    binary_op<boost::mpl::int_<11>, boost::mpl::vector<boost::mpl::char_<'+'>, boost::mpl::int_<2>>>::type

using exp_parser13 = 
 build_parser< 
   foldl_start_with_parser< 
     sequence<one_of<plus_token, minus_token>, int_token>, 
     int_token, 
     boost::mpl::quote2<binary_op> 
   > 
 >;

// query:
//    exp_parser13::apply<BOOST_METAPARSE_STRING("1 + 2 - 3")>::type

#endif

