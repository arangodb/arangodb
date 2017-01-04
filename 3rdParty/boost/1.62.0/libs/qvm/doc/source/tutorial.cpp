//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/operations.hpp>
#include <boost/qvm/map_mat_mat.hpp>
#include <boost/qvm/map_mat_vec.hpp>
#include <boost/qvm/map_vec_mat.hpp>
#include <boost/qvm/vec_access.hpp>
#include <boost/qvm/vec_traits_array.hpp>
#include <boost/qvm/quat.hpp>
#include <boost/qvm/vec.hpp>
#include <boost/qvm/mat.hpp>
#include <boost/qvm/swizzle.hpp>
using namespace boost::qvm;

struct float3 { float a[3]; };
struct float4 { float a[4]; };
struct float33 { float a[3][3]; };
struct fquat { float a[4]; };

namespace
boost
    {
    namespace
    qvm
        {
        template <>
        struct
        vec_traits<float3>
            {
            static int const dim=3;
            typedef float scalar_type;

            template <int I> static inline scalar_type & write_element( float3 & v ) { return v.a[I]; }
            template <int I> static inline scalar_type read_element( float3 const & v ) { return v.a[I]; }

            static inline scalar_type & write_element_idx( int i, float3 & v ) { return v.a[i]; }
            static inline scalar_type read_element_idx( int i, float3 const & v ) { return v.a[i]; }
            };

        template <>
        struct
        vec_traits<float4>
            {
            static int const dim=4;
            typedef float scalar_type;

            template <int I> static inline scalar_type & write_element( float4 & v ) { return v.a[I]; }
            template <int I> static inline scalar_type read_element( float4 const & v ) { return v.a[I]; }

            static inline scalar_type & write_element_idx( int i, float4 & v ) { return v.a[i]; }
            static inline scalar_type read_element_idx( int i, float4 const & v ) { return v.a[i]; }
            };

        template <>
        struct
        mat_traits<float33>
            {
            typedef float scalar_type;
            static int const rows=3;
            static int const cols=3;

            template <int R,int C>
            static inline scalar_type & write_element( float33 & m ) { return m.a[R][C]; }

            template <int R,int C>
            static inline scalar_type read_element( float33 const & m ) { return m.a[R][C]; }

            static inline scalar_type & write_element_idx( int r, int c, float33 & m ) { return m.a[r][c]; }
            static inline scalar_type read_element_idx( int r, int c, float33 const & m ) { return m.a[r][c]; }
            };

        template <>
        struct
        quat_traits<fquat>
            {
            typedef float scalar_type;

            template <int I> static inline scalar_type & write_element( float4 & v ) { return v.a[I]; }
            template <int I> static inline scalar_type read_element( float4 const & v ) { return v.a[I]; }

            static inline scalar_type & write_element_idx( int i, float4 & v ) { return v.a[i]; }
            static inline scalar_type read_element_idx( int i, float4 const & v ) { return v.a[i]; }
            };
        }
    }

void
f()
    {
        {
        quat<float> rx=rotx_quat(3.14159f);
        }

        {
        vec<float,3> v={0,1,0};
        mat<float,4,4> tr=translation_mat(v);
        }

        {
        float3 v;
        X(v) = 0;
        Y(v) = 0;
        Z(v) = 7;
        float vmag = mag(v);
        float33 m = rotx_mat<3>(3.14159f);
        float3 vrot = m * v;
        }

        {
        float v[3] = {0,0,7};
        float3 vrot = rotx_mat<3>(3.14159f) * v;
        }

        {
        float3 v = {0,0,7};
        float3 vrot = transposed(rotx_mat<3>(3.14159f)) * v;
        }

        {
        float3 v = {0,0,7};
        YXZ(v) = rotx_mat<3>(3.14159f) * v;
        }

        {
        float3 v = {0,0,7};
        float4 point = XYZ1(v); //{0,0,7,1}
        float4 vector = XYZ0(v); //{0,0,7,0}
        }

        {
        float3 v = {0,0,7};
        float4 v1 = ZZZZ(v); //{7,7,7,7}
        }

        {
        float v[3] = {0,0,7};
        vref(v) *= 42;
        }
    }

void
multiply_column1( float33 & m, float scalar )
    {
    col<1>(m) *= scalar;
    }
