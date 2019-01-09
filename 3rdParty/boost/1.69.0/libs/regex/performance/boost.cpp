///////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_
//

#include "performance.hpp"
#include <boost/regex.hpp>
#include <boost/version.hpp>
#include <boost/lexical_cast.hpp>

struct boost_regex : public abstract_regex
{
private:
   boost::regex e;
   boost::cmatch what;
public:
   virtual bool set_expression(const char* pe, bool isperl)
   {
      e.assign(pe, isperl ? boost::regex::perl : boost::regex::extended);
      return e.status() == 0;
   }
   virtual bool match_test(const char* text);
   virtual unsigned find_all(const char* text);
   virtual std::string name();

   struct initializer
   {
      initializer()
      {
         boost_regex::register_instance(boost::shared_ptr<abstract_regex>(new boost_regex));
      }
      void do_nothing()const {}
   };
   static const initializer init;
};

const boost_regex::initializer boost_regex::init;


bool boost_regex::match_test(const char * text)
{
   return regex_match(text, what, e);
}

unsigned boost_regex::find_all(const char * text)
{
   boost::regex_iterator<const char*> i(text, text + std::strlen(text), e), j;
   unsigned count = 0;
   while(i != j)
   {
      ++i;
      ++count;
   }
   return count;
}

std::string boost_regex::name()
{
   init.do_nothing();
   return boost_name();
}
