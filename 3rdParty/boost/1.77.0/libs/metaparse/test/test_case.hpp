#ifndef BOOST_METAPARSE_TEST_TEST_CASE_HPP
#define BOOST_METAPARSE_TEST_TEST_CASE_HPP

// Copyright Abel Sinkovics (abel@sinkovics.hu) 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/preprocessor/cat.hpp>

#ifdef BOOST_METAPARSE_TEST_CASE
#  error BOOST_METAPARSE_TEST_CASE already defined
#endif
#define BOOST_METAPARSE_TEST_CASE(name) \
  void BOOST_PP_CAT(metaparse_test_case_, name)()

#endif

