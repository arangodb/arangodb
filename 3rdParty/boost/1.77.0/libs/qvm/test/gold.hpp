/// Copyright (c) 2008-2021 Emil Dotchevski and Reverge Studios, Inc.

/// Distributed under the Boost Software License, Version 1.0. (See accompanying
/// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_QVM_907229FCB3A711DE83C152F855D89593
#define BOOST_QVM_907229FCB3A711DE83C152F855D89593

#include <limits>
#include <math.h>
#include <assert.h>
#include <memory.h>
#include <stdlib.h>

namespace
test_qvm
    {
    namespace
    detail
        {
        inline
        float
        sin( float a )
            {
            return ::sinf(a);
            }

        inline
        double
        sin( double a )
            {
            return ::sin(a);
            }

        inline
        float
        cos( float a )
            {
            return ::cosf(a);
            }

        inline
        double
        cos( double a )
            {
            return ::cos(a);
            }

        inline
        float
        abs( float a )
            {
            return ::fabsf(a);
            }

        inline
        double
        abs( double a )
            {
            return ::fabs(a);
            }

        inline
        float
        atan2( float a, float b )
            {
            return ::atan2f(a,b);
            }

        inline
        double
        atan2( double a, double b )
            {
            return ::atan2(a,b);
            }

        template <class T>
        T
        determinant( T * * a, int n )
            {
            int i,j,j1,j2;
            T det = 0;
            T * * m = 0;
            assert(n>=1);
            if( n==1 )
                det = a[0][0];
            else if( n==2 )
                det = a[0][0] * a[1][1] - a[1][0] * a[0][1];
            else
                {
                det = 0;
                for( j1=0; j1<n; j1++ )
                    {
                    m = static_cast<T * *>(malloc((n-1)*sizeof(T *)));
                    for( i=0; i<n-1; i++ )
                        m[i] = static_cast<T *>(malloc((n-1)*sizeof(T)));
                    for( i=1; i<n; i++ )
                        {
                        j2 = 0;
                        for( j=0; j<n; j++ )
                            {
                            if( j==j1 )
                                continue;
                            m[i-1][j2] = a[i][j];
                            j2++;
                            }
                        }
                    det += T(pow(-1.0,1.0+j1+1.0)) * a[0][j1] * determinant(m,n-1);
                    for( i=0; i<n-1; i++ )
                        free(m[i]);
                    free(m);
                    }
                }
            return(det);
            }

        template <class T,int N>
        void
        cofactor( T * * a, T (&b)[N][N] )
            {
            int i,j,ii,jj,i1,j1;
            T det;
            T * * c;
            c = static_cast<T * *>(malloc((N-1)*sizeof(T *)));
            for( i=0; i<N-1; i++ )
                c[i] = static_cast<T *>(malloc((N-1)*sizeof(T)));
            for( j=0; j<N; j++ )
                {
                for( i=0; i<N; i++ )
                    {
                    i1 = 0;
                    for( ii=0; ii<N; ii++ )
                        {
                        if( ii==i )
                            continue;
                        j1 = 0;
                        for( jj=0; jj<N; jj++ )
                            {
                            if( jj==j )
                                continue;
                            c[i1][j1] = a[ii][jj];
                            j1++;
                            }
                        i1++;
                        }
                    det = determinant(c,N-1);
                    b[i][j] = T(pow(-1.0,i+j+2.0)) * det;
                    }
                }
            for( i=0; i<N-1; i++ )
                free(c[i]);
            free(c);
            }
        }

    template <class T,int D>
    T
    determinant( T (&in)[D][D] )
        {
        T * * m = static_cast<T * *>(malloc(D*sizeof(T *)));
        for( int i=0; i!=D; ++i )
            {
            m[i] = static_cast<T *>(malloc(D*sizeof(T)));
            for( int j=0; j!=D; ++j )
                m[i][j]=in[i][j];
            }
        T det=::test_qvm::detail::determinant(m,D);
        for( int i=0; i<D; ++i )
            free(m[i]);
        free(m);
        return det;
        }

    template <class T,int D>
    void
    inverse( T (&out)[D][D], T (&in)[D][D] )
        {
        T * * m = static_cast<T * *>(malloc(D*sizeof(T *)));
        for( int i=0; i!=D; ++i )
            {
            m[i] = static_cast<T *>(malloc(D*sizeof(T)));
            for( int j=0; j!=D; ++j )
                m[i][j]=in[i][j];
            }
        T det=::test_qvm::detail::determinant(m,D);
        assert(det!=T(0));
        T f=T(1)/det;
        T b[D][D];
        ::test_qvm::detail::cofactor(m,b);
        for( int i=0; i<D; ++i )
            free(m[i]);
        free(m);
        for( int i=0; i!=D; ++i )
            for( int j=0; j!=D; ++j )
                out[j][i]=b[i][j]*f;
        }

    template <class T,int M,int N>
    void
    init_m( T (&r)[M][N], T start=T(0), T step=T(0) )
        {
        for( int i=0; i<M; ++i )
            for( int j=0; j<N; ++j,start+=step )
                r[i][j] = start;
        }

    template <class T,int D>
    void
    init_v( T (&r)[D], T start=T(0), T step=T(0) )
        {
        for( int i=0; i<D; ++i,start+=step )
            r[i] = start;
        }

    template <class T,int M,int N>
    void
    zero_mat( T (&r)[M][N] )
        {
        for( int i=0; i<M; ++i )
            for( int j=0; j<N; ++j )
                r[i][j] = T(0);
        }

    template <class T,int D>
    void
    zero_vec( T (&r)[D] )
        {
        for( int i=0; i<D; ++i )
            r[i] = T(0);
        }

    template <class T,int D>
    void
    identity( T (&r)[D][D] )
        {
        for( int i=0; i<D; ++i )
            for( int j=0; j<D; ++j )
                r[i][j] = (i==j) ? T(1) : T(0);
        }

    template <class T,class U,class V,int M,int N>
    void
    add_m( T (&r)[M][N], U (&a)[M][N], V (&b)[M][N] )
        {
        for( int i=0; i<M; ++i )
            for( int j=0; j<N; ++j )
                r[i][j] = a[i][j] + b[i][j];
        }

    template <class T,class U,class V,int D>
    void
    add_v( T (&r)[D], U (&a)[D], V (&b)[D] )
        {
        for( int i=0; i<D; ++i )
            r[i] = a[i] + b[i];
        }

    template <class T,class U,class V,int M,int N>
    void
    subtract_m( T (&r)[M][N], U (&a)[M][N], V (&b)[M][N] )
        {
        for( int i=0; i<M; ++i )
            for( int j=0; j<N; ++j )
                r[i][j] = a[i][j] - b[i][j];
        }

    template <class T,class U,class V,int D>
    void
    subtract_v( T (&r)[D], U (&a)[D], V (&b)[D] )
        {
        for( int i=0; i<D; ++i )
            r[i] = a[i] - b[i];
        }

    template <class T,int D,class U>
    void
    rotation_x( T (&r)[D][D], U angle )
        {
        identity(r);
        T c=::test_qvm::detail::cos(angle);
        T s=::test_qvm::detail::sin(angle);
        r[1][1]=c;
        r[1][2]=-s;
        r[2][1]=s;
        r[2][2]=c;
        }

    template <class T,int D,class U>
    void
    rotation_y( T (&r)[D][D], U angle )
        {
        identity(r);
        T c=::test_qvm::detail::cos(angle);
        T s=::test_qvm::detail::sin(angle);
        r[0][0]=c;
        r[0][2]=s;
        r[2][0]=-s;
        r[2][2]=c;
        }

    template <class T,int D,class U>
    void
    rotation_z( T (&r)[D][D], U angle )
        {
        identity(r);
        T c=::test_qvm::detail::cos(angle);
        T s=::test_qvm::detail::sin(angle);
        r[0][0]=c;
        r[0][1]=-s;
        r[1][0]=s;
        r[1][1]=c;
        }

    template <class T,int D>
    void
    translation( T (&r)[D][D], T (&t)[D-1] )
        {
        identity(r);
        for( int i=0; i!=D-1; ++i )
            r[i][D-1]=t[i];
        }

    template <class R,class T,class U,int M,int N,int P>
    void
    multiply_m( R (&r)[M][P], T (&a)[M][N], U (&b)[N][P] )
        {
        for( int i=0; i<M; ++i )
            for( int j=0; j<P; ++j )
                {
                R x=0;
                for( int k=0; k<N; ++k )
                    x += R(a[i][k])*R(b[k][j]);
                r[i][j] = x;
                }
        }

    template <class R,class T,class U,int M,int N>
    void
    multiply_mv( R (&r)[M], T (&a)[M][N], U (&b)[N] )
        {
        for( int i=0; i<M; ++i )
            {
            R x=0;
            for( int k=0; k<N; ++k )
                x += R(a[i][k])*R(b[k]);
            r[i] = x;
            }
        }

    template <class R,class T,class U,int N,int P>
    void
    multiply_vm( R (&r)[P], T (&a)[N], U (&b)[N][P] )
        {
        for( int j=0; j<P; ++j )
            {
            R x=0;
            for( int k=0; k<N; ++k )
                x += R(a[k])*R(b[k][j]);
            r[j] = x;
            }
        }

    template <class T,class U,int M,int N,class S>
    void
    scalar_multiply_m( T (&r)[M][N], U (&a)[M][N], S scalar )
        {
        for( int i=0; i<M; ++i )
            for( int j=0; j<N; ++j )
                r[i][j] = a[i][j]*scalar;
        }

    template <class T,class U,int D,class S>
    void
    scalar_multiply_v( T (&r)[D], U (&a)[D], S scalar )
        {
        for( int i=0; i<D; ++i )
            r[i] = a[i]*scalar;
        }

    template <class T,int M,int N>
    void
    transpose( T (&r)[M][N], T (&a)[N][M] )
        {
        for( int i=0; i<M; ++i )
            for( int j=0; j<N; ++j )
                r[i][j] = a[j][i];
        }

    template <class R,class T,class U,int D>
    R
    dot( T (&a)[D], U (&b)[D] )
        {
        R r=R(0);
        for( int i=0; i<D; ++i )
            r+=a[i]*b[i];
        return r;
        }

    template <class T,int M,int N>
    T
    norm_squared( T (&m)[M][N] )
        {
        T f=T(0);
        for( int i=0; i<M; ++i )
            for( int j=0; j<N; ++j )
                {
                T x=m[i][j];
                f+=x*x;
                }
        return f;
        }

    template <class T>
    inline
    void
    matrix_perspective_lh( T (&r)[4][4], T fov_y, T aspect_ratio, T zn, T zf )
        {
        T ys=T(1)/::tanf(fov_y/T(2));
        T xs=ys/aspect_ratio;
        zero_mat(r);
        r[0][0] = xs;
        r[1][1] = ys;
        r[2][2] = zf/(zf-zn);
        r[2][3] = -zn*zf/(zf-zn);
        r[3][2] = 1;
        }

    template <class T>
    inline
    void
    matrix_perspective_rh( T (&r)[4][4], T fov_y, T aspect_ratio, T zn, T zf )
        {
        matrix_perspective_lh(r,fov_y,aspect_ratio,zn,zf);
        r[2][2]=-r[2][2];
        r[3][2]=-r[3][2];

} }

#endif
