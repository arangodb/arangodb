/*
*
* Copyright (c) 2016
* John Maddock
*
* Use, modification and distribution are subject to the
* Boost Software License, Version 1.0. (See accompanying file
* LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*
*/

#include <boost/config.hpp>

#ifndef BOOST_NO_CXX11_CHAR32_T

#include <cstddef>

namespace boost {

   std::size_t hash_value(char32_t const& c) { return c; }

}

#include <boost/regex.hpp>

struct char32_traits
{
   typedef char32_t char_type;
   typedef std::size_t size_type;
   typedef std::vector<char32_t> string_type;
   typedef int locale_type; // not used
   typedef unsigned char_class_type;

   static size_type length(const char32_t* p)
   {
      size_type result = 0;
      while(*p)
      {
         ++p;
         ++result;
      }
      return result;
   }
   static char_type translate(char_type c) { return c; }
   static char_type translate_nocase(char_type c) { return c; }
   static string_type transform(const char32_t* p1, const char32_t* p2)
   {
      return string_type(p1, p2);
   }
   static string_type transform_primary(const char32_t* p1, const char32_t* p2)
   {
      return string_type(p1, p2);
   }
   static char_class_type lookup_classname(const char32_t* p1, const char32_t* p2)
   {
      std::string s(p1, p2);
      return boost::c_regex_traits<char>::lookup_classname(s.c_str(), s.c_str() + s.length());
      return 0;
   }
   static string_type lookup_collatename(const char32_t* p1, const char32_t* p2)
   {
      return string_type(p1, p2);
   }
   static bool isctype(char_type c, char_class_type t)
   {
      if(c < 0xff)
         return boost::c_regex_traits<char>::isctype(c, t);
      return false;
   }
   static boost::intmax_t value(char_type c, int radix)
   {
      switch(radix)
      {
      case 8:
         if((c >= '0') && (c <= '7'))
            return c - '0';
         break;
      case 10:
         if((c >= '0') && (c <= '9'))
            return c - '0';
         break;
      case 16:
         if((c >= '0') && (c <= '9'))
            return c - '0';
         if((c >= 'a') && (c <= 'f'))
            return (c - 'a') + 10;
         if((c >= 'A') && (c <= 'F'))
            return (c - 'A') + 10;
         break;
      }
      return -1;
   }
   static locale_type imbue(locale_type) { return 0; }
   static locale_type getloc() { return 0; }
};


int main()
{
   char32_t big_char[] = { 0xF, 0xFF, 0xFFF, 0xFFFF, 0xFFFFF, 0xFFFFFF, 0xFFFFFFF, 0xFFFFFFFF, 0 };

   boost::basic_regex<char32_t, char32_traits> e(U"\\x{F}\\x{FF}\\x{FFF}\\x{FFFF}\\x{FFFFF}\\x{FFFFFF}\\x{FFFFFFF}\\x{FFFFFFFF}");

   if(!regex_match(big_char, e))
   {
      return 1;
   }
   return 0;
}

#else

int main() { return 0; }

#endif
