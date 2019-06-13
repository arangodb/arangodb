///////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_
//

#include <regex.h>

int main()
{
   regex_t pe;
   int r = regcomp(&pe, "foo", REG_EXTENDED);
   regfree(&pe);
   return r;
}
