//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_QVM_TEST_SINGLE_HEADER
#   include BOOST_QVM_TEST_SINGLE_HEADER
#else
#   include <boost/qvm/mat_operations.hpp>
#   include <boost/qvm/mat.hpp>
#endif

#include "test_qvm_matrix.hpp"
#include "test_qvm_vector.hpp"
#include "gold.hpp"

namespace
    {
    template <int D>
    void
    test_x()
        {
        using namespace boost::qvm;
        test_qvm::vector<V1,3> axis; axis.a[0]=1;
        for( float r=0; r<6.28f; r+=0.5f )
            {
            test_qvm::matrix<M1,D,D> const m1=rot_mat<D>(axis,r);
            test_qvm::rotation_x(m1.b,r);
            BOOST_QVM_TEST_EQ(m1.a,m1.b);
            test_qvm::matrix<M1,D,D> m2(42,1);
            set_rot(m2,axis,r);
            test_qvm::rotation_x(m2.b,r);
            BOOST_QVM_TEST_EQ(m2.a,m2.b);
            test_qvm::matrix<M1,D,D> m3(42,1);
            test_qvm::matrix<M1,D,D> m4(42,1);
            rotate(m3,axis,r);
            m3 = m3*m1;
            BOOST_QVM_TEST_EQ(m3.a,m3.a);
            }
        }

    template <int D>
    void
    test_y()
        {
        using namespace boost::qvm;
        test_qvm::vector<V1,3> axis; axis.a[1]=1;
        for( float r=0; r<6.28f; r+=0.5f )
            {
            test_qvm::matrix<M1,D,D> m1=rot_mat<D>(axis,r);
            test_qvm::rotation_y(m1.b,r);
            BOOST_QVM_TEST_EQ(m1.a,m1.b);
            test_qvm::matrix<M1,D,D> m2(42,1);
            set_rot(m2,axis,r);
            test_qvm::rotation_y(m2.b,r);
            BOOST_QVM_TEST_EQ(m2.a,m2.b);
            test_qvm::matrix<M1,D,D> m3(42,1);
            test_qvm::matrix<M1,D,D> m4(42,1);
            rotate(m3,axis,r);
            m3 = m3*m1;
            BOOST_QVM_TEST_EQ(m3.a,m3.a);
            }
        }

    template <int D>
    void
    test_z()
        {
        using namespace boost::qvm;
        test_qvm::vector<V1,3> axis; axis.a[2]=1;
        for( float r=0; r<6.28f; r+=0.5f )
            {
            test_qvm::matrix<M1,D,D> m1=rot_mat<D>(axis,r);
            test_qvm::rotation_z(m1.b,r);
            BOOST_QVM_TEST_EQ(m1.a,m1.b);
            test_qvm::matrix<M1,D,D> m2(42,1);
            set_rot(m2,axis,r);
            test_qvm::rotation_z(m2.b,r);
            BOOST_QVM_TEST_EQ(m2.a,m2.b);
            test_qvm::matrix<M1,D,D> m3(42,1);
            test_qvm::matrix<M1,D,D> m4(42,1);
            rotate(m3,axis,r);
            m3 = m3*m1;
            BOOST_QVM_TEST_EQ(m3.a,m3.a);
            }
        }

    template <int D>
    void
    test_xzy()
        {
        using namespace boost::qvm;
        for( float x1=0; x1<6.28f; x1+=0.5f )
            for( float z2=0; z2<6.28f; z2+=0.5f )
                for( float y3=0; y3<6.28f; y3+=0.5f )
                {
                mat<float,D,D> const m2 = rotx_mat<D>(x1) * rotz_mat<D>(z2) * roty_mat<D>(y3);
                    {
                    mat<float,D,D> m1 = rot_mat_xzy<D>(x1,z2,y3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1; set_rot_xzy(m1,x1,z2,y3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1 = identity_mat<float,D>(); rotate_xzy(m1,x1,z2,y3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                }
        }

    template <int D>
    void
    test_xyz()
        {
        using namespace boost::qvm;
        for( float x1=0; x1<6.28f; x1+=0.5f )
            for( float y2=0; y2<6.28f; y2+=0.5f )
                for( float z3=0; z3<6.28f; z3+=0.5f )
                {
                mat<float,D,D> const m2 = rotx_mat<D>(x1) * roty_mat<D>(y2) * rotz_mat<D>(z3);
                    {
                    mat<float,D,D> m1 = rot_mat_xyz<D>(x1,y2,z3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1; set_rot_xyz(m1,x1,y2,z3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1 = identity_mat<float,D>(); rotate_xyz(m1,x1,y2,z3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                }
        }

    template <int D>
    void
    test_yxz()
        {
        using namespace boost::qvm;
        for( float y1=0; y1<6.28f; y1+=0.5f )
            for( float x2=0; x2<6.28f; x2+=0.5f )
                for( float z3=0; z3<6.28f; z3+=0.5f )
                {
                mat<float,D,D> const m2 = roty_mat<D>(y1) * rotx_mat<D>(x2) * rotz_mat<D>(z3);
                    {
                    mat<float,D,D> m1 = rot_mat_yxz<D>(y1,x2,z3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1; set_rot_yxz(m1,y1,x2,z3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1 = identity_mat<float,D>(); rotate_yxz(m1,y1,x2,z3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                }
        }

    template <int D>
    void
    test_yzx()
        {
        using namespace boost::qvm;
        for( float y1=0; y1<6.28f; y1+=0.5f )
            for( float z2=0; z2<6.28f; z2+=0.5f )
                for( float x3=0; x3<6.28f; x3+=0.5f )
                {
                mat<float,D,D> const m2 = roty_mat<D>(y1) * rotz_mat<D>(z2) * rotx_mat<D>(x3);
                    {
                    mat<float,D,D> m1 = rot_mat_yzx<D>(y1,z2,x3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1; set_rot_yzx(m1,y1,z2,x3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1 = identity_mat<float,D>(); rotate_yzx(m1,y1,z2,x3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                }
        }

    template <int D>
    void
    test_zyx()
        {
        using namespace boost::qvm;
        for( float z1=0; z1<6.28f; z1+=0.5f )
            for( float y2=0; y2<6.28f; y2+=0.5f )
                for( float x3=0; x3<6.28f; x3+=0.5f )
                {
                mat<float,D,D> const m2 = rotz_mat<D>(z1) * roty_mat<D>(y2) * rotx_mat<D>(x3);
                    {
                    mat<float,D,D> m1 = rot_mat_zyx<D>(z1,y2,x3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1; set_rot_zyx(m1,z1,y2,x3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1 = identity_mat<float,D>(); rotate_zyx(m1,z1,y2,x3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                }
        }

    template <int D>
    void
    test_zxy()
        {
        using namespace boost::qvm;
        for( float z1=0; z1<6.28f; z1+=0.5f )
            for( float x2=0; x2<6.28f; x2+=0.5f )
                for( float y3=0; y3<6.28f; y3+=0.5f )
                {
                mat<float,D,D> const m2 = rotz_mat<D>(z1) * rotx_mat<D>(x2) * roty_mat<D>(y3);
                    {
                    mat<float,D,D> m1 = rot_mat_zxy<D>(z1,x2,y3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1; set_rot_zxy(m1,z1,x2,y3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1 = identity_mat<float,D>(); rotate_zxy(m1,z1,x2,y3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                }
        }

    template <int D>
    void
    test_xzx()
        {
        using namespace boost::qvm;
        for( float x1=0; x1<6.28f; x1+=0.5f )
            for( float z2=0; z2<6.28f; z2+=0.5f )
                for( float x3=0; x3<6.28f; x3+=0.5f )
                {
                mat<float,D,D> const m2 = rotx_mat<D>(x1) * rotz_mat<D>(z2) * rotx_mat<D>(x3);
                    {
                    mat<float,D,D> m1 = rot_mat_xzx<D>(x1,z2,x3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1; set_rot_xzx(m1,x1,z2,x3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1 = identity_mat<float,D>(); rotate_xzx(m1,x1,z2,x3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                }
        }

    template <int D>
    void
    test_xyx()
        {
        using namespace boost::qvm;
        for( float x1=0; x1<6.28f; x1+=0.5f )
            for( float y2=0; y2<6.28f; y2+=0.5f )
                for( float x3=0; x3<6.28f; x3+=0.5f )
                {
                mat<float,D,D> const m2 = rotx_mat<D>(x1) * roty_mat<D>(y2) * rotx_mat<D>(x3);
                    {
                    mat<float,D,D> m1 = rot_mat_xyx<D>(x1,y2,x3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1; set_rot_xyx(m1,x1,y2,x3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1 = identity_mat<float,D>(); rotate_xyx(m1,x1,y2,x3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                }
        }

    template <int D>
    void
    test_yxy()
        {
        using namespace boost::qvm;
        for( float y1=0; y1<6.28f; y1+=0.5f )
            for( float x2=0; x2<6.28f; x2+=0.5f )
                for( float y3=0; y3<6.28f; y3+=0.5f )
                {
                mat<float,D,D> const m2 = roty_mat<D>(y1) * rotx_mat<D>(x2) * roty_mat<D>(y3);
                    {
                    mat<float,D,D> m1 = rot_mat_yxy<D>(y1,x2,y3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1; set_rot_yxy(m1,y1,x2,y3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1 = identity_mat<float,D>(); rotate_yxy(m1,y1,x2,y3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                }
        }

    template <int D>
    void
    test_yzy()
        {
        using namespace boost::qvm;
        for( float y1=0; y1<6.28f; y1+=0.5f )
            for( float z2=0; z2<6.28f; z2+=0.5f )
                for( float y3=0; y3<6.28f; y3+=0.5f )
                {
                mat<float,D,D> const m2 = roty_mat<D>(y1) * rotz_mat<D>(z2) * roty_mat<D>(y3);
                    {
                    mat<float,D,D> m1 = rot_mat_yzy<D>(y1,z2,y3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1; set_rot_yzy(m1,y1,z2,y3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1 = identity_mat<float,D>(); rotate_yzy(m1,y1,z2,y3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                }
        }

    template <int D>
    void
    test_zyz()
        {
        using namespace boost::qvm;
        for( float z1=0; z1<6.28f; z1+=0.5f )
            for( float y2=0; y2<6.28f; y2+=0.5f )
                for( float z3=0; z3<6.28f; z3+=0.5f )
                {
                mat<float,D,D> const m2 = rotz_mat<D>(z1) * roty_mat<D>(y2) * rotz_mat<D>(z3);
                    {
                    mat<float,D,D> m1 = rot_mat_zyz<D>(z1,y2,z3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1; set_rot_zyz(m1,z1,y2,z3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1 = identity_mat<float,D>(); rotate_zyz(m1,z1,y2,z3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                }
        }

    template <int D>
    void
    test_zxz()
        {
        using namespace boost::qvm;
        for( float z1=0; z1<6.28f; z1+=0.5f )
            for( float x2=0; x2<6.28f; x2+=0.5f )
                for( float z3=0; z3<6.28f; z3+=0.5f )
                {
                mat<float,D,D> const m2 = rotz_mat<D>(z1) * rotx_mat<D>(x2) * rotz_mat<D>(z3);
                    {
                    mat<float,D,D> m1 = rot_mat_zxz<D>(z1,x2,z3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1; set_rot_zxz(m1,z1,x2,z3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                    {
                    mat<float,D,D> m1 = identity_mat<float,D>(); rotate_zxz(m1,z1,x2,z3);
                    BOOST_QVM_TEST_CLOSE(m1.a,m2.a,0.0002f);
                    }
                }
        }
    }

int
main()
    {
    test_x<3>();
    test_y<3>();
    test_z<3>();
    test_xzy<3>();
    test_xyz<3>();
    test_yxz<3>();
    test_yzx<3>();
    test_zyx<3>();
    test_zxy<3>();
    test_xzx<3>();
    test_xyx<3>();
    test_yxy<3>();
    test_yzy<3>();
    test_zyz<3>();
    test_zxz<3>();
    test_x<4>();
    test_y<4>();
    test_z<4>();
    test_xzy<4>();
    test_xyz<4>();
    test_yxz<4>();
    test_yzx<4>();
    test_zyx<4>();
    test_zxy<4>();
    test_xzx<4>();
    test_xyx<4>();
    test_yxy<4>();
    test_yzy<4>();
    test_zyz<4>();
    test_zxz<4>();
    test_x<5>();
    test_y<5>();
    test_z<5>();
    test_xzy<5>();
    test_xyz<5>();
    test_yxz<5>();
    test_yzx<5>();
    test_zyx<5>();
    test_zxy<5>();
    test_xzx<5>();
    test_xyx<5>();
    test_yxy<5>();
    test_yzy<5>();
    test_zyz<5>();
    test_zxz<5>();
    return boost::report_errors();
    }
