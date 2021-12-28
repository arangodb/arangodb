// Copyright Daniel Wallin 2005.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/parameter.hpp>
#include "basics.hpp"

namespace test {

    template <typename Params>
    void f(Params const &)
    {
    }
}

int main()
{
    test::f((
        test::_name = 1
      , test::_value = 1
      , test::_index = 1
      , test::_tester = 1
      , test::_value = 1 // repeated keyword: should not compile
    ));
    return 0;
}

