///////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_
//

#include <re2.h>

int main()
{
   return re2::RE2::FullMatch("a", "a") ? 0 : 1;
}
