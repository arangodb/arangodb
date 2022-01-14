//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_QVM_02C176D6B3AB11DE979F9A0D56D89593
#define BOOST_QVM_02C176D6B3AB11DE979F9A0D56D89593

#include <boost/qvm/vec_traits_defaults.hpp>
#include <boost/qvm/deduce_vec.hpp>
#include <boost/qvm/assert.hpp>
#include "test_qvm.hpp"

namespace
test_qvm
    {
    template <class Tag,int Dim,class T=float>
    struct
    vector
        {
        T a[Dim];
        mutable T b[Dim];

        explicit
        vector( T start=T(0), T step=T(0) )
            {
            for( int i=0; i!=Dim; ++i,start+=step )
                a[i]=b[i]=start;
            }
        };

    template <int Dim,class Tag1,class T1,class Tag2,class T2>
    void
    dump_ab( vector<Tag1,Dim,T1> const & a, vector<Tag2,Dim,T2> const & b )
        {
        detail::dump_ab(a.a,b.a);
        }
    }

namespace boost { namespace qvm {

        template <class Tag,int Dim,class T>
        struct
        vec_traits< test_qvm::vector<Tag,Dim,T> >:
            vec_traits_defaults<test_qvm::vector<Tag,Dim,T>,T,Dim>
            {
            typedef vec_traits_defaults<test_qvm::vector<Tag,Dim,T>,T,Dim>base;

            template <int I>
            static
            typename base::scalar_type &
            write_element( typename base::vec_type & m )
                {
                BOOST_QVM_STATIC_ASSERT(I>=0);
                BOOST_QVM_STATIC_ASSERT(I<Dim);
                return m.a[I];
                }

            using base::write_element_idx;
            };

        template <class Tag,class T,int D1,int D2,int Dim>
        struct
        deduce_vec2<test_qvm::vector<Tag,D1,T>,test_qvm::vector<Tag,D2,T>,Dim>
            {
            typedef test_qvm::vector<Tag,Dim,T> type;
            };
        }
    }

namespace
    {
    struct V1;
    struct V2;
    struct V3;
    }

#endif
