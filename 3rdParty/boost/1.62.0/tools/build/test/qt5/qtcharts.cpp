// (c) Copyright Juergen Hunold 2015
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE QtCharts
#include <QtCharts>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE (defines)
{
    BOOST_CHECK_EQUAL(BOOST_IS_DEFINED(QT_WIDGETS_LIB), true);
    BOOST_CHECK_EQUAL(BOOST_IS_DEFINED(QT_CHARTS_LIB), true);
}
