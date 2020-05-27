/*
 *  (C) Copyright Nick Thompson 2018.
 *  Use, modification and distribution are subject to the
 *  Boost Software License, Version 1.0. (See accompanying file
 *  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#include "multiprecision_config.hpp"

#ifndef DISABLE_MP_TESTS
#include <boost/integer/mod_inverse.hpp>
#include <boost/cstdint.hpp>
#include <boost/optional/optional.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/integer/common_factor.hpp>

using boost::multiprecision::int128_t;
using boost::multiprecision::int256_t;
using boost::integer::mod_inverse;
using boost::integer::gcd;

template<class Z>
void test_mod_inverse()
{
    //Z max_arg = std::numeric_limits<Z>::max();
    Z max_arg = 500;
    for (Z modulus = 2; modulus < max_arg; ++modulus)
    {
        if (modulus % 1000 == 0)
        {
            std::cout << "Testing all inverses modulo " << modulus << std::endl;
        }
        for (Z a = 1; a < modulus; ++a)
        {
            Z gcdam = gcd(a, modulus);
            Z inv_a = mod_inverse(a, modulus);
            // Should fail if gcd(a, mod) != 1:
            if (gcdam > 1)
            {
                BOOST_TEST(inv_a == 0);
            }
            else
            {
                BOOST_TEST(inv_a > 0);
                // Cast to a bigger type so the multiplication won't overflow.
                int256_t a_inv = inv_a;
                int256_t big_a = a;
                int256_t m = modulus;
                int256_t outta_be_one = (a_inv*big_a) % m;
                BOOST_TEST_EQ(outta_be_one, 1);
            }
        }
    }
}

int main()
{
    test_mod_inverse<boost::int16_t>();
    test_mod_inverse<boost::int32_t>();
    test_mod_inverse<boost::int64_t>();
    test_mod_inverse<int128_t>();

    return boost::report_errors();
}
#else
int main()
{
  return 0;
}
#endif
