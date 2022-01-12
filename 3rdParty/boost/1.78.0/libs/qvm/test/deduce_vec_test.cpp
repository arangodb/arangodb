//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_QVM_TEST_SINGLE_HEADER
#   include BOOST_QVM_TEST_SINGLE_HEADER
#else
#   include <boost/qvm/deduce_vec.hpp>
#endif

template <class T,class U>
struct same_type;

template <class T>
struct
same_type<T,T>
    {
    };

template <class A,class B,int D,class Result>
struct
check
    {
    same_type<typename boost::qvm::deduce_vec2<A,B,D>::type,Result> a;
    same_type<typename boost::qvm::deduce_vec2<B,A,D>::type,Result> b;
    };

template <class T,int D> struct v1;
template <class T,int D> struct v2;

namespace
boost
    {
    namespace
    qvm
        {
        template <class T,int D>
        struct
        vec_traits< v1<T,D> >
            {
            typedef T scalar_type;
            static int const dim=D;
            };

        template <class T,int D>
        struct
        vec_traits< v2<T,D> >
            {
            typedef T scalar_type;
            static int const dim=D;
            };

        template <int D,class S,int V2D,class V2S>
        struct
        deduce_vec<v2<V2S,V2D>,D,S>
            {
            typedef v2<S,D> type;
            };

        template <int D,class S,class AS,int AD,class BS,int BD>
        struct
        deduce_vec2<v2<AS,AD>,v2<BS,BD>,D,S>
            {
            typedef v2<S,D> type;
            };
        }
    }

int
main()
    {
    same_type< boost::qvm::deduce_vec< v1<int,3> >::type, v1<int,3> >();
    same_type< boost::qvm::deduce_vec< v1<int,3>, 4 >::type, boost::qvm::vec<int,4> >();
    check< v1<int,3>, v1<int,3>, 3, v1<int,3> >();
    check< v1<int,3>, v1<float,3>, 4, boost::qvm::vec<float,4> >();

    same_type< boost::qvm::deduce_vec< v2<int,3> >::type, v2<int,3> >();
    same_type< boost::qvm::deduce_vec< v2<int,3>, 4 >::type, v2<int,4> >();
    check< v2<int,3>, v2<int,3>, 3, v2<int,3> >();
    check< v2<int,3>, v2<float,3>, 4, v2<float,4> >();
    }
