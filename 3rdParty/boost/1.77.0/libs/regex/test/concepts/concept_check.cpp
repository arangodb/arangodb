/*
 *
 * Copyright (c) 2003
 * John Maddock
 *
 * Use, modification and distribution are subject to the 
 * Boost Software License, Version 1.0. (See accompanying file 
 * LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include <boost/regex.hpp>
#include <boost/config.hpp>

#if defined(BOOST_MSVC)
// this lets us compile at warning level 4 without seeing concept-check related warnings
#  pragma warning(disable:4100)
#endif
#ifdef BOOST_BORLANDC
#pragma option -w-8019 -w-8004 -w-8008
#endif
#ifdef BOOST_INTEL
#pragma warning(disable:1418 981 983 595 383)
#endif

#include <boost/regex/concepts.hpp>


int main()
{
   boost::function_requires<
      boost::RegexTraitsConcept<
         boost::regex_traits<char>
      >
   >();
#ifndef BOOST_NO_STD_LOCALE
   boost::function_requires<
      boost::BoostRegexConcept<
         boost::basic_regex<char, boost::cpp_regex_traits<char> >
      >
   >();
#ifndef BOOST_NO_WREGEX
   boost::function_requires<
      boost::BoostRegexConcept<
         boost::basic_regex<wchar_t, boost::cpp_regex_traits<wchar_t> >
      >
   >();
#endif
#endif
   boost::function_requires<
      boost::BoostRegexConcept<
         boost::basic_regex<char, boost::c_regex_traits<char> >
      >
   >();
#ifndef BOOST_NO_WREGEX
   boost::function_requires<
      boost::BoostRegexConcept<
         boost::basic_regex<wchar_t, boost::c_regex_traits<wchar_t> >
      >
   >();
#endif
#if defined(_WIN32) && !defined(BOOST_REGEX_NO_W32)
   boost::function_requires<
      boost::BoostRegexConcept<
         boost::basic_regex<char, boost::w32_regex_traits<char> >
      >
   >();
#ifndef BOOST_NO_WREGEX
   boost::function_requires<
      boost::BoostRegexConcept<
         boost::basic_regex<wchar_t, boost::w32_regex_traits<wchar_t> >
      >
   >();
#endif
#endif
   //
   // now test the regex_traits concepts:
   //
   typedef boost::basic_regex<char, boost::regex_traits_architype<char> > regex_traits_tester_type1;
   boost::function_requires<
      boost::BoostRegexConcept<
         regex_traits_tester_type1
      >
   >();
   typedef boost::basic_regex<boost::char_architype, boost::regex_traits_architype<boost::char_architype> > regex_traits_tester_type2;
   boost::function_requires<
      boost::BaseRegexConcept<
         regex_traits_tester_type2
      >
   >();
   return 0;
}


