///////////////////////////////////////////////////////////////////////////////
//  Copyright 2018 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_complex.hpp>
#ifdef BOOST_HAS_FLOAT128
#include <boost/multiprecision/float128.hpp>
#endif
#ifdef TEST_GMP
#include <boost/multiprecision/gmp.hpp>
#endif
#ifdef TEST_MPFR
#include <boost/multiprecision/mpfr.hpp>
#endif
#ifdef TEST_MPC
#include <boost/multiprecision/mpc.hpp>
#endif

struct A { virtual void g() = 0; };

void f(A&);
void f(boost::multiprecision::cpp_bin_float_50);
void f(boost::multiprecision::cpp_int);
void f(boost::multiprecision::cpp_rational);
void f(boost::multiprecision::cpp_dec_float_50);
void f(boost::multiprecision::cpp_complex_100);
#ifdef TEST_FLOAT128
void f(boost::multiprecision::float128);
#endif
#ifdef TEST_GMP
void f(boost::multiprecision::mpz_int);
void f(boost::multiprecision::mpf_float);
void f(boost::multiprecision::mpq_rational);
#endif
#ifdef TEST_MPFR
void f(boost::multiprecision::mpfr_float);
#endif
#ifdef TEST_MPC
void f(boost::multiprecision::mpc_complex);
#endif

void h(A&a)
{
   f(a);
}
