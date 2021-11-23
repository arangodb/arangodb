/*=============================================================================
    Copyright (c) 2005-2007 Dan Marsden
    Copyright (c) 2005-2007 Joel de Guzman
    Copyright (c) 2014-2015 John Fletcher

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/phoenix.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/stl/algorithm/iteration.hpp>
#include <boost/detail/lightweight_test.hpp>

#include <functional>
#include <vector>
#include <string>
#include <sstream>

namespace
{

    void for_each_test()
    {
        return;
        using boost::phoenix::for_each;
        using boost::phoenix::lambda;
        using boost::phoenix::ref;
        using boost::phoenix::arg_names::arg1;

        std::vector<int> v;
        for (int i = 1; i < 10; i++)
           v.push_back(i);

        int x = 0;
        for_each(arg1, lambda[ref(x) += arg1])(v);
        BOOST_TEST(x == 45);
        return;
    }

}

int main()
{
    for_each_test();
    boost::report_errors();
}
