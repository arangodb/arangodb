///////////////////////////////////////////////////////////////
//  Copyright 2018 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

//[mpfr_variable

/*` 
This example illustrates the use of variable-precision arithmetic with
the `mpfr_float` number type.  We'll calculate the median of the
beta distribution to an absurdly high precision and compare the
accuracy and times taken for various methods.  That is, we want
to calculate the value of `x` for which ['I[sub x](a, b) = 0.5].
 
Ultimately we'll use Newtons method and set the precision of
mpfr_float to have just enough digits at each iteration.

The full source of the this program is in [@../../example/mpfr_precision.cpp]

We'll skip over the #includes and using declations, and go straight to
some support code, first off a simple stopwatch for performance measurement:

*/

//=template <class clock_type>
//=struct stopwatch { /*details \*/ };

/*`
We'll use `stopwatch<std::chono::high_resolution_clock>` as our performance measuring device.

We also have a small utility class for controlling the current precision of mpfr_float:

   struct scoped_precision
   {
      unsigned p;
      scoped_precision(unsigned new_p) : p(mpfr_float::default_precision())
      {
         mpfr_float::default_precision(new_p);
      }
      ~scoped_precision()
      {
         mpfr_float::default_precision(p);
      }
   };

*/
//<-
#include <boost/multiprecision/mpfr.hpp>
#include <boost/math/special_functions/beta.hpp>
#include <boost/math/special_functions/relative_difference.hpp>
#include <iostream>
#include <chrono>

using boost::multiprecision::mpfr_float;
using boost::math::ibeta_inv;
using namespace boost::math::policies;

template <class clock_type>
struct stopwatch
{
public:
   typedef typename clock_type::duration duration_type;

   stopwatch() : m_start(clock_type::now()) { }

   stopwatch(const stopwatch& other) : m_start(other.m_start) { }

   stopwatch& operator=(const stopwatch& other)
   {
      m_start = other.m_start;
      return *this;
   }

   ~stopwatch() { }

   float elapsed() const
   {
      return float(std::chrono::nanoseconds((clock_type::now() - m_start)).count()) / 1e9f;
   }

   void reset()
   {
      m_start = clock_type::now();
   }

private:
   typename clock_type::time_point m_start;
};

struct scoped_precision
{
   unsigned p;
   scoped_precision(unsigned new_p) : p(mpfr_float::default_precision())
   {
      mpfr_float::default_precision(new_p);
   }
   ~scoped_precision()
   {
      mpfr_float::default_precision(p);
   }
};
//->

/*`
We'll begin with a reference method that simply calls the Boost.Math function `ibeta_inv` and uses the
full working precision of the arguments throughout.  Our reference function takes 3 arguments:

* The 2 parameters `a` and `b` of the beta distribution, and
* The number of decimal digits precision to achieve in the result.

We begin by setting the default working precision to that requested, and then, since we don't know where
our arguments `a` and `b` have been or what precision they have, we make a copy of them - note that since
copying also copies the precision as well as the value, we have to set the precision expicitly with a 
second argument to the copy.  Then we can simply return the result of `ibeta_inv`:
*/
mpfr_float beta_distribution_median_method_1(mpfr_float const& a_, mpfr_float const& b_, unsigned digits10)
{
   scoped_precision sp(digits10);
   mpfr_float half(0.5), a(a_, digits10), b(b_, digits10);
   return ibeta_inv(a, b, half);
}
/*`
You be wondering why we needed to change the precision of our variables `a` and `b` as well as setting the default -
there are in fact two ways in which this can go wrong if we don't do that:

* The variables have too much precision - this will cause all arithmetic operations involving those types to be
promoted to the higher precision wasting precious calculation time.
* The variables have too little precision - this will cause expressions involving only those variables to be
calculated at the lower precision - for example if we calculate `exp(a)` internally, this will be evaluated at
the precision of `a`, and not the current default.

Since our reference method carries out all calculations at the full precision requested, an obvious refinement
would be to calculate a first approximation to `double` precision and then to use Newton steps to refine it further.

Our function begins the same as before: set the new default precision and then make copies of our arguments
at the correct precision.  We then call `ibeta_inv` with all double precision arguments, promote the result
to an `mpfr_float` and perform Newton steps to obtain the result.  Note that our termination condition is somewhat
cude: we simply assume that we have approximately 14 digits correct from the double-precision approximation and
that the precision doubles with each step.  We also cheat, and use an internal Boost.Math function that calculates
['I[sub x](a, b)] and it's derivative in one go:

*/
mpfr_float beta_distribution_median_method_2(mpfr_float const& a_, mpfr_float const& b_, unsigned digits10)
{
   scoped_precision sp(digits10);
   mpfr_float half(0.5), a(a_, digits10), b(b_, digits10);
   mpfr_float guess = ibeta_inv((double)a, (double)b, 0.5);
   unsigned current_digits = 14;
   mpfr_float f, f1;
   while (current_digits < digits10)
   {
      f = boost::math::detail::ibeta_imp(a, b, guess, boost::math::policies::policy<>(), false, true, &f1) - half;
      guess -= f / f1;
      current_digits *= 2;
   }
   return guess;
}
/*`
Before we refine the method further, it might be wise to take stock and see how method's 1 and 2 compare.
We'll ask them both for 1500 digit precision, and compare against the value produced by `ibeta_inv` at 1700 digits.
Here's what the results look like:

[pre
Method 1 time = 0.611647
Relative error: 2.99991e-1501
Method 2 time = 0.646746
Relative error: 7.55843e-1501
]

Clearly they are both equally accurate, but Method 1 is actually faster and our plan for improved performance 
hasn't actually worked.  It turns out that we're not actually comparing like with like, because `ibeta_inv` uses
Halley iteration internally which churns out more digits of precision rather more rapidly than Newton iteration.  
So the time we save by refining an initial `double` approximation, then loose it again by taking more iterations
to get to the result.

Time for a more refined approach.  It follows the same form as Method 2, but now we set the working precision
within the Newton iteration loop, to just enough digits to cover the expected precision at each step.  That means
we also create new copies of our arguments at the correct precision within the loop, and likewise change the precision
of the current `guess` each time through:

*/

mpfr_float beta_distribution_median_method_3(mpfr_float const& a_, mpfr_float const& b_, unsigned digits10)
{
   mpfr_float guess = ibeta_inv((double)a_, (double)b_, 0.5);
   unsigned current_digits = 14;
   mpfr_float f(0, current_digits), f1(0, current_digits), delta(1);
   while (current_digits < digits10)
   {
      current_digits *= 2;
      scoped_precision sp(std::min(current_digits, digits10));
      mpfr_float a(a_, mpfr_float::default_precision()), b(b_, mpfr_float::default_precision());
      guess.precision(mpfr_float::default_precision());
      f = boost::math::detail::ibeta_imp(a, b, guess, boost::math::policies::policy<>(), false, true, &f1) - 0.5f;
      guess -= f / f1;
   }
   return guess;
}

/*`
The new performance results look much more promising:

[pre
Method 1 time = 0.591244
Relative error: 2.99991e-1501
Method 2 time = 0.622679
Relative error: 7.55843e-1501
Method 3 time = 0.143393
Relative error: 4.03898e-1501
]

This time we're 4x faster than `ibeta_inv`, and no doubt that could be improved a little more by carefully
optimising the number of iterations and the method (Halley vs Newton) taken.

Finally, here's the driver code for the above methods:

*/

int main()
{
   try {
      mpfr_float a(10), b(20);

      mpfr_float true_value = beta_distribution_median_method_1(a, b, 1700);

      stopwatch<std::chrono::high_resolution_clock> my_stopwatch;

      mpfr_float v1 = beta_distribution_median_method_1(a, b, 1500);
      float hp_time = my_stopwatch.elapsed();
      std::cout << "Method 1 time = " << hp_time << std::endl;
      std::cout << "Relative error: " << boost::math::relative_difference(v1, true_value) << std::endl;

      my_stopwatch.reset();
      mpfr_float v2 = beta_distribution_median_method_2(a, b, 1500);
      hp_time = my_stopwatch.elapsed();
      std::cout << "Method 2 time = " << hp_time << std::endl;
      std::cout << "Relative error: " << boost::math::relative_difference(v2, true_value) << std::endl;

      my_stopwatch.reset();
      mpfr_float v3 = beta_distribution_median_method_3(a, b, 1500);
      hp_time = my_stopwatch.elapsed();
      std::cout << "Method 3 time = " << hp_time << std::endl;
      std::cout << "Relative error: " << boost::math::relative_difference(v3, true_value) << std::endl;
   }
   catch (const std::exception& e)
   {
      std::cout << "Found exception with message: " << e.what() << std::endl;
   }
   return 0;
}
//]

