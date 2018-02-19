//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/deduce_vec.hpp>

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

template <class T,int D> struct v;

namespace
boost
    {
    namespace
    qvm
        {
        template <class T,int D>
        struct
        vec_traits< v<T,D> >
            {
            typedef T scalar_type;
            static int const dim=D;
            };                     
        }
    }

int
main()
    {
    same_type< boost::qvm::deduce_vec< v<int,3> >::type, v<int,3> >();
    same_type< boost::qvm::deduce_vec< v<int,3>, 4 >::type, boost::qvm::vec<int,4> >();
    check< v<int,3>, v<int,3>, 3, v<int,3> >();
    check< v<int,3>, v<float,3>, 4, boost::qvm::vec<float,4> >();
    }
