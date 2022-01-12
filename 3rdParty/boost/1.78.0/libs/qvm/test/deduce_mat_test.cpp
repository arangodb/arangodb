//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_QVM_TEST_SINGLE_HEADER
#   include BOOST_QVM_TEST_SINGLE_HEADER
#else
#   include <boost/qvm/deduce_mat.hpp>
#endif

template <class T,class U>
struct same_type;

template <class T>
struct
same_type<T,T>
    {
    };

template <class A,class B,int R,int C,class Result>
struct
check
    {
    same_type<typename boost::qvm::deduce_mat2<A,B,R,C>::type,Result> a;
    same_type<typename boost::qvm::deduce_mat2<B,A,R,C>::type,Result> b;
    };

template <class T,int R,int C> struct m1;
template <class T,int R,int C> struct m2;

namespace
boost
    {
    namespace
    qvm
        {
        template <class T,int R,int C>
        struct
        mat_traits< m1<T,R,C> >
            {
            typedef T scalar_type;
            static int const rows=R;
            static int const cols=C;
            };

        template <class T,int R,int C>
        struct
        mat_traits< m2<T,R,C> >
            {
            typedef T scalar_type;
            static int const rows=R;
            static int const cols=C;
            };

        template <int R,int C,class S,int M2R,int M2C,class M2S>
        struct
        deduce_mat<m2<M2S,M2R,M2C>,R,C,S>
            {
            typedef m2<S,R,C> type;
            };

        template <int R,int C,class S,class AS,int AR,int AC,class BS,int BR,int BC>
        struct
        deduce_mat2<m2<AS,AR,AC>,m2<BS,BR,BC>,R,C,S>
            {
            typedef m2<S,R,C> type;
            };
        }
    }

int
main()
    {
    same_type< boost::qvm::deduce_mat< m1<int,4,2> >::type, m1<int,4,2> >();
    same_type< boost::qvm::deduce_mat< m1<int,4,2>, 4, 4 >::type, boost::qvm::mat<int,4,4> >();
    check< m1<int,4,2>, m1<int,4,2>, 4, 2, m1<int,4,2> >();
    check< m1<int,4,2>, m1<float,4,2>, 4, 4, boost::qvm::mat<float,4,4> >();

    same_type< boost::qvm::deduce_mat< m2<int,4,2> >::type, m2<int,4,2> >();
    same_type< boost::qvm::deduce_mat< m2<int,4,2>, 4, 4 >::type, m2<int,4,4> >();
    check< m2<int,4,2>, m2<int,4,2>, 4, 2, m2<int,4,2> >();
    check< m2<int,4,2>, m2<float,4,2>, 4, 4, m2<float,4,4> >();
    }
