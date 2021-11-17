#ifndef BOOST_METAPARSE_GETTING_STARTED_3_3_HPP
#define BOOST_METAPARSE_GETTING_STARTED_3_3_HPP

// Automatically generated header file

// Definitions before section 3.2
#include "3_2.hpp"

// Definitions of section 3.2
// query:
//    exp_parser1::apply<BOOST_METAPARSE_STRING("11 13")>::type

#include <boost/metaparse/entire_input.hpp>

using exp_parser2 = build_parser<entire_input<int_>>;

// query:
//    exp_parser2::apply<BOOST_METAPARSE_STRING("13")>::type

// query:
//    exp_parser2::apply<BOOST_METAPARSE_STRING("11 13")>::type

#endif

