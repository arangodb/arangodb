//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestInteropQt
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/detail/is_contiguous_iterator.hpp>
#include <boost/compute/interop/qt.hpp>

#include <QList>
#include <QVector>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bcl = boost::compute;

BOOST_AUTO_TEST_CASE(qimage_format)
{
    BOOST_CHECK(
        bcl::qt_qimage_format_to_image_format(QImage::Format_RGB32) ==
        bcl::image_format(CL_BGRA, CL_UNORM_INT8)
    );
}

BOOST_AUTO_TEST_CASE(copy_qvector_to_device)
{
    QList<int> qvector;
    qvector.append(0);
    qvector.append(2);
    qvector.append(4);
    qvector.append(6);

    bcl::vector<int> vector(4, context);
    bcl::copy(qvector.begin(), qvector.end(), vector.begin(), queue);
    CHECK_RANGE_EQUAL(int, 4, vector, (0, 2, 4, 6));
}

BOOST_AUTO_TEST_CASE(copy_qlist_to_device)
{
    QList<int> list;
    list.append(1);
    list.append(3);
    list.append(5);
    list.append(7);

    bcl::vector<int> vector(4, context);
    bcl::copy(list.begin(), list.end(), vector.begin(), queue);
    CHECK_RANGE_EQUAL(int, 4, vector, (1, 3, 5, 7));
}

BOOST_AUTO_TEST_CASE(qvector_of_qpoint)
{
    QVector<QPoint> qt_points;
    qt_points.append(QPoint(0, 1));
    qt_points.append(QPoint(2, 3));
    qt_points.append(QPoint(4, 5));
    qt_points.append(QPoint(6, 7));

    bcl::vector<QPoint> bcl_points(qt_points.size(), context);
    bcl::copy(qt_points.begin(), qt_points.end(), bcl_points.begin(), queue);
}

BOOST_AUTO_TEST_CASE(qvector_of_qpointf)
{
    QVector<QPointF> qt_points;
    qt_points.append(QPointF(0.3f, 1.7f));
    qt_points.append(QPointF(2.3f, 3.7f));
    qt_points.append(QPointF(4.3f, 5.7f));
    qt_points.append(QPointF(6.3f, 7.7f));

    bcl::vector<QPointF> bcl_points(qt_points.size(), context);
    bcl::copy(qt_points.begin(), qt_points.end(), bcl_points.begin(), queue);
}

BOOST_AUTO_TEST_CASE(qvector_iterator)
{
    using boost::compute::detail::is_contiguous_iterator;

    BOOST_STATIC_ASSERT(is_contiguous_iterator<QVector<int>::iterator>::value == true);
    BOOST_STATIC_ASSERT(is_contiguous_iterator<QVector<int>::const_iterator>::value == true);
    BOOST_STATIC_ASSERT(is_contiguous_iterator<QList<int>::iterator>::value == false);
    BOOST_STATIC_ASSERT(is_contiguous_iterator<QList<int>::const_iterator>::value == false);
}

BOOST_AUTO_TEST_SUITE_END()
