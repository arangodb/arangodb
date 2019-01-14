/*
 *
 * Copyright (c) 2010
 * John Maddock
 *
 * Use, modification and distribution are subject to the 
 * Boost Software License, Version 1.0. (See accompanying file 
 * LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include <unicode/uversion.h>
#include <unicode/utypes.h>
#include <unicode/uchar.h>
#include <unicode/coll.h>
#include <boost/scoped_ptr.hpp>
#include <iostream>
#include <iomanip>

#if defined(_MSC_VER) && !defined(_DLL)
//#error "Mixing ICU with a static runtime doesn't work"
#endif

void print_error(UErrorCode err, const char* func)
{
   std::cerr << "Error from function " << func << " with error: " << ::u_errorName(err) << std::endl;
}

int main()
{
   // To detect possible binary mismatches between the installed ICU build, and whatever
   // C++ std lib's we're using, we need to:
   // * Make sure we call ICU C++ API's
   // * Make sure we call std lib C++ API's as well (cout).
   // * Be sure this program is run, not just built.
   UErrorCode err = U_ZERO_ERROR;
   UChar32 c = ::u_charFromName(U_UNICODE_CHAR_NAME, "GREEK SMALL LETTER ALPHA", &err);
   std::cout << (int)c << std::endl;
   if(err > 0)
   {
      print_error(err, "u_charFromName");
      return err;
   }
   U_NAMESPACE_QUALIFIER Locale l;
   boost::scoped_ptr<U_NAMESPACE_QUALIFIER Collator> p_col(U_NAMESPACE_QUALIFIER Collator::createInstance(l, err));
   if(err > 0)
   {
      print_error(err, "Collator::createInstance");
      return err;
   }
   return err > 0 ? err : 0;
}
