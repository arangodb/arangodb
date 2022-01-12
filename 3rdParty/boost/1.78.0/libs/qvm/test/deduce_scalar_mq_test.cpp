//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_QVM_TEST_SINGLE_HEADER
#   include BOOST_QVM_TEST_SINGLE_HEADER
#else
#   include <boost/qvm/deduce_scalar.hpp>
#   include <boost/qvm/quat.hpp>
#   include <boost/qvm/quat_operations.hpp>
#   include <boost/qvm/vec.hpp>
#   include <boost/qvm/quat_vec_operations.hpp>
#endif

#include <boost/core/lightweight_test.hpp>

template <class T>
struct
wrap
    {
    T t;
    wrap() { }
    wrap(T t):t(t) { }
    };

template <class S, class T>
wrap<T>
operator*(S s, wrap<T> w)
    {
    return wrap<T>(s * w.t);
    }

template <class T>
wrap<T> operator+(wrap<T> a, wrap<T> b)
    {
    return wrap<T>(a.t + b.t);
    }

namespace
boost
    {
    namespace
    qvm
        {
        template <class T>
        struct
        is_scalar<wrap<T> >
            {
            static bool const value=true;
            };
        template <class S, class T>
        struct
        deduce_scalar<S, wrap<T> >
            {
            typedef wrap<typename deduce_scalar<S, T>::type> type;
            };
        }
    }

int
main()
    {
    using namespace boost::qvm;
    quat<double> q = rotz_quat(3.14159);
    vec<wrap<double>, 3> v;
    v.a[0] = wrap<double>(1.0);
    v.a[1] = wrap<double>(0);
    v.a[2] = wrap<double>(0);
    vec<wrap<double>, 3> r = q * v;
    BOOST_TEST_LT(fabs(r.a[0].t+1), 0.0001);
    BOOST_TEST_LT(fabs(r.a[1].t), 0.0001);
    BOOST_TEST_LT(fabs(r.a[2].t), 0.0001);
    return boost::report_errors();
    }
