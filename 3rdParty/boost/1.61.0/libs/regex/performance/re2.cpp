///////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_
//

#ifdef TEST_RE2

#include "performance.hpp"
#include <boost/scoped_ptr.hpp>
#include <re2.h>

using namespace re2;

struct re2_regex : public abstract_regex
{
private:
   boost::scoped_ptr<RE2> pat;
public:
   re2_regex() {}
   ~re2_regex(){}
   virtual bool set_expression(const char* pp, bool isperl)
   {
      if(!isperl)
         return false;
      std::string s("(?m)");
      s += pp;
      pat.reset(new RE2(s));
      return pat->ok();
   }
   virtual bool match_test(const char* text);
   virtual unsigned find_all(const char* text);
   virtual std::string name();

   struct initializer
   {
      initializer()
      {
         re2_regex::register_instance(boost::shared_ptr<abstract_regex>(new re2_regex));
      }
      void do_nothing()const {}
   };
   static const initializer init;
};

const re2_regex::initializer re2_regex::init;


bool re2_regex::match_test(const char * text)
{
   return RE2::FullMatch(text, *pat);
}

unsigned re2_regex::find_all(const char * text)
{
   unsigned count = 0;
   StringPiece input(text);
   while(RE2::FindAndConsume(&input, *pat))
   {
      ++count;
   }
   return count;
}

std::string re2_regex::name()
{
   init.do_nothing();
   return "RE2";
}

#endif
