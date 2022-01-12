// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2019 Tinko Bartels

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <geometry_test_common.hpp>

#include <boost/geometry/strategies/transform/matrix_transformers.hpp>
#include <boost/geometry/algorithms/transform.hpp>
#include <boost/geometry/geometries/point.hpp>

template <typename coordinate_type>
void test_all()
{
    typedef bg::model::point<coordinate_type, 2, bg::cs::cartesian> point_2d;
    typedef bg::model::point<coordinate_type, 3, bg::cs::cartesian> point_3d;
    typedef bg::model::point<coordinate_type, 4, bg::cs::cartesian> point_4d;

    point_2d p2d;
    point_3d p3d;
    point_4d p4d;

    bg::assign_values(p2d, 3, 5);

    boost::qvm::mat<coordinate_type, 5, 3> mat24;
    boost::qvm::A<0, 0>(mat24) =  1; boost::qvm::A<0, 1>(mat24) =  0; boost::qvm::A<0, 2>(mat24) = 0;
    boost::qvm::A<1, 0>(mat24) =  0; boost::qvm::A<1, 1>(mat24) =  1; boost::qvm::A<1, 2>(mat24) = 0;
    boost::qvm::A<2, 0>(mat24) =  1; boost::qvm::A<2, 1>(mat24) = -1; boost::qvm::A<2, 2>(mat24) = 0;
    boost::qvm::A<3, 0>(mat24) = -1; boost::qvm::A<3, 1>(mat24) =  1; boost::qvm::A<3, 2>(mat24) = 0;
    boost::qvm::A<4, 0>(mat24) =  0; boost::qvm::A<4, 1>(mat24) =  0; boost::qvm::A<4, 2>(mat24) = 1;
    bg::strategy::transform::matrix_transformer<coordinate_type, 2, 4> trans24(mat24);
    bg::transform(p2d, p4d, trans24);

    BOOST_CHECK_CLOSE(double(bg::get<0>(p4d)),  3.0, 0.001);
    BOOST_CHECK_CLOSE(double(bg::get<1>(p4d)),  5.0, 0.001);
    BOOST_CHECK_CLOSE(double(bg::get<2>(p4d)), -2.0, 0.001);
    BOOST_CHECK_CLOSE(double(bg::get<3>(p4d)),  2.0, 0.001);

    bg::strategy::transform::scale_transformer<coordinate_type, 4, 4> scale44(2);
    bg::transform(p4d, p4d, scale44);

    BOOST_CHECK_CLOSE(double(bg::get<0>(p4d)),  6.0, 0.001);
    BOOST_CHECK_CLOSE(double(bg::get<1>(p4d)), 10.0, 0.001);
    BOOST_CHECK_CLOSE(double(bg::get<2>(p4d)), -4.0, 0.001);
    BOOST_CHECK_CLOSE(double(bg::get<3>(p4d)),  4.0, 0.001);

    boost::qvm::mat<coordinate_type, 4, 5> mat43;
    boost::qvm::A<0, 0>(mat43) = 0  ; boost::qvm::A<0, 1>(mat43) = 0; boost::qvm::A<0, 2>(mat43) = 0.5; boost::qvm::A<0, 3>(mat43) = 0  ; boost::qvm::A<0, 4>(mat43) = 0;
    boost::qvm::A<1, 0>(mat43) = 0.5; boost::qvm::A<1, 1>(mat43) = 0; boost::qvm::A<1, 2>(mat43) = 0  ; boost::qvm::A<1, 3>(mat43) = 0  ; boost::qvm::A<1, 4>(mat43) = 0;
    boost::qvm::A<2, 0>(mat43) = 0  ; boost::qvm::A<2, 1>(mat43) = 0; boost::qvm::A<2, 2>(mat43) = 0  ; boost::qvm::A<2, 3>(mat43) = 0.5; boost::qvm::A<2, 4>(mat43) = 0;
    boost::qvm::A<3, 0>(mat43) = 0  ; boost::qvm::A<3, 1>(mat43) = 0; boost::qvm::A<3, 2>(mat43) = 0  ; boost::qvm::A<3, 3>(mat43) = 0  ; boost::qvm::A<3, 4>(mat43) = 1;
    bg::strategy::transform::matrix_transformer<coordinate_type, 4, 3> trans43(mat43);
    bg::transform(p4d, p3d, trans43);

    BOOST_CHECK_CLOSE(double(bg::get<0>(p3d)), -2.0, 0.001);
    BOOST_CHECK_CLOSE(double(bg::get<1>(p3d)),  3.0, 0.001);
    BOOST_CHECK_CLOSE(double(bg::get<2>(p3d)),  2.0, 0.001);

    bg::strategy::transform::matrix_transformer<coordinate_type, 3, 3> trans33(1, 0, 0, 0,
        0, 0, 1, 0,
        0, 1, 0, 0,
        0, 0, 0, 1);
    bg::transform(p3d, p3d, trans33);

    BOOST_CHECK_CLOSE(double(bg::get<0>(p3d)), -2.0, 0.001);
    BOOST_CHECK_CLOSE(double(bg::get<1>(p3d)),  2.0, 0.001);
    BOOST_CHECK_CLOSE(double(bg::get<2>(p3d)),  3.0, 0.001);

    boost::qvm::mat<coordinate_type, 3, 4> mat32;
    boost::qvm::A<0, 0>(mat32) =  1; boost::qvm::A<0, 1>(mat32) = 1; boost::qvm::A<0, 2>(mat32) = 1; boost::qvm::A<0, 3>(mat32) = 0;
    boost::qvm::A<1, 0>(mat32) = -1; boost::qvm::A<1, 1>(mat32) = 0; boost::qvm::A<1, 2>(mat32) = 1; boost::qvm::A<1, 3>(mat32) = 0;
    boost::qvm::A<2, 0>(mat32) =  0; boost::qvm::A<2, 1>(mat32) = 0; boost::qvm::A<2, 2>(mat32) = 0; boost::qvm::A<2, 3>(mat32) = 1;

    bg::strategy::transform::matrix_transformer<coordinate_type, 3, 2> trans32(mat32);
    bg::transform(p3d, p2d, trans32);

    BOOST_CHECK_CLOSE(double(bg::get<0>(p2d)), 3.0, 0.001);
    BOOST_CHECK_CLOSE(double(bg::get<1>(p2d)), 5.0, 0.001);

    bg::strategy::transform::matrix_transformer<coordinate_type, 2, 2>
        trans_composed(
            trans32.matrix() * trans33.matrix() * trans43.matrix() * scale44.matrix() * trans24.matrix());
    bg::transform(p2d, p2d, trans_composed);

    BOOST_CHECK_CLOSE(double(bg::get<0>(p2d)), 3.0, 0.001);
    BOOST_CHECK_CLOSE(double(bg::get<1>(p2d)), 5.0, 0.001);
}

int test_main(int, char* [])
{
    test_all<float>();
    test_all<double>();

    return 0;
}
