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
  *   FILE         captures_test.cpp
  *   VERSION      see <boost/version.hpp>
  *   DESCRIPTION: Basic tests for additional captures information.
  */

#include <boost/regex.hpp>
#include <boost/detail/lightweight_main.hpp>
#include "../test_macros.hpp"
#include <boost/array.hpp>
#include <cstring>

#ifdef BOOST_HAS_ICU
#include <boost/regex/icu.hpp>
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

template <int N>
int array_size(const char* (&p)[N])
{
   for(int i = 0; i < N; ++i)
      if(p[i] == 0)
         return i;
   return N;
}

std::wstring make_wstring(const char* p)
{
   return std::wstring(p, p + std::strlen(p));
}

#ifdef __sgi
template <class T>
void test_captures(const std::string& regx, const std::string& text, const T& expected)
#else
template <class T>
void test_captures(const std::string& regx, const std::string& text, T& expected)
#endif
{
   boost::regex e(regx);
   boost::smatch what;
   if(boost::regex_match(text, what, e, boost::match_extra))
   {
      unsigned i, j;
#ifndef __sgi
      // strange type deduction causes this test to fail on SGI:
      BOOST_CHECK(what.size() == ARRAY_SIZE(expected));
#endif
      for(i = 0; i < what.size(); ++i)
      {
         BOOST_CHECK(what.captures(i).size() == array_size(expected[i]));
         for(j = 0; j < what.captures(i).size(); ++j)
         {
            BOOST_CHECK(what.captures(i)[j] == expected[i][j]);
         }
      }
   }

   std::wstring wre(regx.begin(), regx.end());
   std::wstring wtext(text.begin(), text.end());
   boost::wregex we(wre);
   boost::wsmatch wwhat;
   if(boost::regex_match(wtext, wwhat, we, boost::match_extra))
   {
      unsigned i, j;
#ifndef __sgi
      // strange type deduction causes this test to fail on SGI:
      BOOST_CHECK(wwhat.size() == ARRAY_SIZE(expected));
#endif
      for(i = 0; i < wwhat.size(); ++i)
      {
         BOOST_CHECK(wwhat.captures(i).size() == array_size(expected[i]));
         for(j = 0; j < wwhat.captures(i).size(); ++j)
         {
            BOOST_CHECK(wwhat.captures(i)[j] == make_wstring(expected[i][j]));
         }
      }
   }

#ifdef BOOST_HAS_ICU
   boost::u32regex ure = boost::make_u32regex(regx);
   what = boost::smatch();
   if(boost::u32regex_match(text, what, ure, boost::match_extra))
   {
      unsigned i, j;
#ifndef __sgi
      // strange type deduction causes this test to fail on SGI:
      BOOST_CHECK(what.size() == ARRAY_SIZE(expected));
#endif
      for(i = 0; i < what.size(); ++i)
      {
         BOOST_CHECK(what.captures(i).size() == array_size(expected[i]));
         for(j = 0; j < what.captures(i).size(); ++j)
         {
            BOOST_CHECK(what.captures(i)[j] == expected[i][j]);
         }
      }
   }
#endif
}

int cpp_main(int , char* [])
{
   typedef const char* pchar;
   pchar e1[4][5] = 
   {
      { "aBBcccDDDDDeeeeeeee", },
      { "a", "BB", "ccc", "DDDDD", "eeeeeeee", },
      { "a", "ccc", "eeeeeeee", },
      { "BB", "DDDDD", },
   };
   test_captures("(([[:lower:]]+)|([[:upper:]]+))+", "aBBcccDDDDDeeeeeeee", e1);
   pchar e2[4][2] = 
   {
      { "abd" },
      { "b", "" },
      { "" },
   };
   test_captures("a(b+|((c)*))+d", "abd", e2);
   pchar e3[3][1] = 
   {
      { "abcbar" },
      { "abc" },
   };
   test_captures("(.*)bar|(.*)bah", "abcbar", e3);
   pchar e4[3][1] = 
   {
      { "abcbah" },
      { 0, },
      { "abc" },
   };
   test_captures("(.*)bar|(.*)bah", "abcbah", e4);
   pchar e5[2][16] = 
   {
      { "now is the time for all good men to come to the aid of the party" },
      { "now", "is", "the", "time", "for", "all", "good", "men", "to", "come", "to", "the", "aid", "of", "the", "party" },
   };
   test_captures("^(?:(\\w+)|(?>\\W+))*$", "now is the time for all good men to come to the aid of the party", e5);
   pchar e6[2][16] = 
   {
      { "now is the time for all good men to come to the aid of the party" },
      { "now", "is", "the", "time", "for", "all", "good", "men", "to", "come", "to", "the", "aid", "of", "the", "party" },
   };
   test_captures("^(?>(\\w+)\\W*)*$", "now is the time for all good men to come to the aid of the party", e6);
   pchar e7[4][14] = 
   {
      { "now is the time for all good men to come to the aid of the party" },
      { "now" },
      { "is", "the", "time", "for", "all", "good", "men", "to", "come", "to", "the", "aid", "of", "the" },
      { "party" },
   };
   test_captures("^(\\w+)\\W+(?>(\\w+)\\W+)*(\\w+)$", "now is the time for all good men to come to the aid of the party", e7);
   pchar e8[5][9] = 
   {
      { "now is the time for all good men to come to the aid of the party" } ,
      { "now" },
      { "is", "for", "men", "to", "of" },
      { "the", "time", "all", "good", "to", "come", "the", "aid", "the" },
      { "party" },
   };
   test_captures("^(\\w+)\\W+(?>(\\w+)\\W+(?:(\\w+)\\W+){0,2})*(\\w+)$", "now is the time for all good men to come to the aid of the party", e8);
   return 0;
}

