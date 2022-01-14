
// Copyright 2019 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.

#include <boost/timer/progress_display.hpp>
#include <boost/core/lightweight_test.hpp>
#include <sstream>
#include <cstddef>

int main()
{
    int n = 17;

    std::ostringstream os;
    boost::timer::progress_display pd( n, os, "L1:", "L2:", "L3:" );

    BOOST_TEST_EQ( os.str(), std::string(
        "L1:0%   10   20   30   40   50   60   70   80   90   100%\n"
        "L2:|----|----|----|----|----|----|----|----|----|----|\n"
        "L3:" ) );

    for( int i = 0; i < n; ++i )
    {
        std::size_t m1 = os.str().size();
        ++pd;
        std::size_t m2 = os.str().size();

        BOOST_TEST_LE( m1, m2 );
    }

    BOOST_TEST_EQ( os.str(), std::string(
        "L1:0%   10   20   30   40   50   60   70   80   90   100%\n"
        "L2:|----|----|----|----|----|----|----|----|----|----|\n"
        "L3:***************************************************\n" ) );

    return boost::report_errors();
}
