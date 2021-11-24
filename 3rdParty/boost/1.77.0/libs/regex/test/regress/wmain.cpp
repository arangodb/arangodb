/*
 *
 * Copyright (c) 2004
 * John Maddock
 *
 * Use, modification and distribution are subject to the 
 * Boost Software License, Version 1.0. (See accompanying file 
 * LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

 /*
  *   LOCATION:    see http://www.boost.org for most recent version.
  *   FILE         main.cpp
  *   VERSION      see <boost/version.hpp>
  *   DESCRIPTION: entry point for test program.
  */

#include "test.hpp"
#include "test_locale.hpp"
#include <stdarg.h>
#include <iostream>
#include <iomanip>

#ifdef BOOST_HAS_ICU
#include <unicode/uloc.h>
#endif

#ifdef TEST_THREADS
#include <list>
#include <boost/thread.hpp>
#include <boost/thread/tss.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/array.hpp>

int* get_array_data();

#endif

#ifndef BOOST_NO_WREGEX
void test(const wchar_t& c, const test_regex_replace_tag& tag)
{
   do_test(c, tag);
}
void test(const wchar_t& c, const test_regex_search_tag& tag)
{
   do_test(c, tag);
}
void test(const wchar_t& c, const test_invalid_regex_tag& tag)
{
   do_test(c, tag);
}
#endif

