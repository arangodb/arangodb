///////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_
//

#ifdef TEST_POSIX

#include "performance.hpp"
#include <boost/lexical_cast.hpp>

#include <regex.h>

struct posix_regex : public abstract_regex
{
private:
   regex_t pe, pe2;
   bool init;
public:
   posix_regex() : init(false) {}
   ~posix_regex()
   {
      if(init)
      {
         regfree(&pe);
         regfree(&pe2);
      }
   }
   virtual bool set_expression(const char* pat, bool isperl)
   {
      if(isperl)
         return false;
      if(init)
      {
         regfree(&pe);
         regfree(&pe2);
      }
      else
         init = true;
      int r = regcomp(&pe, pat, REG_EXTENDED);
      std::string s(pat);
      if(s.size() && (s[0] != '^'))
         s.insert(0, 1, '^');
      if(s.size() && (*s.rbegin() != '$'))
         s.append("$");
      r |= regcomp(&pe2, s.c_str(), REG_EXTENDED);
      return r ? false : true;
   }
   virtual bool match_test(const char* text);
   virtual unsigned find_all(const char* text);
   virtual std::string name();

   struct initializer
   {
      initializer()
      {
         posix_regex::register_instance(boost::shared_ptr<abstract_regex>(new posix_regex));
      }
      void do_nothing()const {}
   };
   static const initializer init2;
};

const posix_regex::initializer posix_regex::init2;


bool posix_regex::match_test(const char * text)
{
   regmatch_t m[30];
   int r = regexec(&pe2, text, 30, m, 0);
   return r == 0;
}

unsigned posix_regex::find_all(const char * text)
{
   unsigned count = 0;
   regmatch_t m[30];
   int flags = 0;
   while(regexec(&pe, text, 30, m, flags) == 0)
   {
      ++count;
      text += m[0].rm_eo;
      if(m[0].rm_eo - m[0].rm_so)
         flags = *(text - 1) == '\n' ? 0 : REG_NOTBOL;
      else
         flags = 0;
   }
   return 0;
}

std::string posix_regex::name()
{
   init2.do_nothing();
   return "POSIX";
}

#endif
