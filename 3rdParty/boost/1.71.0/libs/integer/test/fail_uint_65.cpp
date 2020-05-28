//  Copyright John Maddock 2012.
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/integer.hpp>
#include <iostream>

int main()
{
 std::cout << std::numeric_limits<boost::uint_t<65>::least>::digits;
 return 0;
}
