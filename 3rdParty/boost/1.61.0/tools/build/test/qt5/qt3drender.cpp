// (c) Copyright Juergen Hunold 2015
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE Qt3DRender
#include <Qt3DRender>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE (defines)
{
    BOOST_CHECK_EQUAL(BOOST_IS_DEFINED(QT_3DCORE_LIB), true);
    BOOST_CHECK_EQUAL(BOOST_IS_DEFINED(QT_3DRENDER_LIB), true);
}

BOOST_AUTO_TEST_CASE ( sample_code )
{
    Qt3DCore::QEntity rootEntity;
    Qt3DRender::QMaterial material(&rootEntity);
}
