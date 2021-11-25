// Boost.Range library
//
//  Copyright Robin Eckert 2015. Use, modification and distribution is
//  subject to the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//
// For more information, see http://www.boost.org/libs/range/
//
//[ref_unwrapped_example
#include <boost/range/adaptor/ref_unwrapped.hpp>
#include <iostream>
#include <vector>

struct example
{
  int value;
};

int main(int argc, const char* argv[])
{
//<-
#if !defined(BOOST_NO_CXX11_DECLTYPE) \
 && !defined(BOOST_NO_CXX11_RANGE_BASED_FOR) \
 && !defined(BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX) \
 && !defined(BOOST_NO_CXX11_AUTO_DECLARATIONS)
//->
    using boost::adaptors::ref_unwrapped;

    example one{1};
    example two{2};
    example three{3};

    std::vector<std::reference_wrapper<example> > input{one, two, three};

    for (auto&& entry : input | ref_unwrapped)
    {
      std::cout << entry.value;
    }

    return 0;
//<-
#endif
//->
}
//]
