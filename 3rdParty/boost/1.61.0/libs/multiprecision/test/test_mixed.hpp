///////////////////////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MATH_TEST_MIXED_HPP
#define BOOST_MATH_TEST_MIXED_HPP

#include "test.hpp"

template <class Big, class Small>
void test()
{
   Big big_val = 1;
   big_val += std::numeric_limits<Big>::epsilon();
   Small small_val = 1;

   BOOST_CHECK_EQUAL(big_val == small_val, false);
   BOOST_CHECK_EQUAL(big_val <= small_val, false);
   BOOST_CHECK_EQUAL(big_val >= small_val, true);
   BOOST_CHECK_EQUAL(big_val < small_val, false);
   BOOST_CHECK_EQUAL(big_val > small_val, true);
   BOOST_CHECK_EQUAL(big_val != small_val, true);
   BOOST_CHECK_EQUAL(small_val == big_val, false);
   BOOST_CHECK_EQUAL(small_val <= big_val, true);
   BOOST_CHECK_EQUAL(small_val >= big_val, false);
   BOOST_CHECK_EQUAL(small_val < big_val, true);
   BOOST_CHECK_EQUAL(small_val > big_val, false);
   BOOST_CHECK_EQUAL(small_val != big_val, true);
   // Again with expression templates, on one the other, or both args:
   BOOST_CHECK_EQUAL(big_val == small_val * 1, false);
   BOOST_CHECK_EQUAL(big_val <= small_val * 1, false);
   BOOST_CHECK_EQUAL(big_val >= small_val * 1, true);
   BOOST_CHECK_EQUAL(big_val < small_val * 1, false);
   BOOST_CHECK_EQUAL(big_val > small_val * 1, true);
   BOOST_CHECK_EQUAL(big_val != small_val * 1, true);
   BOOST_CHECK_EQUAL(small_val * 1 == big_val, false);
   BOOST_CHECK_EQUAL(small_val * 1 <= big_val, true);
   BOOST_CHECK_EQUAL(small_val * 1 >= big_val, false);
   BOOST_CHECK_EQUAL(small_val * 1 < big_val, true);
   BOOST_CHECK_EQUAL(small_val * 1 > big_val, false);
   BOOST_CHECK_EQUAL(small_val * 1 != big_val, true);
   BOOST_CHECK_EQUAL(big_val * 1 == small_val, false);
   BOOST_CHECK_EQUAL(big_val * 1 <= small_val, false);
   BOOST_CHECK_EQUAL(big_val * 1 >= small_val, true);
   BOOST_CHECK_EQUAL(big_val * 1 < small_val, false);
   BOOST_CHECK_EQUAL(big_val * 1 > small_val, true);
   BOOST_CHECK_EQUAL(big_val * 1 != small_val, true);
   BOOST_CHECK_EQUAL(small_val == big_val * 1, false);
   BOOST_CHECK_EQUAL(small_val <= big_val * 1, true);
   BOOST_CHECK_EQUAL(small_val >= big_val * 1, false);
   BOOST_CHECK_EQUAL(small_val < big_val * 1, true);
   BOOST_CHECK_EQUAL(small_val > big_val * 1, false);
   BOOST_CHECK_EQUAL(small_val != big_val * 1, true);
   BOOST_CHECK_EQUAL(big_val * 1 == small_val * 1, false);
   BOOST_CHECK_EQUAL(big_val * 1 <= small_val * 1, false);
   BOOST_CHECK_EQUAL(big_val * 1 >= small_val * 1, true);
   BOOST_CHECK_EQUAL(big_val * 1 < small_val * 1, false);
   BOOST_CHECK_EQUAL(big_val * 1 > small_val * 1, true);
   BOOST_CHECK_EQUAL(big_val * 1 != small_val * 1, true);
   BOOST_CHECK_EQUAL(small_val * 1 == big_val * 1, false);
   BOOST_CHECK_EQUAL(small_val * 1 <= big_val * 1, true);
   BOOST_CHECK_EQUAL(small_val * 1 >= big_val * 1, false);
   BOOST_CHECK_EQUAL(small_val * 1 < big_val * 1, true);
   BOOST_CHECK_EQUAL(small_val * 1 > big_val * 1, false);
   BOOST_CHECK_EQUAL(small_val * 1 != big_val * 1, true);


   BOOST_CHECK_EQUAL(small_val + big_val, Big(small_val) + big_val);
   BOOST_CHECK_EQUAL(small_val - big_val, Big(small_val) - big_val);
   BOOST_CHECK_EQUAL(small_val * big_val, Big(small_val) * big_val);
   BOOST_CHECK_EQUAL(small_val / big_val, Big(small_val) / big_val);
   BOOST_CHECK_EQUAL(big_val + small_val, big_val + Big(small_val));
   BOOST_CHECK_EQUAL(big_val - small_val, big_val - Big(small_val));
   BOOST_CHECK_EQUAL(big_val * small_val, big_val * Big(small_val));
   BOOST_CHECK_EQUAL(big_val / small_val, big_val / Big(small_val));
   // Again with expression templates, on one the other, or both args:
   BOOST_CHECK_EQUAL(small_val + (big_val * 1), Big(small_val) + (big_val * 1));
   BOOST_CHECK_EQUAL(small_val - (big_val * 1), Big(small_val) - (big_val * 1));
   BOOST_CHECK_EQUAL(small_val * (big_val * 1), Big(small_val) * (big_val * 1));
   BOOST_CHECK_EQUAL(small_val / (big_val * 1), Big(small_val) / (big_val * 1));
   BOOST_CHECK_EQUAL((big_val * 1) + small_val, (big_val * 1) + Big(small_val));
   BOOST_CHECK_EQUAL((big_val * 1) - small_val, (big_val * 1) - Big(small_val));
   BOOST_CHECK_EQUAL((big_val * 1) * small_val, (big_val * 1) * Big(small_val));
   BOOST_CHECK_EQUAL((big_val * 1) / small_val, (big_val * 1) / Big(small_val));
   BOOST_CHECK_EQUAL((small_val * 1) + big_val, Big((small_val * 1)) + big_val);
   BOOST_CHECK_EQUAL((small_val * 1) - big_val, Big((small_val * 1)) - big_val);
   BOOST_CHECK_EQUAL((small_val * 1) * big_val, Big((small_val * 1)) * big_val);
   BOOST_CHECK_EQUAL((small_val * 1) / big_val, Big((small_val * 1)) / big_val);
   BOOST_CHECK_EQUAL(big_val + (small_val * 1), big_val + Big((small_val * 1)));
   BOOST_CHECK_EQUAL(big_val - (small_val * 1), big_val - Big((small_val * 1)));
   BOOST_CHECK_EQUAL(big_val * (small_val * 1), big_val * Big((small_val * 1)));
   BOOST_CHECK_EQUAL(big_val / (small_val * 1), big_val / Big((small_val * 1)));
   BOOST_CHECK_EQUAL((small_val * 1) + (big_val * 1), Big((small_val * 1)) + (big_val * 1));
   BOOST_CHECK_EQUAL((small_val * 1) - (big_val * 1), Big((small_val * 1)) - (big_val * 1));
   BOOST_CHECK_EQUAL((small_val * 1) * (big_val * 1), Big((small_val * 1)) * (big_val * 1));
   BOOST_CHECK_EQUAL((small_val * 1) / (big_val * 1), Big((small_val * 1)) / (big_val * 1));
   BOOST_CHECK_EQUAL((big_val * 1) + (small_val * 1), (big_val * 1) + Big((small_val * 1)));
   BOOST_CHECK_EQUAL((big_val * 1) - (small_val * 1), (big_val * 1) - Big((small_val * 1)));
   BOOST_CHECK_EQUAL((big_val * 1) * (small_val * 1), (big_val * 1) * Big((small_val * 1)));
   BOOST_CHECK_EQUAL((big_val * 1) / (small_val * 1), (big_val * 1) / Big((small_val * 1)));

   big_val = 1;
   big_val -= std::numeric_limits<Big>::epsilon();

   BOOST_CHECK_EQUAL(big_val == small_val, false);
   BOOST_CHECK_EQUAL(big_val <= small_val, true);
   BOOST_CHECK_EQUAL(big_val >= small_val, false);
   BOOST_CHECK_EQUAL(big_val < small_val, true);
   BOOST_CHECK_EQUAL(big_val > small_val, false);
   BOOST_CHECK_EQUAL(big_val != small_val, true);
   BOOST_CHECK_EQUAL(small_val == big_val, false);
   BOOST_CHECK_EQUAL(small_val <= big_val, false);
   BOOST_CHECK_EQUAL(small_val >= big_val, true);
   BOOST_CHECK_EQUAL(small_val < big_val, false);
   BOOST_CHECK_EQUAL(small_val > big_val, true);
   BOOST_CHECK_EQUAL(small_val != big_val, true);
   // Again with expression templates, on one the other, or both args:
   BOOST_CHECK_EQUAL(big_val == small_val * 1, false);
   BOOST_CHECK_EQUAL(big_val <= small_val * 1, true);
   BOOST_CHECK_EQUAL(big_val >= small_val * 1, false);
   BOOST_CHECK_EQUAL(big_val < small_val * 1, true);
   BOOST_CHECK_EQUAL(big_val > small_val * 1, false);
   BOOST_CHECK_EQUAL(big_val != small_val * 1, true);
   BOOST_CHECK_EQUAL(small_val * 1 == big_val, false);
   BOOST_CHECK_EQUAL(small_val * 1 <= big_val, false);
   BOOST_CHECK_EQUAL(small_val * 1 >= big_val, true);
   BOOST_CHECK_EQUAL(small_val * 1 < big_val, false);
   BOOST_CHECK_EQUAL(small_val * 1 > big_val, true);
   BOOST_CHECK_EQUAL(small_val * 1 != big_val, true);
   BOOST_CHECK_EQUAL(big_val * 1 == small_val, false);
   BOOST_CHECK_EQUAL(big_val * 1 <= small_val, true);
   BOOST_CHECK_EQUAL(big_val * 1 >= small_val, false);
   BOOST_CHECK_EQUAL(big_val * 1 < small_val, true);
   BOOST_CHECK_EQUAL(big_val * 1 > small_val, false);
   BOOST_CHECK_EQUAL(big_val * 1 != small_val, true);
   BOOST_CHECK_EQUAL(small_val == big_val * 1, false);
   BOOST_CHECK_EQUAL(small_val <= big_val * 1, false);
   BOOST_CHECK_EQUAL(small_val >= big_val * 1, true);
   BOOST_CHECK_EQUAL(small_val < big_val * 1, false);
   BOOST_CHECK_EQUAL(small_val > big_val * 1, true);
   BOOST_CHECK_EQUAL(small_val != big_val * 1, true);
   BOOST_CHECK_EQUAL(big_val * 1 == small_val * 1, false);
   BOOST_CHECK_EQUAL(big_val * 1 <= small_val * 1, true);
   BOOST_CHECK_EQUAL(big_val * 1 >= small_val * 1, false);
   BOOST_CHECK_EQUAL(big_val * 1 < small_val * 1, true);
   BOOST_CHECK_EQUAL(big_val * 1 > small_val * 1, false);
   BOOST_CHECK_EQUAL(big_val * 1 != small_val * 1, true);
   BOOST_CHECK_EQUAL(small_val * 1 == big_val * 1, false);
   BOOST_CHECK_EQUAL(small_val * 1 <= big_val * 1, false);
   BOOST_CHECK_EQUAL(small_val * 1 >= big_val * 1, true);
   BOOST_CHECK_EQUAL(small_val * 1 < big_val * 1, false);
   BOOST_CHECK_EQUAL(small_val * 1 > big_val * 1, true);
   BOOST_CHECK_EQUAL(small_val * 1 != big_val * 1, true);

   BOOST_CHECK_EQUAL(small_val + big_val, Big(small_val) + big_val);
   BOOST_CHECK_EQUAL(small_val - big_val, Big(small_val) - big_val);
   BOOST_CHECK_EQUAL(small_val * big_val, Big(small_val) * big_val);
   BOOST_CHECK_EQUAL(small_val / big_val, Big(small_val) / big_val);
   BOOST_CHECK_EQUAL(big_val + small_val, big_val + Big(small_val));
   BOOST_CHECK_EQUAL(big_val - small_val, big_val - Big(small_val));
   BOOST_CHECK_EQUAL(big_val * small_val, big_val * Big(small_val));
   BOOST_CHECK_EQUAL(big_val / small_val, big_val / Big(small_val));
   // Again with expression templates, on one the other, or both args:
   BOOST_CHECK_EQUAL(small_val + (big_val * 1), Big(small_val) + (big_val * 1));
   BOOST_CHECK_EQUAL(small_val - (big_val * 1), Big(small_val) - (big_val * 1));
   BOOST_CHECK_EQUAL(small_val * (big_val * 1), Big(small_val) * (big_val * 1));
   BOOST_CHECK_EQUAL(small_val / (big_val * 1), Big(small_val) / (big_val * 1));
   BOOST_CHECK_EQUAL((big_val * 1) + small_val, (big_val * 1) + Big(small_val));
   BOOST_CHECK_EQUAL((big_val * 1) - small_val, (big_val * 1) - Big(small_val));
   BOOST_CHECK_EQUAL((big_val * 1) * small_val, (big_val * 1) * Big(small_val));
   BOOST_CHECK_EQUAL((big_val * 1) / small_val, (big_val * 1) / Big(small_val));
   BOOST_CHECK_EQUAL((small_val * 1) + big_val, Big((small_val * 1)) + big_val);
   BOOST_CHECK_EQUAL((small_val * 1) - big_val, Big((small_val * 1)) - big_val);
   BOOST_CHECK_EQUAL((small_val * 1) * big_val, Big((small_val * 1)) * big_val);
   BOOST_CHECK_EQUAL((small_val * 1) / big_val, Big((small_val * 1)) / big_val);
   BOOST_CHECK_EQUAL(big_val + (small_val * 1), big_val + Big((small_val * 1)));
   BOOST_CHECK_EQUAL(big_val - (small_val * 1), big_val - Big((small_val * 1)));
   BOOST_CHECK_EQUAL(big_val * (small_val * 1), big_val * Big((small_val * 1)));
   BOOST_CHECK_EQUAL(big_val / (small_val * 1), big_val / Big((small_val * 1)));
   BOOST_CHECK_EQUAL((small_val * 1) + (big_val * 1), Big((small_val * 1)) + (big_val * 1));
   BOOST_CHECK_EQUAL((small_val * 1) - (big_val * 1), Big((small_val * 1)) - (big_val * 1));
   BOOST_CHECK_EQUAL((small_val * 1) * (big_val * 1), Big((small_val * 1)) * (big_val * 1));
   BOOST_CHECK_EQUAL((small_val * 1) / (big_val * 1), Big((small_val * 1)) / (big_val * 1));
   BOOST_CHECK_EQUAL((big_val * 1) + (small_val * 1), (big_val * 1) + Big((small_val * 1)));
   BOOST_CHECK_EQUAL((big_val * 1) - (small_val * 1), (big_val * 1) - Big((small_val * 1)));
   BOOST_CHECK_EQUAL((big_val * 1) * (small_val * 1), (big_val * 1) * Big((small_val * 1)));
   BOOST_CHECK_EQUAL((big_val * 1) / (small_val * 1), (big_val * 1) / Big((small_val * 1)));
}

#endif // BOOST_MATH_TEST_MIXED_HPP
