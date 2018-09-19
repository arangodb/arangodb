///////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_
//

#include <boost/config.hpp>

#ifndef BOOST_NO_CXX11_HDR_REGEX

#include "performance.hpp"
#include <regex>

struct std_regex : public abstract_regex
{
private:
   std::regex e;
   std::cmatch what;
public:
   virtual bool set_expression(const char* pe, bool isperl)
   {
      try
      {
         e.assign(pe, isperl ? std::regex::ECMAScript : std::regex::extended);
      }
      catch(const std::exception&)
      {
         return false;
      }
      return true;
   }
   virtual bool match_test(const char* text);
   virtual unsigned find_all(const char* text);
   virtual std::string name();

   struct initializer
   {
      initializer()
      {
         std_regex::register_instance(boost::shared_ptr<abstract_regex>(new std_regex));
      }
      void do_nothing()const {}
   };
   static const initializer init;
};

const std_regex::initializer std_regex::init;


bool std_regex::match_test(const char * text)
{
   return regex_match(text, what, e);
}

unsigned std_regex::find_all(const char * text)
{
   std::regex_iterator<const char*> i(text, text + std::strlen(text), e), j;
   unsigned count = 0;
   while(i != j)
   {
      ++i;
      ++count;
   }
   return count;
}

std::string std_regex::name()
{
   init.do_nothing();
   return "std::regex";
}

#endif
