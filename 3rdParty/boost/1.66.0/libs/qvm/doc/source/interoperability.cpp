//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/mat_operations.hpp>
#include <boost/qvm/mat.hpp>
using namespace boost::qvm;

namespace
n1
    {
    struct user_matrix1 { float a[3][3]; };
    struct user_matrix2 { float a[3][3]; };
    }

namespace
n2
    {
    struct user_matrix1 { float a[3][3]; };
    struct user_matrix2 { float a[3][3]; };
    }

namespace
boost
    {
    namespace
    qvm
        {
        template <>
        struct
        mat_traits<n1::user_matrix1>
            {
            typedef float scalar_type;
            static int const rows=3;
            static int const cols=3;

            template <int R,int C>
            static inline scalar_type & write_element( n1::user_matrix1 & m ) { return m.a[R][C]; }

            template <int R,int C>
            static inline scalar_type read_element( n1::user_matrix1 const & m ) { return m.a[R][C]; }

            static inline scalar_type & write_element_idx( int r, int c, n1::user_matrix1 & m ) { return m.a[r][c]; }
            static inline scalar_type read_element_idx( int r, int c, n1::user_matrix1 const & m ) { return m.a[r][c]; }
            };

        template <>
        struct
        mat_traits<n1::user_matrix2>
            {
            typedef float scalar_type;
            static int const rows=3;
            static int const cols=3;

            template <int R,int C>
            static inline scalar_type & write_element( n1::user_matrix2 & m ) { return m.a[R][C]; }

            template <int R,int C>
            static inline scalar_type read_element( n1::user_matrix2 const & m ) { return m.a[R][C]; }

            static inline scalar_type & write_element_idx( int r, int c, n1::user_matrix2 & m ) { return m.a[r][c]; }
            static inline scalar_type read_element_idx( int r, int c, n1::user_matrix2 const & m ) { return m.a[r][c]; }
            };

        template <>
        struct
        mat_traits<n2::user_matrix1>
            {
            typedef float scalar_type;
            static int const rows=3;
            static int const cols=3;

            template <int R,int C>
            static inline scalar_type & write_element( n2::user_matrix1 & m ) { return m.a[R][C]; }

            template <int R,int C>
            static inline scalar_type read_element( n2::user_matrix1 const & m ) { return m.a[R][C]; }

            static inline scalar_type & write_element_idx( int r, int c, n2::user_matrix1 & m ) { return m.a[r][c]; }
            static inline scalar_type read_element_idx( int r, int c, n2::user_matrix1 const & m ) { return m.a[r][c]; }
            };

        template <>
        struct
        mat_traits<n2::user_matrix2>
            {
            typedef float scalar_type;
            static int const rows=3;
            static int const cols=3;

            template <int R,int C>
            static inline scalar_type & write_element( n2::user_matrix2 & m ) { return m.a[R][C]; }

            template <int R,int C>
            static inline scalar_type read_element( n2::user_matrix2 const & m ) { return m.a[R][C]; }

            static inline scalar_type & write_element_idx( int r, int c, n2::user_matrix2 & m ) { return m.a[r][c]; }
            static inline scalar_type read_element_idx( int r, int c, n2::user_matrix2 const & m ) { return m.a[r][c]; }
            };

        template <>
        struct
        deduce_mat2<n2::user_matrix1,n2::user_matrix2,3,3>
            { typedef n2::user_matrix1 type; };

        template <>
        struct
        deduce_mat2<n2::user_matrix2,n2::user_matrix1,3,3>
            { typedef n2::user_matrix1 type; };
        }
    }

void
f()
    {
        {
        n1::user_matrix1 m1;
        n1::user_matrix2 m2;
        n1::user_matrix1 m = m1 * m2;
        }
        {
        n2::user_matrix1 m1;
        n2::user_matrix2 m2;
        n2::user_matrix1 m = m1 * m2;
        }
        {
        float m[3][3];
        (void) inverse(m);
        }
    }
