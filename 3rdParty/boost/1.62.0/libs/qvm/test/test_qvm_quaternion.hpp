//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef UUID_EF9152E42E4711DFB699737156D89593
#define UUID_EF9152E42E4711DFB699737156D89593

#include <boost/qvm/deduce_quat.hpp>
#include <boost/qvm/assert.hpp>
#include "test_qvm.hpp"

namespace
test_qvm
    {
    template <class Tag,class T=float>
    struct
    quaternion
        {
        T a[4];
        mutable T b[4];

        explicit
        quaternion( T start=T(0), T step=T(0) )
            {
            for( int i=0; i!=4; ++i,start+=step )
                a[i]=b[i]=start;
            }
        };

    template <class Tag1,class T1,class Tag2,class T2>
    void
    dump_ab( quaternion<Tag1,T1> const & a, quaternion<Tag2,T2> const & b )
        {
        detail::dump_ab(a.a,b.a);
        }
    }

namespace
boost
    {
    namespace
    qvm
        {
        template <class Tag,class T>
        struct
        quat_traits< test_qvm::quaternion<Tag,T> >
            {
            typedef T scalar_type;
            typedef test_qvm::quaternion<Tag,T> this_quaternion_type;

            template <int I>
            static
            scalar_type &
            write_element( this_quaternion_type & m )
                {
                BOOST_QVM_STATIC_ASSERT(I>=0);
                BOOST_QVM_STATIC_ASSERT(I<4);
                return m.a[I];
                }

            template <int I>
            static
            scalar_type
            read_element( this_quaternion_type const & m )
                {
                BOOST_QVM_STATIC_ASSERT(I>=0);
                BOOST_QVM_STATIC_ASSERT(I<4);
                return m.a[I];
                }

            static
            inline
            scalar_type &
            write_element_idx( int i, this_quaternion_type & m )
                {
                BOOST_QVM_ASSERT(i>=0);
                BOOST_QVM_ASSERT(i<4);
                return m.a[i];
                }

            static
            inline
            scalar_type
            read_element_idx( int i, this_quaternion_type const & m )
                {
                BOOST_QVM_ASSERT(i>=0);
                BOOST_QVM_ASSERT(i<4);
                return m.a[i];
                }
            };

        template <class Tag,class T>
        struct
        deduce_quat2<test_qvm::quaternion<Tag,T>,test_qvm::quaternion<Tag,T> >
            {
            typedef test_qvm::quaternion<Tag,T> type;
            };
        }
    }

namespace
    {
    struct Q1;
    struct Q2;
    struct Q3;
    }

#endif
