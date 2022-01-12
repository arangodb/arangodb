//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_QVM_TEST_SINGLE_HEADER
#   include BOOST_QVM_TEST_SINGLE_HEADER
#else
#   include <boost/qvm/math.hpp>
#endif

#include <boost/core/lightweight_test.hpp>
#include <stdlib.h>

namespace
    {
    template <class T>
    void
    test1( T (*f1)(T), T (*f2)(T) )
        {
        for( int i=0; i!=100; ++i )
            {
            T a = T(rand()) / T(RAND_MAX);
            BOOST_TEST_EQ(f1(a), f2(a));
            }
        }
    template <class T,class U>
    void
    test2( T (*f1)(T,U), T (*f2)(T,U) )
        {
        for( int i=0; i!=100; ++i )
            {
            T a = T(rand()) / T(RAND_MAX);
            T b = T(rand()) / T(RAND_MAX);
            BOOST_TEST_EQ(f1(a,b), f2(a,b));
            }
        }
    }

int
main()
    {
    test1<float>(&boost::qvm::acos<float>, &::acosf);
    test1<float>(&boost::qvm::asin<float>, &::asinf);
    test1<float>(&boost::qvm::atan<float>, &::atanf);
    test2<float,float>(&boost::qvm::atan2<float>, &::atan2f);
    test1<float>(&boost::qvm::cos<float>, &::cosf);
    test1<float>(&boost::qvm::sin<float>, &::sinf);
    test1<float>(&boost::qvm::tan<float>, &::tanf);
    test1<float>(&boost::qvm::cosh<float>, &::coshf);
    test1<float>(&boost::qvm::sinh<float>, &::sinhf);
    test1<float>(&boost::qvm::tanh<float>, &::tanhf);
    test1<float>(&boost::qvm::exp<float>, &::expf);
    test1<float>(&boost::qvm::log<float>, &::logf);
    test1<float>(&boost::qvm::log10<float>, &::log10f);
    test2<float,float>(&boost::qvm::mod<float>, &::fmodf);
    test2<float,float>(&boost::qvm::pow<float>, &::powf);
    test1<float>(&boost::qvm::sqrt<float>, &::sqrtf);
    test1<float>(&boost::qvm::ceil<float>, &::ceilf);
    test1<float>(&boost::qvm::abs<float>, &::fabsf);
    test1<float>(&boost::qvm::floor<float>, &::floorf);
    test2<float, int>(&boost::qvm::ldexp<float>, &::ldexpf);

    test1<double>(&boost::qvm::acos<double>, &::acos);
    test1<double>(&boost::qvm::asin<double>, &::asin);
    test1<double>(&boost::qvm::atan<double>, &::atan);
    test2<double,double>(&boost::qvm::atan2<double>, &::atan2);
    test1<double>(&boost::qvm::cos<double>, &::cos);
    test1<double>(&boost::qvm::sin<double>, &::sin);
    test1<double>(&boost::qvm::tan<double>, &::tan);
    test1<double>(&boost::qvm::cosh<double>, &::cosh);
    test1<double>(&boost::qvm::sinh<double>, &::sinh);
    test1<double>(&boost::qvm::tanh<double>, &::tanh);
    test1<double>(&boost::qvm::exp<double>, &::exp);
    test1<double>(&boost::qvm::log<double>, &::log);
    test1<double>(&boost::qvm::log10<double>, &::log10);
    test2<double,double>(&boost::qvm::mod<double>, &::fmod);
    test2<double,double>(&boost::qvm::pow<double>, &::pow);
    test1<double>(&boost::qvm::sqrt<double>, &::sqrt);
    test1<double>(&boost::qvm::ceil<double>, &::ceil);
    test1<double>(&boost::qvm::abs<double>, &::fabs);
    test1<double>(&boost::qvm::floor<double>, &::floor);
    test2<double, int>(&boost::qvm::ldexp<double>, &::ldexp);

    test1<long double>(&boost::qvm::acos<long double>, &::acosl);
    test1<long double>(&boost::qvm::asin<long double>, &::asinl);
    test1<long double>(&boost::qvm::atan<long double>, &::atanl);
    test2<long double,long double>(&boost::qvm::atan2<long double>, &::atan2l);
    test1<long double>(&boost::qvm::cos<long double>, &::cosl);
    test1<long double>(&boost::qvm::sin<long double>, &::sinl);
    test1<long double>(&boost::qvm::tan<long double>, &::tanl);
    test1<long double>(&boost::qvm::cosh<long double>, &::coshl);
    test1<long double>(&boost::qvm::sinh<long double>, &::sinhl);
    test1<long double>(&boost::qvm::tanh<long double>, &::tanhl);
    test1<long double>(&boost::qvm::exp<long double>, &::expl);
    test1<long double>(&boost::qvm::log<long double>, &::logl);
    test1<long double>(&boost::qvm::log10<long double>, &::log10l);
    test2<long double,long double>(&boost::qvm::mod<long double>, &::fmodl);
    test2<long double,long double>(&boost::qvm::pow<long double>, &::powl);
    test1<long double>(&boost::qvm::sqrt<long double>, &::sqrtl);
    test1<long double>(&boost::qvm::ceil<long double>, &::ceill);
    test1<long double>(&boost::qvm::abs<long double>, &::fabsl);
    test1<long double>(&boost::qvm::floor<long double>, &::floorl);
    test2<long double, int>(&boost::qvm::ldexp<long double>, &::ldexpl);

    return boost::report_errors();
    }
