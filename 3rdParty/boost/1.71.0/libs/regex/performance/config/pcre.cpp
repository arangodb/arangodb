///////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_
//

#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 8

#include <pcre2.h>

int main()
{
   pcre2_match_data* pdata = pcre2_match_data_create(30, NULL);
   pcre2_match_data_free(pdata);
   return 0;
}
