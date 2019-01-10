/*==============================================================================
    Copyright (c) 2005-2010 Joel de Guzman
    Copyright (c) 2010 Thomas Heller

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <utility> // for std::forward used by boost/range in some cases.
#include <boost/range.hpp>
#include <boost/range/irange.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/phoenix.hpp>
#include <boost/detail/lightweight_test.hpp>

using namespace boost::phoenix::arg_names;
using namespace boost::adaptors;

int foo() { return 5; }

int main()
{
    BOOST_TEST((*boost::begin(boost::irange(0,5) | transformed( arg1)) == 0));

    boost::report_errors();
}
