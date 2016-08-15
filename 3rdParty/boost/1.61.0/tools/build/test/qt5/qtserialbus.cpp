// (c) Copyright Juergen Hunold 2016
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE QtSerialBus

#include <QtSerialBus>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE( defines)
{
    BOOST_CHECK_EQUAL(BOOST_IS_DEFINED(QT_CORE_LIB), true);
    BOOST_CHECK_EQUAL(BOOST_IS_DEFINED(QT_SERIALBUS_LIB), true);
}

/*!
  create a canbus instance
 */
BOOST_AUTO_TEST_CASE( serialBus )
{
    auto canbus = QCanBus::instance();
    Q_UNUSED(canbus);
}
