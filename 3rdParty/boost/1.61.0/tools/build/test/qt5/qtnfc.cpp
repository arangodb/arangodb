// (c) Copyright Juergen Hunold 2016
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE QtNfc

#include <QtNfc>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE( defines)
{
    BOOST_CHECK_EQUAL(BOOST_IS_DEFINED(QT_CORE_LIB), true);
    BOOST_CHECK_EQUAL(BOOST_IS_DEFINED(QT_NFC_LIB), true);
}

/*!
  Try to detect a device
 */
BOOST_AUTO_TEST_CASE( nfc )
{
    QNearFieldManager manager;
    if (!manager.isAvailable())
    {
        BOOST_TEST_MESSAGE("No Nfc");
    }
}
