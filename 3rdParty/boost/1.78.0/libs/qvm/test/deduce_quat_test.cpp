//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_QVM_TEST_SINGLE_HEADER
#   include BOOST_QVM_TEST_SINGLE_HEADER
#else
#   include <boost/qvm/deduce_quat.hpp>
#endif

template <class T,class U>
struct same_type;

template <class T>
struct
same_type<T,T>
    {
    };

template <class A,class B,class Result>
struct
check
    {
    same_type<typename boost::qvm::deduce_quat2<A,B>::type,Result> a;
    same_type<typename boost::qvm::deduce_quat2<B,A>::type,Result> b;
    };

template <class T> struct q1;
template <class T> struct q2;

namespace
boost
    {
    namespace
    qvm
        {
        template <class T>
        struct
        quat_traits< q1<T> >
            {
            typedef T scalar_type;
            };

        template <class T>
        struct
        quat_traits< q2<T> >
            {
            typedef T scalar_type;
            };

        template <class S,class Q2S>
        struct
        deduce_quat<q2<Q2S>,S>
            {
            typedef q2<S> type;
            };

        template <class S,class AS,class BS>
        struct
        deduce_quat2<q2<AS>,q2<BS>,S>
            {
            typedef q2<S> type;
            };
        }
    }

int
main()
    {
    same_type< boost::qvm::deduce_quat< q1<int> >::type, q1<int> >();
    check< q1<int>, q1<int>, q1<int> >();

    same_type< boost::qvm::deduce_quat< q2<int> >::type, q2<int> >();
    same_type< boost::qvm::deduce_quat< q2<int> >::type, q2<int> >();
    check< q2<int>, q2<int>, q2<int> >();
    check< q2<int>, q2<float>, q2<float> >();
    }
