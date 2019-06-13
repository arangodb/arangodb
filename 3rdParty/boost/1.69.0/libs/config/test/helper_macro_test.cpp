//  Use, modification and distribution are subject to the  
//  Boost Software License, Version 1.0. (See accompanying file  
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) 

#include <boost/config.hpp>

int test_fallthrough(int n) 
{ 
   switch (n) 
   { 
   case 0: 
      n++; 
      BOOST_FALLTHROUGH; 
   case 1: 
      n++; 
      break; 
   } 
   return n; 
}

int test_unreachable(int i)
{
   if(BOOST_LIKELY(i)) return i;

   throw i;
   BOOST_UNREACHABLE_RETURN(0);
}

BOOST_FORCEINLINE int always_inline(int i){ return ++i; }
BOOST_NOINLINE int never_inline(int i){ return ++i; }

BOOST_NORETURN void always_throw()
{
   throw 0;
}

struct BOOST_MAY_ALIAS aliasing_struct {};
typedef unsigned int BOOST_MAY_ALIAS aliasing_uint;


#define test_fallthrough(x) foobar(x)


int main()
{
   typedef int unused_type BOOST_ATTRIBUTE_UNUSED;
   try
   {
      int result = test_fallthrough BOOST_PREVENT_MACRO_SUBSTITUTION(0);
      BOOST_STATIC_CONSTANT(bool, value = 0);
      result += test_unreachable(1);
      result += always_inline(2);
      result += never_inline(3);
      if(BOOST_UNLIKELY(!result))
         always_throw();
   }
   catch(int)
   {
      return 1;
   }
   return 0;
}

