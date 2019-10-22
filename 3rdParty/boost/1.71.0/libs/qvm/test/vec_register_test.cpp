//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.
//Copyright (c) 2018 agate-pris

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/qvm/vec_register.hpp>

struct v2
{
    float x;
    float y;
};

struct v3 : v2
{
    float z;
};

struct v4 : v3
{
    float w;
};

struct v2r
{
    float xr;
    float yr;
};

struct v3r : v2r
{
    float zr;
};

struct v4r : v3r
{
    float wr;
};

struct v2rw : v2, v2r { };
struct v3rw : v3, v3r { };
struct v4rw : v4, v4r { };

BOOST_QVM_REGISTER_VEC_2(v2, float, x, y)
BOOST_QVM_REGISTER_VEC_3(v3, float, x, y, z)
BOOST_QVM_REGISTER_VEC_4(v4, float, x, y, z, w)

BOOST_QVM_REGISTER_VEC_2_READ(v2r, float, xr, yr)
BOOST_QVM_REGISTER_VEC_3_READ(v3r, float, xr, yr, zr)
BOOST_QVM_REGISTER_VEC_4_READ(v4r, float, xr, yr, zr, wr)

BOOST_QVM_REGISTER_VEC_2_READ_WRITE(v2rw, float, xr, yr, x, y)
BOOST_QVM_REGISTER_VEC_3_READ_WRITE(v3rw, float, xr, yr, zr, x, y, z)
BOOST_QVM_REGISTER_VEC_4_READ_WRITE(v4rw, float, xr, yr, zr, wr, x, y, z, w)

int main()
{
    using namespace boost::qvm;

    v2  v_v2;
    v3  v_v3;
    v4  v_v4;
    v2r v_v2r;
    v3r v_v3r;
    v4r v_v4r;
    v2rw v_v2rw;
    v3rw v_v3rw;
    v4rw v_v4rw;

    v_v2.x = 41.f;
    v_v3.x = 42.f;
    v_v4.x = 43.f;
    v_v2.y = 44.f;
    v_v3.y = 45.f;
    v_v4.y = 46.f;
    v_v3.z = 47.f;
    v_v4.z = 48.f;
    v_v4.w = 49.f;

    v_v2r.xr = 51.f;
    v_v3r.xr = 52.f;
    v_v4r.xr = 53.f;
    v_v2r.yr = 54.f;
    v_v3r.yr = 55.f;
    v_v4r.yr = 56.f;
    v_v3r.zr = 57.f;
    v_v4r.zr = 58.f;
    v_v4r.wr = 59.f;

    v_v2rw.x = 61.f;
    v_v3rw.x = 62.f;
    v_v4rw.x = 63.f;
    v_v2rw.y = 64.f;
    v_v3rw.y = 65.f;
    v_v4rw.y = 66.f;
    v_v3rw.z = 67.f;
    v_v4rw.z = 68.f;
    v_v4rw.w = 69.f;

    v_v2rw.xr = 71.f;
    v_v3rw.xr = 72.f;
    v_v4rw.xr = 73.f;
    v_v2rw.yr = 74.f;
    v_v3rw.yr = 75.f;
    v_v4rw.yr = 76.f;
    v_v3rw.zr = 77.f;
    v_v4rw.zr = 78.f;
    v_v4rw.wr = 79.f;

    BOOST_TEST(vec_traits<v2  >::read_element<0>(v_v2  ) == v_v2  .x );
    BOOST_TEST(vec_traits<v3  >::read_element<0>(v_v3  ) == v_v3  .x );
    BOOST_TEST(vec_traits<v4  >::read_element<0>(v_v4  ) == v_v4  .x );
    BOOST_TEST(vec_traits<v2r >::read_element<0>(v_v2r ) == v_v2r .xr);
    BOOST_TEST(vec_traits<v3r >::read_element<0>(v_v3r ) == v_v3r .xr);
    BOOST_TEST(vec_traits<v4r >::read_element<0>(v_v4r ) == v_v4r .xr);
    BOOST_TEST(vec_traits<v2rw>::read_element<0>(v_v2rw) == v_v2rw.xr);
    BOOST_TEST(vec_traits<v3rw>::read_element<0>(v_v3rw) == v_v3rw.xr);
    BOOST_TEST(vec_traits<v4rw>::read_element<0>(v_v4rw) == v_v4rw.xr);
    BOOST_TEST(vec_traits<v2  >::read_element<1>(v_v2  ) == v_v2  .y );
    BOOST_TEST(vec_traits<v3  >::read_element<1>(v_v3  ) == v_v3  .y );
    BOOST_TEST(vec_traits<v4  >::read_element<1>(v_v4  ) == v_v4  .y );
    BOOST_TEST(vec_traits<v2r >::read_element<1>(v_v2r ) == v_v2r .yr);
    BOOST_TEST(vec_traits<v3r >::read_element<1>(v_v3r ) == v_v3r .yr);
    BOOST_TEST(vec_traits<v4r >::read_element<1>(v_v4r ) == v_v4r .yr);
    BOOST_TEST(vec_traits<v2rw>::read_element<1>(v_v2rw) == v_v2rw.yr);
    BOOST_TEST(vec_traits<v3rw>::read_element<1>(v_v3rw) == v_v3rw.yr);
    BOOST_TEST(vec_traits<v4rw>::read_element<1>(v_v4rw) == v_v4rw.yr);
    BOOST_TEST(vec_traits<v3  >::read_element<2>(v_v3  ) == v_v3  .z );
    BOOST_TEST(vec_traits<v4  >::read_element<2>(v_v4  ) == v_v4  .z );
    BOOST_TEST(vec_traits<v3r >::read_element<2>(v_v3r ) == v_v3r .zr);
    BOOST_TEST(vec_traits<v4r >::read_element<2>(v_v4r ) == v_v4r .zr);
    BOOST_TEST(vec_traits<v3rw>::read_element<2>(v_v3rw) == v_v3rw.zr);
    BOOST_TEST(vec_traits<v4rw>::read_element<2>(v_v4rw) == v_v4rw.zr);
    BOOST_TEST(vec_traits<v4  >::read_element<3>(v_v4  ) == v_v4  .w );
    BOOST_TEST(vec_traits<v4r >::read_element<3>(v_v4r ) == v_v4r .wr);
    BOOST_TEST(vec_traits<v4rw>::read_element<3>(v_v4rw) == v_v4rw.wr);

    BOOST_TEST(vec_traits<v2  >::read_element_idx(0, v_v2  ) == v_v2  .x );
    BOOST_TEST(vec_traits<v3  >::read_element_idx(0, v_v3  ) == v_v3  .x );
    BOOST_TEST(vec_traits<v4  >::read_element_idx(0, v_v4  ) == v_v4  .x );
    BOOST_TEST(vec_traits<v2r >::read_element_idx(0, v_v2r ) == v_v2r .xr);
    BOOST_TEST(vec_traits<v3r >::read_element_idx(0, v_v3r ) == v_v3r .xr);
    BOOST_TEST(vec_traits<v4r >::read_element_idx(0, v_v4r ) == v_v4r .xr);
    BOOST_TEST(vec_traits<v2rw>::read_element_idx(0, v_v2rw) == v_v2rw.xr);
    BOOST_TEST(vec_traits<v3rw>::read_element_idx(0, v_v3rw) == v_v3rw.xr);
    BOOST_TEST(vec_traits<v4rw>::read_element_idx(0, v_v4rw) == v_v4rw.xr);
    BOOST_TEST(vec_traits<v2  >::read_element_idx(1, v_v2  ) == v_v2  .y );
    BOOST_TEST(vec_traits<v3  >::read_element_idx(1, v_v3  ) == v_v3  .y );
    BOOST_TEST(vec_traits<v4  >::read_element_idx(1, v_v4  ) == v_v4  .y );
    BOOST_TEST(vec_traits<v2r >::read_element_idx(1, v_v2r ) == v_v2r .yr);
    BOOST_TEST(vec_traits<v3r >::read_element_idx(1, v_v3r ) == v_v3r .yr);
    BOOST_TEST(vec_traits<v4r >::read_element_idx(1, v_v4r ) == v_v4r .yr);
    BOOST_TEST(vec_traits<v2rw>::read_element_idx(1, v_v2rw) == v_v2rw.yr);
    BOOST_TEST(vec_traits<v3rw>::read_element_idx(1, v_v3rw) == v_v3rw.yr);
    BOOST_TEST(vec_traits<v4rw>::read_element_idx(1, v_v4rw) == v_v4rw.yr);
    BOOST_TEST(vec_traits<v3  >::read_element_idx(2, v_v3  ) == v_v3  .z );
    BOOST_TEST(vec_traits<v4  >::read_element_idx(2, v_v4  ) == v_v4  .z );
    BOOST_TEST(vec_traits<v3r >::read_element_idx(2, v_v3r ) == v_v3r .zr);
    BOOST_TEST(vec_traits<v4r >::read_element_idx(2, v_v4r ) == v_v4r .zr);
    BOOST_TEST(vec_traits<v3rw>::read_element_idx(2, v_v3rw) == v_v3rw.zr);
    BOOST_TEST(vec_traits<v4rw>::read_element_idx(2, v_v4rw) == v_v4rw.zr);
    BOOST_TEST(vec_traits<v4  >::read_element_idx(3, v_v4  ) == v_v4  .w );
    BOOST_TEST(vec_traits<v4r >::read_element_idx(3, v_v4r ) == v_v4r .wr);
    BOOST_TEST(vec_traits<v4rw>::read_element_idx(3, v_v4rw) == v_v4rw.wr);

    BOOST_TEST(&vec_traits<v2  >::write_element<0>(v_v2  ) == &v_v2  .x );
    BOOST_TEST(&vec_traits<v3  >::write_element<0>(v_v3  ) == &v_v3  .x );
    BOOST_TEST(&vec_traits<v4  >::write_element<0>(v_v4  ) == &v_v4  .x );
    BOOST_TEST_NOT(&vec_traits<v2rw>::write_element<0>(v_v2rw) == &v_v2rw.xr);
    BOOST_TEST_NOT(&vec_traits<v3rw>::write_element<0>(v_v3rw) == &v_v3rw.xr);
    BOOST_TEST_NOT(&vec_traits<v4rw>::write_element<0>(v_v4rw) == &v_v4rw.xr);
    BOOST_TEST(&vec_traits<v2rw>::write_element<0>(v_v2rw) == &v_v2rw.x );
    BOOST_TEST(&vec_traits<v3rw>::write_element<0>(v_v3rw) == &v_v3rw.x );
    BOOST_TEST(&vec_traits<v4rw>::write_element<0>(v_v4rw) == &v_v4rw.x );
    BOOST_TEST(&vec_traits<v2  >::write_element<1>(v_v2  ) == &v_v2  .y );
    BOOST_TEST(&vec_traits<v3  >::write_element<1>(v_v3  ) == &v_v3  .y );
    BOOST_TEST(&vec_traits<v4  >::write_element<1>(v_v4  ) == &v_v4  .y );
    BOOST_TEST_NOT(&vec_traits<v2rw>::write_element<1>(v_v2rw) == &v_v2rw.yr);
    BOOST_TEST_NOT(&vec_traits<v3rw>::write_element<1>(v_v3rw) == &v_v3rw.yr);
    BOOST_TEST_NOT(&vec_traits<v4rw>::write_element<1>(v_v4rw) == &v_v4rw.yr);
    BOOST_TEST(&vec_traits<v2rw>::write_element<1>(v_v2rw) == &v_v2rw.y );
    BOOST_TEST(&vec_traits<v3rw>::write_element<1>(v_v3rw) == &v_v3rw.y );
    BOOST_TEST(&vec_traits<v4rw>::write_element<1>(v_v4rw) == &v_v4rw.y );
    BOOST_TEST(&vec_traits<v3  >::write_element<2>(v_v3  ) == &v_v3  .z );
    BOOST_TEST(&vec_traits<v4  >::write_element<2>(v_v4  ) == &v_v4  .z );
    BOOST_TEST_NOT(&vec_traits<v3rw>::write_element<2>(v_v3rw) == &v_v3rw.zr);
    BOOST_TEST_NOT(&vec_traits<v4rw>::write_element<2>(v_v4rw) == &v_v4rw.zr);
    BOOST_TEST(&vec_traits<v3rw>::write_element<2>(v_v3rw) == &v_v3rw.z );
    BOOST_TEST(&vec_traits<v4rw>::write_element<2>(v_v4rw) == &v_v4rw.z );
    BOOST_TEST(&vec_traits<v4  >::write_element<3>(v_v4  ) == &v_v4  .w );
    BOOST_TEST_NOT(&vec_traits<v4rw>::write_element<3>(v_v4rw) == &v_v4rw.wr);
    BOOST_TEST(&vec_traits<v4rw>::write_element<3>(v_v4rw) == &v_v4rw.w );

    BOOST_TEST(&vec_traits<v2  >::write_element_idx(0, v_v2  ) == &v_v2  .x );
    BOOST_TEST(&vec_traits<v3  >::write_element_idx(0, v_v3  ) == &v_v3  .x );
    BOOST_TEST(&vec_traits<v4  >::write_element_idx(0, v_v4  ) == &v_v4  .x );
    BOOST_TEST_NOT(&vec_traits<v2rw>::write_element_idx(0, v_v2rw) == &v_v2rw.xr);
    BOOST_TEST_NOT(&vec_traits<v3rw>::write_element_idx(0, v_v3rw) == &v_v3rw.xr);
    BOOST_TEST_NOT(&vec_traits<v4rw>::write_element_idx(0, v_v4rw) == &v_v4rw.xr);
    BOOST_TEST(&vec_traits<v2rw>::write_element_idx(0, v_v2rw) == &v_v2rw.x );
    BOOST_TEST(&vec_traits<v3rw>::write_element_idx(0, v_v3rw) == &v_v3rw.x );
    BOOST_TEST(&vec_traits<v4rw>::write_element_idx(0, v_v4rw) == &v_v4rw.x );
    BOOST_TEST(&vec_traits<v2  >::write_element_idx(1, v_v2  ) == &v_v2  .y );
    BOOST_TEST(&vec_traits<v3  >::write_element_idx(1, v_v3  ) == &v_v3  .y );
    BOOST_TEST(&vec_traits<v4  >::write_element_idx(1, v_v4  ) == &v_v4  .y );
    BOOST_TEST_NOT(&vec_traits<v2rw>::write_element_idx(1, v_v2rw) == &v_v2rw.yr);
    BOOST_TEST_NOT(&vec_traits<v3rw>::write_element_idx(1, v_v3rw) == &v_v3rw.yr);
    BOOST_TEST_NOT(&vec_traits<v4rw>::write_element_idx(1, v_v4rw) == &v_v4rw.yr);
    BOOST_TEST(&vec_traits<v2rw>::write_element_idx(1, v_v2rw) == &v_v2rw.y );
    BOOST_TEST(&vec_traits<v3rw>::write_element_idx(1, v_v3rw) == &v_v3rw.y );
    BOOST_TEST(&vec_traits<v4rw>::write_element_idx(1, v_v4rw) == &v_v4rw.y );
    BOOST_TEST(&vec_traits<v3  >::write_element_idx(2, v_v3  ) == &v_v3  .z );
    BOOST_TEST(&vec_traits<v4  >::write_element_idx(2, v_v4  ) == &v_v4  .z );
    BOOST_TEST_NOT(&vec_traits<v3rw>::write_element_idx(2, v_v3rw) == &v_v3rw.zr);
    BOOST_TEST_NOT(&vec_traits<v4rw>::write_element_idx(2, v_v4rw) == &v_v4rw.zr);
    BOOST_TEST(&vec_traits<v3rw>::write_element_idx(2, v_v3rw) == &v_v3rw.z );
    BOOST_TEST(&vec_traits<v4rw>::write_element_idx(2, v_v4rw) == &v_v4rw.z );
    BOOST_TEST(&vec_traits<v4  >::write_element_idx(3, v_v4  ) == &v_v4  .w );
    BOOST_TEST_NOT(&vec_traits<v4rw>::write_element_idx(3, v_v4rw) == &v_v4rw.wr);
    BOOST_TEST(&vec_traits<v4rw>::write_element_idx(3, v_v4rw) == &v_v4rw.w );

    return boost::report_errors();
}
