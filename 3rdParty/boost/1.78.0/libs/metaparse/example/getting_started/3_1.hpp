#ifndef BOOST_METAPARSE_GETTING_STARTED_3_1_HPP
#define BOOST_METAPARSE_GETTING_STARTED_3_1_HPP

// Automatically generated header file

// Definitions before section 3
#include "3.hpp"

// Definitions of section 3
#include <boost/metaparse/int_.hpp>

#include <boost/metaparse/build_parser.hpp>

using namespace boost::metaparse;

using exp_parser1 = build_parser<int_>;

// query:
//    exp_parser1::apply<BOOST_METAPARSE_STRING("13")>::type

#endif

