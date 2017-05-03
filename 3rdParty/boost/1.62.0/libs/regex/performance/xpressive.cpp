///////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_
//

#include <boost/config.hpp>

#include "performance.hpp"
#include <boost/xpressive/xpressive.hpp>

using namespace boost::xpressive;

struct xpressive_regex : public abstract_regex
{
private:
   cregex e;
   cmatch what;
public:
   virtual bool set_expression(const char* pe, bool isperl)
   {
      if(!isperl)
         return false;
      try
      {
         e = cregex::compile(pe, regex_constants::ECMAScript);
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
         xpressive_regex::register_instance(boost::shared_ptr<abstract_regex>(new xpressive_regex));
      }
      void do_nothing()const {}
   };
   static const initializer init;
};

const xpressive_regex::initializer xpressive_regex::init;


bool xpressive_regex::match_test(const char * text)
{
   return regex_match(text, what, e);
}

unsigned xpressive_regex::find_all(const char * text)
{
   cregex_token_iterator i(text, text + std::strlen(text), e), j;
   unsigned count = 0;
   while(i != j)
   {
      ++i;
      ++count;
   }
   return count;
}

std::string xpressive_regex::name()
{
   init.do_nothing();
   return "boost::xpressive::cregex";
}

