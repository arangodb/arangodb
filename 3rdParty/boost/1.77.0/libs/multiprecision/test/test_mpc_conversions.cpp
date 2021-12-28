///////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2002 - 2011.
//  Copyright 2011 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
//
// This work is based on an earlier work:
// "Algorithm 910: A Portable C++ Multiple-Precision System for Special-Function Calculations",
// in ACM TOMS, {VOL 37, ISSUE 4, (February 2011)} (C) ACM, 2011. http://doi.acm.org/10.1145/1916461.1916469

#ifdef _MSC_VER
#define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/detail/lightweight_test.hpp>
#include <boost/array.hpp>
#include "test.hpp"

#include <boost/multiprecision/mpc.hpp>
#include <boost/multiprecision/gmp.hpp>

int main()
{
   using namespace boost::multiprecision;
   //
   // Test interconversions between GMP supported backends:
   //
   mpf_t  mpf;
   mpz_t  mpz;
   mpq_t  mpq;
   mpfr_t mpfr;
   mpf_init2(mpf, 100);
   mpf_set_ui(mpf, 2u);
   mpz_init(mpz);
   mpz_set_ui(mpz, 2u);
   mpq_init(mpq);
   mpq_set_ui(mpq, 2u, 1u);
   mpfr_init(mpfr);
   mpfr_set_ui(mpfr, 2u, GMP_RNDN);

   BOOST_TEST(mpc_complex(mpf) == 2);
   BOOST_TEST(mpc_complex_50(mpf) == 2);
   BOOST_TEST(mpc_complex(mpz) == 2);
   BOOST_TEST(mpc_complex_50(mpz) == 2);
   BOOST_TEST(mpc_complex(mpq) == 2);
   BOOST_TEST(mpc_complex_50(mpq) == 2);
   BOOST_TEST(mpc_complex(mpfr) == 2);
   BOOST_TEST(mpc_complex_50(mpfr) == 2);

   mpc_complex    f0;
   mpc_complex_50 f50;
   f0 = mpf;
   BOOST_TEST(f0 == 2);
   f0 = 0;
   f0 = mpz;
   BOOST_TEST(f0 == 2);
   f0 = 0;
   f0 = mpq;
   BOOST_TEST(f0 == 2);
   f0 = mpfr;
   BOOST_TEST(f0 == 2);

   f50 = mpf;
   BOOST_TEST(f50 == 2);
   f50 = 0;
   f50 = mpz;
   BOOST_TEST(f50 == 2);
   f50 = 0;
   f50 = mpq;
   BOOST_TEST(f50 == 2);
   f50 = mpfr;
   BOOST_TEST(f50 == 2);

   f50 = 4;
   f0  = f50;
   BOOST_TEST(f0 == 4);
   f0  = 3;
   f50 = f0;
   BOOST_TEST(f50 == 3);
   f50 = 4;
   BOOST_TEST(mpc_complex(f50) == 4);
   BOOST_TEST(mpc_complex_50(f0) == 3);

   mpz_int      iz(2);
   mpq_rational rat(2);
   mpf_float    gf(2);
   f50 = 3;
   f50 = iz;
   BOOST_TEST(f50 == 2);
   f50 = 3;
   f0  = iz;
   BOOST_TEST(f0 == 2);
   f50 = 3;
   f0  = gf;
   BOOST_TEST(f0 == 2);
   BOOST_TEST(mpc_complex(iz) == 2);
   BOOST_TEST(mpc_complex_50(iz) == 2);
   BOOST_TEST(mpc_complex(rat) == 2);
   BOOST_TEST(mpc_complex_50(rat) == 2);
   BOOST_TEST(mpc_complex(gf) == 2);
   BOOST_TEST(mpc_complex_50(gf) == 2);

   //
   // Conversions involving precision only:
   //
   mpc_complex::default_precision(30);
   f50 = 2;
   mpc_complex_100 f100(3);
   mpc_complex     f0a(4);
   mpc_complex     f0b(f100);
   BOOST_TEST(f0a.precision() == 30);
   BOOST_TEST(f0b.precision() == 100);
   f0a = f100;
   BOOST_TEST(f0a == 3);
   BOOST_TEST(f0a.precision() == 100); // precision preserved on assignment

   f100 = f50;
   BOOST_TEST(f100 == 2);

   f50 = static_cast<mpc_complex_50>(f100);

   mpf_clear(mpf);
   mpz_clear(mpz);
   mpq_clear(mpq);
   mpfr_clear(mpfr);

   return boost::report_errors();
}
