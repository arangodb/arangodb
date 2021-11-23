//  (C) Copyright 2009-2011 Frederic Bron.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/has_unary_plus.hpp>
#endif
#include "test.hpp"
#include "check_integral_constant.hpp"

#define BOOST_TT_TRAIT_NAME has_unary_plus
#define BOOST_TT_TRAIT_OP +


#include "has_prefix_operators.hpp"

void specific() {
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void, void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void, bool >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void, bool const >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void, bool & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void, bool const & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void, int >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void, int const >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void, int & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void, int const & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool, void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool, bool >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool, bool const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool, bool & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool, bool const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool, int >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool, int const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool, int & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool, int const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool const, void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool const, bool >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool const, bool const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool const, bool & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool const, bool const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool const, int >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool const, int const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool const, int & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool const, int const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool &, void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool &, bool >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool &, bool const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool &, bool & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool &, bool const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool &, int >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool &, int const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool &, int & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool &, int const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool const &, void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool const &, bool >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool const &, bool const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool const &, bool & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool const &, bool const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool const &, int >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool const &, int const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool const &, int & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< bool const &, int const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int, void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int, bool >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int, bool & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int, bool const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int, int >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int, int const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int, int & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int, int const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int const, void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int const, bool >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int const, bool const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int const, bool & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int const, bool const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int const, int >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int const, int const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int const, int & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int const, int const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int &, void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int &, bool >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int &, bool & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int &, bool const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int &, int >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int &, int const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int &, int & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int &, int const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int const &, void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int const &, bool const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int const &, bool & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int const &, bool const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int const &, int >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int const &, int const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int const &, int & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int const &, int const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double, void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double, bool >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double, bool const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double, bool & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double, bool const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double, int >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double, int const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double, int & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double, int const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double const, void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double const, bool >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double const, bool const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double const, bool & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double const, bool const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double const, int >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double const, int const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double const, int & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double const, int const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double &, void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double &, bool >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double &, bool const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double &, bool & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double &, bool const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double &, int >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double &, int const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double &, int & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double &, int const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double const &, void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double const &, bool >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double const &, bool const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double const &, bool & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double const &, int >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double const &, int const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double const &, int & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< double const &, int const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void*, void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void*, bool >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void*, bool const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void*, bool const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void*, int >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void*, int const >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void*, int & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void*, int const & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* const, void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* const, bool >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* const, bool const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* const, bool & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* const, int >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* const, int const >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* const, int & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* const, int const & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* &, void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* &, bool >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* &, bool const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* &, bool & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* &, bool const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* &, int >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* &, int const >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* &, int & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* &, int const & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* const &, void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* const &, bool >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* const &, bool const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* const &, bool & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* const &, bool const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* const &, int >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* const &, int const >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< void* const &, int const & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int*, void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int*, bool >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int*, bool const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int*, bool & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int*, bool const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int*, int >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int*, int const >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int*, int & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int*, int const & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* const, void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* const, bool const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* const, bool & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* const, bool const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* const, int >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* const, int & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* const, int const & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* &, bool >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* &, bool const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* &, bool & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* &, bool const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* &, int >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* &, int const >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* &, int & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* &, int const & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* const &, void >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* const &, bool >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* const &, bool const >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* const &, bool & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* const &, bool const & >::value), 1);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* const &, int >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* const &, int const >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* const &, int & >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< int* const &, int const & >::value), 0);

#if defined(BOOST_TT_HAS_ACCURATE_BINARY_OPERATOR_DETECTION)
   // There are some things that pass that wouldn't otherwise do so:
   auto f = []() {};
   auto f2 = [](double)->int { return 2; };
#if BOOST_WORKAROUND(BOOST_MSVC, <= 1900)
   bool result = false;
#else
   bool result = true;
#endif
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME<decltype(f)>::value), result);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME<decltype(f2)>::value), result);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME<decltype(f), bool>::value), result);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME<decltype(f2), bool>::value), result);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME<decltype(f), void>::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME<decltype(f2), void>::value), 0);

#ifndef BOOST_NO_CXX11_SCOPED_ENUMS
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< scoped_enum >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< scoped_enum, scoped_enum >::value), 0);
   BOOST_CHECK_INTEGRAL_CONSTANT((::boost::BOOST_TT_TRAIT_NAME< scoped_enum, void >::value), 0);
#endif
#endif
}

TT_TEST_BEGIN(BOOST_TT_TRAIT_NAME)
   common();
   specific();
TT_TEST_END
