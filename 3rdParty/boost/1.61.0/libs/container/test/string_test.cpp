//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/string.hpp>
#include <string>
#include <vector>
#include <boost/container/detail/algorithm.hpp> //equal()
#include <cstring>
#include <cstdio>
#include <cstddef>
#include <new>
#include "dummy_test_allocator.hpp"
#include "check_equal_containers.hpp"
#include "expand_bwd_test_allocator.hpp"
#include "expand_bwd_test_template.hpp"
#include "propagate_allocator_test.hpp"
#include "default_init_test.hpp"
#include "../../intrusive/test/iterator_test.hpp"

using namespace boost::container;

typedef test::simple_allocator<char>           SimpleCharAllocator;
typedef basic_string<char, std::char_traits<char>, SimpleCharAllocator> SimpleString;
typedef test::simple_allocator<SimpleString>    SimpleStringAllocator;
typedef test::simple_allocator<wchar_t>              SimpleWCharAllocator;
typedef basic_string<wchar_t, std::char_traits<wchar_t>, SimpleWCharAllocator> SimpleWString;
typedef test::simple_allocator<SimpleWString>         SimpleWStringAllocator;

namespace boost {
namespace container {

//Explicit instantiations of container::basic_string
template class basic_string<char,    std::char_traits<char>, SimpleCharAllocator>;
template class basic_string<wchar_t, std::char_traits<wchar_t>, SimpleWCharAllocator>;
template class basic_string<char,    std::char_traits<char>, std::allocator<char> >;
template class basic_string<wchar_t, std::char_traits<wchar_t>, std::allocator<wchar_t> >;

//Explicit instantiation of container::vectors of container::strings
template class vector<SimpleString, SimpleStringAllocator>;
template class vector<SimpleWString, SimpleWStringAllocator>;

}}

struct StringEqual
{
   template<class Str1, class Str2>
   bool operator ()(const Str1 &string1, const Str2 &string2) const
   {
      if(string1.size() != string2.size())
         return false;
      return std::char_traits<typename Str1::value_type>::compare
         (string1.c_str(), string2.c_str(), string1.size()) == 0;
   }
};

//Function to check if both lists are equal
template<class StrVector1, class StrVector2>
bool CheckEqualStringVector(StrVector1 *strvect1, StrVector2 *strvect2)
{
   StringEqual comp;
   return boost::container::algo_equal(strvect1->begin(), strvect1->end(),
                     strvect2->begin(), comp);
}

template<class ForwardIt>
ForwardIt unique(ForwardIt first, ForwardIt const last)
{
   if(first == last){
      ForwardIt i = first;
      //Find first adjacent pair
      while(1){
         if(++i == last){
            return last;
         }
         else if(*first == *i){
            break;
         }
         ++first;
      }
      //Now overwrite skipping adjacent elements
      while (++i != last) {
         if (!(*first == *i)) {
            *(++first) = boost::move(*i);
         }
      }
      ++first;
   }
   return first;
}

template<class CharType>
struct string_literals;

template<>
struct string_literals<char>
{
   static const char *String()
      {  return "String";  }
   static const char *Prefix()
      {  return "Prefix";  }
   static const char *Suffix()
      {  return "Suffix";  }
   static const char *LongString()
      {  return "LongLongLongLongLongLongLongLongLongLongLongLongLongString";  }
   static char Char()
      {  return 'C';  }
   static void sprintf_number(char *buf, int number)
   {
      std::sprintf(buf, "%i", number);
   }
};

template<>
struct string_literals<wchar_t>
{
   static const wchar_t *String()
      {  return L"String";  }
   static const wchar_t *Prefix()
      {  return L"Prefix";  }
   static const wchar_t *Suffix()
      {  return L"Suffix";  }
   static const wchar_t *LongString()
      {  return L"LongLongLongLongLongLongLongLongLongLongLongLongLongString";  }
   static wchar_t Char()
      {  return L'C';  }
   static void sprintf_number(wchar_t *buffer, unsigned int number)
   {
      //For compilers without wsprintf, print it backwards
      const wchar_t *digits = L"0123456789";
      wchar_t *buf = buffer;

      while(1){
         int rem = number % 10;
         number  = number / 10;

         *buf = digits[rem];
         ++buf;
         if(!number){
            *buf = 0;
            break;
         }
      }

   }
};

template<class CharType>
int string_test()
{
   typedef std::basic_string<CharType> StdString;
   typedef vector<StdString>  StdStringVector;
   typedef basic_string<CharType> BoostString;
   typedef vector<BoostString> BoostStringVector;

   const int MaxSize = 100;

   {
      BoostStringVector *boostStringVect = new BoostStringVector;
      StdStringVector *stdStringVect = new StdStringVector;
      BoostString auxBoostString;
      StdString auxStdString(StdString(auxBoostString.begin(), auxBoostString.end() ));

      CharType buffer [20];

      //First, push back
      for(int i = 0; i < MaxSize; ++i){
         auxBoostString = string_literals<CharType>::String();
         auxStdString = string_literals<CharType>::String();
         string_literals<CharType>::sprintf_number(buffer, i);
         auxBoostString += buffer;
         auxStdString += buffer;
         boostStringVect->push_back(auxBoostString);
         stdStringVect->push_back(auxStdString);
      }

      if(!CheckEqualStringVector(boostStringVect, stdStringVect)){
         return 1;
      }

      //Now push back moving
      for(int i = 0; i < MaxSize; ++i){
         auxBoostString = string_literals<CharType>::String();
         auxStdString = string_literals<CharType>::String();
         string_literals<CharType>::sprintf_number(buffer, i);
         auxBoostString += buffer;
         auxStdString += buffer;
         boostStringVect->push_back(boost::move(auxBoostString));
         stdStringVect->push_back(auxStdString);
      }

      if(!CheckEqualStringVector(boostStringVect, stdStringVect)){
         return 1;
      }

      //push front
      for(int i = 0; i < MaxSize; ++i){
         auxBoostString = string_literals<CharType>::String();
         auxStdString = string_literals<CharType>::String();
         string_literals<CharType>::sprintf_number(buffer, i);
         auxBoostString += buffer;
         auxStdString += buffer;
         boostStringVect->insert(boostStringVect->begin(), auxBoostString);
         stdStringVect->insert(stdStringVect->begin(), auxStdString);
      }

      if(!CheckEqualStringVector(boostStringVect, stdStringVect)){
         return 1;
      }

      //Now push front moving
      for(int i = 0; i < MaxSize; ++i){
         auxBoostString = string_literals<CharType>::String();
         auxStdString = string_literals<CharType>::String();
         string_literals<CharType>::sprintf_number(buffer, i);
         auxBoostString += buffer;
         auxStdString += buffer;
         boostStringVect->insert(boostStringVect->begin(), boost::move(auxBoostString));
         stdStringVect->insert(stdStringVect->begin(), auxStdString);
      }

      if(!CheckEqualStringVector(boostStringVect, stdStringVect)){
         return 1;
      }

      //Now test long and short representation swapping

      //Short first
      auxBoostString = string_literals<CharType>::String();
      auxStdString = string_literals<CharType>::String();
      BoostString boost_swapper;
      StdString std_swapper;
      boost_swapper.swap(auxBoostString);
      std_swapper.swap(auxStdString);
      if(!StringEqual()(auxBoostString, auxStdString))
         return 1;
      if(!StringEqual()(boost_swapper, std_swapper))
         return 1;
      boost_swapper.swap(auxBoostString);
      std_swapper.swap(auxStdString);
      if(!StringEqual()(auxBoostString, auxStdString))
         return 1;
      if(!StringEqual()(boost_swapper, std_swapper))
         return 1;

      //Shrink_to_fit
      auxBoostString.shrink_to_fit();
      StdString(auxStdString).swap(auxStdString);
      if(!StringEqual()(auxBoostString, auxStdString))
         return 1;

      //Reserve + shrink_to_fit
      auxBoostString.reserve(boost_swapper.size()*2+1);
      auxStdString.reserve(std_swapper.size()*2+1);
      if(!StringEqual()(auxBoostString, auxStdString))
         return 1;

      auxBoostString.shrink_to_fit();
      StdString(auxStdString).swap(auxStdString);
      if(!StringEqual()(auxBoostString, auxStdString))
         return 1;

      //Long string
      auxBoostString = string_literals<CharType>::LongString();
      auxStdString   = string_literals<CharType>::LongString();
      boost_swapper = BoostString();
      std_swapper = StdString();
      boost_swapper.swap(auxBoostString);
      std_swapper.swap(auxStdString);
      if(!StringEqual()(auxBoostString, auxStdString))
         return 1;
      if(!StringEqual()(boost_swapper, std_swapper))
         return 1;
      boost_swapper.swap(auxBoostString);
      std_swapper.swap(auxStdString);

      //Shrink_to_fit
      auxBoostString.shrink_to_fit();
      StdString(auxStdString).swap(auxStdString);
      if(!StringEqual()(auxBoostString, auxStdString))
         return 1;

      auxBoostString.clear();
      auxStdString.clear();
      auxBoostString.shrink_to_fit();
      StdString(auxStdString).swap(auxStdString);
      if(!StringEqual()(auxBoostString, auxStdString))
         return 1;

      //No sort
      std::sort(boostStringVect->begin(), boostStringVect->end());
      std::sort(stdStringVect->begin(), stdStringVect->end());
      if(!CheckEqualStringVector(boostStringVect, stdStringVect)) return 1;

      const CharType *prefix    = string_literals<CharType>::Prefix();
      const int  prefix_size    = std::char_traits<CharType>::length(prefix);
      const CharType *sufix      = string_literals<CharType>::Suffix();

      for(int i = 0; i < MaxSize; ++i){
         (*boostStringVect)[i].append(sufix);
         (*stdStringVect)[i].append(sufix);
         (*boostStringVect)[i].insert((*boostStringVect)[i].begin(),
                                    prefix, prefix + prefix_size);
         (*stdStringVect)[i].insert((*stdStringVect)[i].begin(),
                                    prefix, prefix + prefix_size);
      }

      if(!CheckEqualStringVector(boostStringVect, stdStringVect)) return 1;

      for(int i = 0; i < MaxSize; ++i){
         std::reverse((*boostStringVect)[i].begin(), (*boostStringVect)[i].end());
         std::reverse((*stdStringVect)[i].begin(), (*stdStringVect)[i].end());
      }

      if(!CheckEqualStringVector(boostStringVect, stdStringVect)) return 1;

      for(int i = 0; i < MaxSize; ++i){
         std::reverse((*boostStringVect)[i].begin(), (*boostStringVect)[i].end());
         std::reverse((*stdStringVect)[i].begin(), (*stdStringVect)[i].end());
      }

      if(!CheckEqualStringVector(boostStringVect, stdStringVect)) return 1;

      for(int i = 0; i < MaxSize; ++i){
         std::sort(boostStringVect->begin(), boostStringVect->end());
         std::sort(stdStringVect->begin(), stdStringVect->end());
      }

      if(!CheckEqualStringVector(boostStringVect, stdStringVect)) return 1;

      for(int i = 0; i < MaxSize; ++i){
         (*boostStringVect)[i].replace((*boostStringVect)[i].begin(),
                                    (*boostStringVect)[i].end(),
                                    string_literals<CharType>::String());
         (*stdStringVect)[i].replace((*stdStringVect)[i].begin(),
                                    (*stdStringVect)[i].end(),
                                    string_literals<CharType>::String());
      }

      if(!CheckEqualStringVector(boostStringVect, stdStringVect)) return 1;

      boostStringVect->erase((unique)(boostStringVect->begin(), boostStringVect->end()),
                           boostStringVect->end());
      stdStringVect->erase((unique)(stdStringVect->begin(), stdStringVect->end()),
                           stdStringVect->end());
      if(!CheckEqualStringVector(boostStringVect, stdStringVect)) return 1;

      //Check addition
      {
         BoostString bs2 = string_literals<CharType>::String();
         StdString   ss2 = string_literals<CharType>::String();
         BoostString bs3 = string_literals<CharType>::Suffix();
         StdString   ss3 = string_literals<CharType>::Suffix();
         BoostString bs4 = bs2 + bs3;
         StdString   ss4 = ss2 + ss3;
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         bs4 = bs2 + BoostString();
         ss4 = ss2 + StdString();
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         bs4 = BoostString() + bs2;
         ss4 = StdString() + ss2;
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         bs4 = BoostString() + boost::move(bs2);
         ss4 = StdString() + boost::move(ss2);
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         bs2 = string_literals<CharType>::String();
         ss2 = string_literals<CharType>::String();
         bs4 = boost::move(bs2) + BoostString();
         ss4 = boost::move(ss2) + StdString();
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         bs2 = string_literals<CharType>::String();
         ss2 = string_literals<CharType>::String();
         bs4 = string_literals<CharType>::Prefix() + boost::move(bs2);
         ss4 = string_literals<CharType>::Prefix() + boost::move(ss2);
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         bs2 = string_literals<CharType>::String();
         ss2 = string_literals<CharType>::String();
         bs4 = boost::move(bs2) + string_literals<CharType>::Suffix();
         ss4 = boost::move(ss2) + string_literals<CharType>::Suffix();
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         bs2 = string_literals<CharType>::String();
         ss2 = string_literals<CharType>::String();
         bs4 = string_literals<CharType>::Prefix() + bs2;
         ss4 = string_literals<CharType>::Prefix() + ss2;
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         bs2 = string_literals<CharType>::String();
         ss2 = string_literals<CharType>::String();
         bs4 = bs2 + string_literals<CharType>::Suffix();
         ss4 = ss2 + string_literals<CharType>::Suffix();
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         bs2 = string_literals<CharType>::String();
         ss2 = string_literals<CharType>::String();
         bs4 = string_literals<CharType>::Char() + bs2;
         ss4 = string_literals<CharType>::Char() + ss2;
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         bs2 = string_literals<CharType>::String();
         ss2 = string_literals<CharType>::String();
         bs4 = bs2 + string_literals<CharType>::Char();
         ss4 = ss2 + string_literals<CharType>::Char();
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         //Check front/back/begin/end

         if(bs4.front() != *ss4.begin())
            return 1;

         if(bs4.back() != *(ss4.end()-1))
            return 1;

         bs4.pop_back();
         ss4.erase(ss4.end()-1);
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         if(*bs4.begin() != *ss4.begin())
            return 1;
         if(*bs4.cbegin() != *ss4.begin())
            return 1;
         if(*bs4.rbegin() != *ss4.rbegin())
            return 1;
         if(*bs4.crbegin() != *ss4.rbegin())
            return 1;
         if(*(bs4.end()-1) != *(ss4.end()-1))
            return 1;
         if(*(bs4.cend()-1) != *(ss4.end()-1))
            return 1;
         if(*(bs4.rend()-1) != *(ss4.rend()-1))
            return 1;
         if(*(bs4.crend()-1) != *(ss4.rend()-1))
            return 1;
      }

      //When done, delete vector
      delete boostStringVect;
      delete stdStringVect;
   }
   return 0;
}

bool test_expand_bwd()
{
   //Now test all back insertion possibilities
   typedef test::expand_bwd_test_allocator<char>
      allocator_type;
   typedef basic_string<char, std::char_traits<char>, allocator_type>
      string_type;
   return  test::test_all_expand_bwd<string_type>();
}

struct boost_container_string;

namespace boost { namespace container {   namespace test {

template<>
struct alloc_propagate_base<boost_container_string>
{
   template <class T, class Allocator>
   struct apply
   {
      typedef boost::container::basic_string<T, std::char_traits<T>, Allocator> type;
   };
};

}}}   //namespace boost::container::test

int main()
{
   if(string_test<char>()){
      return 1;
   }

   if(string_test<wchar_t>()){
      return 1;
   }

   ////////////////////////////////////
   //    Backwards expansion test
   ////////////////////////////////////
   if(!test_expand_bwd())
      return 1;

   ////////////////////////////////////
   //    Allocator propagation testing
   ////////////////////////////////////
   if(!boost::container::test::test_propagate_allocator<boost_container_string>())
      return 1;

   ////////////////////////////////////
   //    Default init test
   ////////////////////////////////////
   if(!test::default_init_test< basic_string<char, std::char_traits<char>, test::default_init_allocator<char> > >()){
      std::cerr << "Default init test failed" << std::endl;
      return 1;
   }

   if(!test::default_init_test< basic_string<wchar_t, std::char_traits<wchar_t>, test::default_init_allocator<wchar_t> > >()){
      std::cerr << "Default init test failed" << std::endl;
      return 1;
   }

   ////////////////////////////////////
   //    Iterator testing
   ////////////////////////////////////
   {
      typedef boost::container::basic_string<char> cont_int;
      cont_int a; a.push_back(char(1)); a.push_back(char(2)); a.push_back(char(3));
      boost::intrusive::test::test_iterator_random< cont_int >(a);
      if(boost::report_errors() != 0) {
         return 1;
      }
   }
   {
      typedef boost::container::basic_string<wchar_t> cont_int;
      cont_int a; a.push_back(wchar_t(1)); a.push_back(wchar_t(2)); a.push_back(wchar_t(3));
      boost::intrusive::test::test_iterator_random< cont_int >(a);
      if(boost::report_errors() != 0) {
         return 1;
      }
   }

   return 0;
}

#include <boost/container/detail/config_end.hpp>
