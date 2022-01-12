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

// 
// This define keep ICU in it's own namespace: helps us to track bugs that would 
// otherwise go unnoticed:
//
#define U_USING_ICU_NAMESPACE 0

#include <boost/regex/config.hpp>

#if defined(BOOST_MSVC)
// this lets us compile at warning level 4 without seeing concept-check related warnings
#  pragma warning(disable:4100)
#endif
#ifdef BOOST_BORLANDC
#pragma option -w-8019 -w-8004 -w-8008
#endif

#ifdef BOOST_HAS_ICU

#include <boost/regex/icu.hpp>
#include <boost/detail/workaround.hpp>
#if !BOOST_WORKAROUND(_MSC_VER, < 1310) && !defined(BOOST_NO_MEMBER_TEMPLATES) && !defined(__IBMCPP__) && !BOOST_WORKAROUND(__GNUC__, < 3)
#include <boost/regex/concepts.hpp>
#endif

template <class I>
void check_token_iterator(I i)
{
   typedef typename I::value_type value_type;
   typedef typename value_type::value_type char_type;
   typedef std::basic_string<char_type> string_type;

   I j;

   std::vector<string_type> v;

   while (i != j)
   {
      v.push_back(i->str());
      ++i;
   }

}

template <class I>
void check_iterator(I i)
{
   typedef typename I::value_type value_type;

   std::vector <value_type> v(i, I());
   (void)v;

}
int main()
{
   // VC6 and VC7 can't cope with the iterator architypes, 
   // don't bother testing as it doesn't work:
#if !BOOST_WORKAROUND(_MSC_VER, < 1310) && !defined(BOOST_NO_MEMBER_TEMPLATES) && !defined(__IBMCPP__) && !BOOST_WORKAROUND(__GNUC__, < 3)
   boost::function_requires<
      boost::RegexTraitsConcept<
         boost::icu_regex_traits
      >
   >();
   boost::function_requires<
      boost::BoostRegexConcept<
         boost::u32regex
      >
   >();
   //
   // Now test additional function overloads:
   //
   bool b;
   unsigned long buf[2] = { 0, };
   const void* pb = buf;
   typedef boost::bidirectional_iterator_archetype<char> utf8_arch1;
   typedef boost::bidirectional_iterator_archetype<unsigned char> utf8_arch2;
   typedef boost::bidirectional_iterator_archetype<UChar> utf16_arch;
   typedef boost::bidirectional_iterator_archetype<wchar_t> wchar_arch;
   boost::match_results<utf8_arch1> m1;
   boost::match_results<utf8_arch2> m2;
   boost::match_results<utf16_arch> m3;
   boost::match_results<wchar_arch> m4;
   boost::match_results<const char*> cm1;
   boost::match_results<const unsigned char*> cm2;
   boost::match_results<const UChar*> cm3;
   boost::match_results<const wchar_t*> cm4;
   boost::match_results<std::string::const_iterator> sm1;
   boost::match_results<std::wstring::const_iterator> sm2;
   boost::u32regex e1;
   boost::regex_constants::match_flag_type flgs = boost::regex_constants::match_default;
   std::string s1;
   std::wstring s2;
   U_NAMESPACE_QUALIFIER UnicodeString us;
   b = boost::u32regex_match(utf8_arch1(), utf8_arch1(), m1, e1, flgs);
   b = boost::u32regex_match(utf8_arch1(), utf8_arch1(), m1, e1);
   b = boost::u32regex_match(utf8_arch2(), utf8_arch2(), m2, e1, flgs);
   b = boost::u32regex_match(utf8_arch2(), utf8_arch2(), m2, e1);
   b = boost::u32regex_match(utf16_arch(), utf16_arch(), m3, e1, flgs);
   b = boost::u32regex_match(utf16_arch(), utf16_arch(), m3, e1);
   b = boost::u32regex_match(wchar_arch(), wchar_arch(), m4, e1, flgs);
   b = boost::u32regex_match(wchar_arch(), wchar_arch(), m4, e1);
   b = boost::u32regex_match((const char*)(pb), cm1, e1, flgs);
   b = boost::u32regex_match((const char*)(pb), cm1, e1);
   b = boost::u32regex_match((const unsigned char*)(pb), cm2, e1, flgs);
   b = boost::u32regex_match((const unsigned char*)(pb), cm2, e1);
   b = boost::u32regex_match((const UChar*)(pb), cm3, e1, flgs);
   b = boost::u32regex_match((const UChar*)(pb), cm3, e1);
   b = boost::u32regex_match((const wchar_t*)(pb), cm4, e1, flgs);
   b = boost::u32regex_match((const wchar_t*)(pb), cm4, e1);
   b = boost::u32regex_match(s1, sm1, e1, flgs);
   b = boost::u32regex_match(s1, sm1, e1);
   b = boost::u32regex_match(s2, sm2, e1, flgs);
   b = boost::u32regex_match(s2, sm2, e1);
   b = boost::u32regex_match(us, cm3, e1, flgs);
   b = boost::u32regex_match(us, cm3, e1);

   b = boost::u32regex_search(utf8_arch1(), utf8_arch1(), m1, e1, flgs);
   b = boost::u32regex_search(utf8_arch1(), utf8_arch1(), m1, e1);
   b = boost::u32regex_search(utf8_arch2(), utf8_arch2(), m2, e1, flgs);
   b = boost::u32regex_search(utf8_arch2(), utf8_arch2(), m2, e1);
   b = boost::u32regex_search(utf16_arch(), utf16_arch(), m3, e1, flgs);
   b = boost::u32regex_search(utf16_arch(), utf16_arch(), m3, e1);
   b = boost::u32regex_search(wchar_arch(), wchar_arch(), m4, e1, flgs);
   b = boost::u32regex_search(wchar_arch(), wchar_arch(), m4, e1);
   b = boost::u32regex_search((const char*)(pb), cm1, e1, flgs);
   b = boost::u32regex_search((const char*)(pb), cm1, e1);
   b = boost::u32regex_search((const unsigned char*)(pb), cm2, e1, flgs);
   b = boost::u32regex_search((const unsigned char*)(pb), cm2, e1);
   b = boost::u32regex_search((const UChar*)(pb), cm3, e1, flgs);
   b = boost::u32regex_search((const UChar*)(pb), cm3, e1);
   b = boost::u32regex_search((const wchar_t*)(pb), cm4, e1, flgs);
   b = boost::u32regex_search((const wchar_t*)(pb), cm4, e1);
   b = boost::u32regex_search(s1, sm1, e1, flgs);
   b = boost::u32regex_search(s1, sm1, e1);
   b = boost::u32regex_search(s2, sm2, e1, flgs);
   b = boost::u32regex_search(s2, sm2, e1);
   b = boost::u32regex_search(us, cm3, e1, flgs);
   b = boost::u32regex_search(us, cm3, e1);
   boost::output_iterator_archetype<char> out1 = boost::detail::dummy_constructor();
   out1 = boost::u32regex_replace(out1, utf8_arch1(), utf8_arch1(), e1, (const char*)(pb), flgs);
   boost::output_iterator_archetype<unsigned char> out2 = boost::detail::dummy_constructor();
   out2 = boost::u32regex_replace(out2, utf8_arch2(), utf8_arch2(), e1, (const unsigned char*)(pb), flgs);
   boost::output_iterator_archetype<UChar> out3 = boost::detail::dummy_constructor();
   out3 = boost::u32regex_replace(out3, utf16_arch(), utf16_arch(), e1, (const UChar*)(pb), flgs);
   boost::output_iterator_archetype<wchar_t> out4 = boost::detail::dummy_constructor();
   out4 = boost::u32regex_replace(out4, wchar_arch(), wchar_arch(), e1, (const wchar_t*)(pb), flgs);

   out1 = boost::u32regex_replace(out1, utf8_arch1(), utf8_arch1(), e1, s1, flgs);
   out2 = boost::u32regex_replace(out2, utf8_arch2(), utf8_arch2(), e1, s1, flgs);
   out3 = boost::u32regex_replace(out3, utf16_arch(), utf16_arch(), e1, s1, flgs);
   out4 = boost::u32regex_replace(out4, wchar_arch(), wchar_arch(), e1, s1, flgs);
   out1 = boost::u32regex_replace(out1, utf8_arch1(), utf8_arch1(), e1, s2, flgs);
   out2 = boost::u32regex_replace(out2, utf8_arch2(), utf8_arch2(), e1, s2, flgs);
   out3 = boost::u32regex_replace(out3, utf16_arch(), utf16_arch(), e1, s2, flgs);
   out4 = boost::u32regex_replace(out4, wchar_arch(), wchar_arch(), e1, s2, flgs);
   out1 = boost::u32regex_replace(out1, utf8_arch1(), utf8_arch1(), e1, us, flgs);
   out2 = boost::u32regex_replace(out2, utf8_arch2(), utf8_arch2(), e1, us, flgs);
   out3 = boost::u32regex_replace(out3, utf16_arch(), utf16_arch(), e1, us, flgs);
   out4 = boost::u32regex_replace(out4, wchar_arch(), wchar_arch(), e1, us, flgs);
   // string overloads:
   s1 = boost::u32regex_replace(s1, e1, (const char*)(pb), flgs);
   s2 = boost::u32regex_replace(s2, e1, (const wchar_t*)(pb), flgs);
   s1 = boost::u32regex_replace(s1, e1, s1, flgs);
   s2 = boost::u32regex_replace(s2, e1, s2, flgs);
   s1 = boost::u32regex_replace(s1, e1, (const char*)(pb));
   s2 = boost::u32regex_replace(s2, e1, (const wchar_t*)(pb));
   s1 = boost::u32regex_replace(s1, e1, s1);
   s2 = boost::u32regex_replace(s2, e1, s2);

   std::vector<int> subs1;
   int subs2[2] = { 1, 2 };

   check_token_iterator(boost::make_u32regex_token_iterator((const char*)(pb), e1, 0, boost::regex_constants::match_default));
   check_token_iterator(boost::make_u32regex_token_iterator((const char*)(pb), e1, 0));
   check_token_iterator(boost::make_u32regex_token_iterator((const char*)(pb), e1));
   check_token_iterator(boost::make_u32regex_token_iterator((const UChar*)(pb), e1, 0, boost::regex_constants::match_default));
   check_token_iterator(boost::make_u32regex_token_iterator((const UChar*)(pb), e1, 0));
   check_token_iterator(boost::make_u32regex_token_iterator((const UChar*)(pb), e1));
   check_token_iterator(boost::make_u32regex_token_iterator((const wchar_t*)(pb), e1, 0, boost::regex_constants::match_default));
   check_token_iterator(boost::make_u32regex_token_iterator((const wchar_t*)(pb), e1, 0));
   check_token_iterator(boost::make_u32regex_token_iterator((const wchar_t*)(pb), e1));
   check_token_iterator(boost::make_u32regex_token_iterator(s1, e1, 0, boost::regex_constants::match_default));
   check_token_iterator(boost::make_u32regex_token_iterator(s1, e1, 0));
   check_token_iterator(boost::make_u32regex_token_iterator(s1, e1));
   check_token_iterator(boost::make_u32regex_token_iterator(s2, e1, 0, boost::regex_constants::match_default));
   check_token_iterator(boost::make_u32regex_token_iterator(s2, e1, 0));
   check_token_iterator(boost::make_u32regex_token_iterator(s2, e1));
   check_token_iterator(boost::make_u32regex_token_iterator(us, e1, 0, boost::regex_constants::match_default));
   check_token_iterator(boost::make_u32regex_token_iterator(us, e1, 0));
   check_token_iterator(boost::make_u32regex_token_iterator(us, e1));

   check_token_iterator(boost::make_u32regex_token_iterator((const char*)(pb), e1, subs2, boost::regex_constants::match_default));
   check_token_iterator(boost::make_u32regex_token_iterator((const char*)(pb), e1, subs2));
   check_token_iterator(boost::make_u32regex_token_iterator((const UChar*)(pb), e1, subs2, boost::regex_constants::match_default));
   check_token_iterator(boost::make_u32regex_token_iterator((const UChar*)(pb), e1, subs2));
   check_token_iterator(boost::make_u32regex_token_iterator((const wchar_t*)(pb), e1, subs2, boost::regex_constants::match_default));
   check_token_iterator(boost::make_u32regex_token_iterator((const wchar_t*)(pb), e1, subs2));
   check_token_iterator(boost::make_u32regex_token_iterator(s1, e1, subs2, boost::regex_constants::match_default));
   check_token_iterator(boost::make_u32regex_token_iterator(s1, e1, subs2));
   check_token_iterator(boost::make_u32regex_token_iterator(s2, e1, subs2, boost::regex_constants::match_default));
   check_token_iterator(boost::make_u32regex_token_iterator(s2, e1, subs2));
   check_token_iterator(boost::make_u32regex_token_iterator(us, e1, subs2, boost::regex_constants::match_default));
   check_token_iterator(boost::make_u32regex_token_iterator(us, e1, subs2));

   check_token_iterator(boost::make_u32regex_token_iterator((const char*)(pb), e1, subs1, boost::regex_constants::match_default));
   check_token_iterator(boost::make_u32regex_token_iterator((const char*)(pb), e1, subs1));
   check_token_iterator(boost::make_u32regex_token_iterator((const UChar*)(pb), e1, subs1, boost::regex_constants::match_default));
   check_token_iterator(boost::make_u32regex_token_iterator((const UChar*)(pb), e1, subs1));
   check_token_iterator(boost::make_u32regex_token_iterator((const wchar_t*)(pb), e1, subs1, boost::regex_constants::match_default));
   check_token_iterator(boost::make_u32regex_token_iterator((const wchar_t*)(pb), e1, subs1));
   check_token_iterator(boost::make_u32regex_token_iterator(s1, e1, subs1, boost::regex_constants::match_default));
   check_token_iterator(boost::make_u32regex_token_iterator(s1, e1, subs1));
   check_token_iterator(boost::make_u32regex_token_iterator(s2, e1, subs1, boost::regex_constants::match_default));
   check_token_iterator(boost::make_u32regex_token_iterator(s2, e1, subs1));
   check_token_iterator(boost::make_u32regex_token_iterator(us, e1, subs1, boost::regex_constants::match_default));
   check_token_iterator(boost::make_u32regex_token_iterator(us, e1, subs1));

   check_iterator(boost::make_u32regex_iterator((const char*)(pb), e1, boost::regex_constants::match_default));
   check_iterator(boost::make_u32regex_iterator((const char*)(pb), e1));
   check_iterator(boost::make_u32regex_iterator((const UChar*)(pb), e1, boost::regex_constants::match_default));
   check_iterator(boost::make_u32regex_iterator((const UChar*)(pb), e1));
   check_iterator(boost::make_u32regex_iterator((const wchar_t*)(pb), e1, boost::regex_constants::match_default));
   check_iterator(boost::make_u32regex_iterator((const wchar_t*)(pb), e1));
   check_iterator(boost::make_u32regex_iterator(s1, e1, boost::regex_constants::match_default));
   check_iterator(boost::make_u32regex_iterator(s2, e1));
   check_iterator(boost::make_u32regex_iterator(us, e1));

#endif   
   return 0;
}

#else

int main()
{
   return 0;
}

#endif
