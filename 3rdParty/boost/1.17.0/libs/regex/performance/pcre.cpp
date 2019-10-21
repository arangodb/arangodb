///////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_
//

#ifdef TEST_PCRE2

#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 8

#include "performance.hpp"
#include <pcre2.h>
#include <boost/version.hpp>
#include <boost/lexical_cast.hpp>

struct pcre_regex : public abstract_regex
{
private:
   pcre2_code* pe;
   pcre2_match_data* pdata;
public:
   pcre_regex()
      : pe(0) 
   {
      pdata = pcre2_match_data_create(30, NULL);
   }
   ~pcre_regex()
   {
      if(pe)
         pcre2_code_free(pe);
      pcre2_match_data_free(pdata);
   }
   virtual bool set_expression(const char* pat, bool isperl)
   {
      if(!isperl)
         return false;
      if(pe)
         pcre2_code_free(pe);
      int errorcode = 0;
      PCRE2_SIZE erroroffset;
      pe = pcre2_compile((PCRE2_SPTR)pat, std::strlen(pat), PCRE2_MULTILINE, &errorcode, &erroroffset, NULL);
      return pe ? true : false;
   }
   virtual bool match_test(const char* text);
   virtual unsigned find_all(const char* text);
   virtual std::string name();

   struct initializer
   {
      initializer()
      {
         pcre_regex::register_instance(boost::shared_ptr<abstract_regex>(new pcre_regex));
      }
      void do_nothing()const {}
   };
   static const initializer init;
};

const pcre_regex::initializer pcre_regex::init;


bool pcre_regex::match_test(const char * text)
{
   int r = pcre2_match(pe, (PCRE2_SPTR)text, std::strlen(text), 0, PCRE2_ANCHORED, pdata, NULL);
   return r >= 0;
}

unsigned pcre_regex::find_all(const char * text)
{
   unsigned count = 0;
   int flags = 0;
   const char* end = text + std::strlen(text);
   while(pcre2_match(pe, (PCRE2_SPTR)text, end - text, 0, flags, pdata, NULL) >= 0)
   {
      ++count;
      PCRE2_SIZE* v = pcre2_get_ovector_pointer(pdata);
      text += v[1];
      if(v[0] == v[1])
         ++text;
      if(*text)
      {
         flags = *(text - 1) == '\n' ? 0 : PCRE2_NOTBOL;
      }
   }
   return count;
}

std::string pcre_regex::name()
{
   init.do_nothing();
   return std::string("PCRE-") + boost::lexical_cast<std::string>(PCRE2_MAJOR) + "." + boost::lexical_cast<std::string>(PCRE2_MINOR);
}

#endif
