///////////////////////////////////////////////////////////////
//  Copyright 2021 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#include <iostream>
#include <boost/math/special_functions/relative_difference.hpp>
#include <boost/math/special_functions/next.hpp>
#include <boost/multiprecision/mpfr.hpp>
#include <boost/multiprecision/debug_adaptor.hpp>

//[scoped_precision_1

/*`
All our precision changing examples are based around `mpfr_float`.
However, in order to make running this example a little easier to debug,
we'll use `debug_adaptor` throughout so that the values of all variables
can be displayed in your debugger of choice:
*/

using mp_t = boost::multiprecision::debug_adaptor_t<boost::multiprecision::mpfr_float>;

/*`
Our first example will investigate calculating the Bessel J function via it's well known 
series representation:

[$../bessel2.svg]

This simple series suffers from catastrophic cancellation error
near the roots of the function, so we'll investigate slowly increasing the precision of
the calculation until we get the result to N-decimal places.  We'll begin by defining
a function to calculate the series for Bessel J, the details of which we'll leave in the
source code:
*/
mp_t calculate_bessel_J_as_series(mp_t x, mp_t v, mp_t* err)
//<-
{
   mp_t sum        = pow(x / 2, v) / tgamma(v + 1);
   mp_t abs_sum    = abs(sum);
   mp_t multiplier = -x * x / 4;
   mp_t divider    = v + 1;
   mp_t term       = sum;
   mp_t eps        = boost::math::tools::epsilon<mp_t>();
   unsigned   m          = 1;

   while (fabs(term / abs_sum) > eps)
   {
      term *= multiplier;
      term /= m;
      term /= divider;
      ++divider;
      ++m;
      sum += term;
      abs_sum += abs(term);
   }
   if (err)
      *err = eps * fabs(abs_sum / sum);
   /*
   std::cout << eps << std::endl;
   if (err)
      std::cout << *err << std::endl;
   std::cout << abs_sum << std::endl;
   std::cout << sum << std::endl;
   */
   return sum;
}
//->

/*`
Next come some simple helper classes, these allow us to modify the current precision and precision-options
via scoped objects which will put everything back as it was at the end.  We'll begin with the class to
modify the working precision:
*/
struct scoped_mpfr_precision
{
   unsigned saved_digits10;
   scoped_mpfr_precision(unsigned digits10) : saved_digits10(mp_t::thread_default_precision())
   {
      mp_t::thread_default_precision(digits10);
   }
   ~scoped_mpfr_precision()
   {
      mp_t::thread_default_precision(saved_digits10);
   }
   void reset(unsigned digits10)
   {
      mp_t::thread_default_precision(digits10);
   }
   void reset()
   {
      mp_t::thread_default_precision(saved_digits10);
   }
};
/*`
And a second class to modify the precision options:
*/
struct scoped_mpfr_precision_options
{
   boost::multiprecision::variable_precision_options saved_options;
   scoped_mpfr_precision_options(boost::multiprecision::variable_precision_options opts) : saved_options(mp_t::thread_default_variable_precision_options())
   {
      mp_t::thread_default_variable_precision_options(opts);
   }
   ~scoped_mpfr_precision_options()
   {
      mp_t::thread_default_variable_precision_options(saved_options);
   }
   void reset(boost::multiprecision::variable_precision_options opts)
   {
      mp_t::thread_default_variable_precision_options(opts);
   }
};
/*`
We can now begin writing a function to calculate J[sub v](z) to a specified precision.  
In order to keep the logic as simple as possible, we'll adopt a ['uniform precision computing] approach, 
which is to say, within the body of the function, all variables are always at the same working precision.
*/
mp_t Bessel_J_to_precision(mp_t v, mp_t x, unsigned digits10)
{
   //
   // Begin by backing up digits10:
   //
   unsigned saved_digits10 = digits10;
   // 
   //
   // Start by defining 2 scoped objects to control precision and associated options.
   // We'll begin by setting the working precision to the required target precision,
   // and since all variables will always be of uniform precision, we can tell the
   // library to ignore all precision control by setting variable_precision_options::assume_uniform_precision:
   //
   scoped_mpfr_precision           scoped(digits10);
   scoped_mpfr_precision_options   scoped_opts(boost::multiprecision::variable_precision_options::assume_uniform_precision);

   mp_t            result;
   mp_t            current_error{1};
   mp_t            target_error {std::pow(10., -static_cast<int>(digits10))};

   while (target_error < current_error)
   {
      //
      // Everything must be of uniform precision in here, including
      // our input values, so we'll begin by setting their precision:
      //
      v.precision(digits10);
      x.precision(digits10);
      //
      // Calculate our approximation and error estimate:
      //
      result = calculate_bessel_J_as_series(x, v, &current_error);
      //
      // If the error from the current approximation is too high we'll need 
      // to loop round and try again, in this case we use the simple heuristic
      // of doubling the working precision with each loop.  More refined approaches
      // are certainly available:
      //
      digits10 *= 2;
      scoped.reset(digits10);
   }
   //
   // We now have an accurate result, but it may have too many digits,
   // so lets round the result to the requested precision now:
   //
   result.precision(saved_digits10);
   //
   // To maintain uniform precision during function return, lets
   // reset the default precision now:
   //
   scoped.reset(saved_digits10);
   return result;
}

/*`
So far, this is all well and good, but there is still a potential trap for the unwary here,
when the function returns the variable [/result] may be copied/moved either once or twice
depending on whether the compiler implements the named-return-value optimisation.  And since this
all happens outside the scope of this function, the precision of the returned value may get unexpected
changed - and potentially with different behaviour once optimisations are turned on!

To prevent these kinds of unintended consequences, a function returning a value with specified precision
must either:

* Be called in a /uniform-precision-environment/, with the current working precision, the same as both
the returned value and the variable to which the result will be assigned.
* Be called in an environment that has one of the following set:
   * variable_precision_options::preserve_source_precision
   * variable_precision_options::preserve_component_precision
   * variable_precision_options::preserve_related_precision
   * variable_precision_options::preserve_all_precision

In the case of our example program, we use a /uniform-precision-environment/ and call the function
with the value of `6541389046624379 / 562949953421312` which happens to be near a root of J[sub v](x)
and requires a high-precision calculation to obtain low relative error in the result of 
`-9.31614245636402072613249153246313221710284959883647822724e-15`.

You will note in the example we have so far that there are a number of unnecessary temporaries
created: we pass values to our functions by value, and we call the `.precision()` member function
to change the working precision of some variables - something that requires a reallocation internally.
We'll now make our example just a little more efficient, by removing these temporaries, though in the
process, we'll need just a little more control over how mixed-precision arithmetic behaves.

It's tempting to simply define the function that calculates the series to take arguments
by constant reference like so:

*/

mp_t calculate_bessel_J_as_series_2(const mp_t& x, const mp_t& v, mp_t* err)
//<-
{
   mp_t sum        = pow(x / 2, v) / tgamma(v + 1);
   mp_t abs_sum    = abs(sum);
   mp_t multiplier = -x * x / 4;
   mp_t divider    = v + 1;
   mp_t term       = sum;
   mp_t eps        = boost::math::tools::epsilon<mp_t>();
   unsigned   m          = 1;

   while (fabs(term / abs_sum) > eps)
   {
      term *= multiplier;
      term /= m;
      term /= divider;
      ++divider;
      ++m;
      sum += term;
      abs_sum += abs(term);
   }
   if (err)
      *err = eps * fabs(abs_sum / sum);

   return sum;
}
//->

/*`
And to then pass our arguments to it, without first altering their precision to match
the current working default.  However, imagine that `calculate_bessel_J_as_series_2`
calculates x[super 2] internally, what precision is the result?  If it's the same as the
precision of /x/, then our calculation will loose precision, since we really want the result
calculated to the full current working precision, which may be significantly higher than
that of our input variables.  Our new version of Bessel_J_to_precision therefore uses
`variable_precision_options::preserve_target_precision` internally, so that expressions
containing only the low-precision input variables are calculated at the precision of
(at least) the target - which will have been constructed at the current working precision.

Here's our revised code:

*/

mp_t Bessel_J_to_precision_2(const mp_t& v, const mp_t& x, unsigned digits10)
{
   //
   // Begin with 2 scoped objects, one to manage current working precision, one to
   // manage mixed precision arithmetic.  Use of variable_precision_options::preserve_target_precision
   // ensures that expressions containing only low-precision input variables are evaluated at the precision
   // of the variable they are being assigned to (ie current working precision).
   //
   scoped_mpfr_precision scoped(digits10);
   scoped_mpfr_precision_options scoped_opts(boost::multiprecision::variable_precision_options::preserve_target_precision);

   mp_t                    result;
   mp_t            current_error{1};
   mp_t            target_error{std::pow(10., -static_cast<int>(digits10))};

   while (target_error < current_error)
   {
      //
      // The assignment here, rounds the high precision result
      // returned by calculate_bessel_J_as_series_2, to the precision
      // of variable result: ie to the target precision we specified in
      // the function call.  This is only the case because we have
      // variable_precision_options::preserve_target_precision set.
      //
      result = calculate_bessel_J_as_series_2(x, v, &current_error);

      digits10 *= 2;
      scoped.reset(digits10);
   }
   //
   // There may be temporaries created when we return, we must make sure
   // that we reset the working precision and options before we return.
   // In the case of the options, we must preserve the precision of the source
   // object during the return, not only here, but in the calling function too:
   //
   scoped.reset();
   scoped_opts.reset(boost::multiprecision::variable_precision_options::preserve_source_precision);
   return result;
}

/*`
In our final example, we'll look at a (somewhat contrived) case where we reduce the argument
by N * PI, in this case we change the mixed-precision arithmetic options several times,
depending what it is we are trying to achieve at that moment in time:

*/
mp_t reduce_n_pi(const mp_t& arg)
{
   //
   // We begin by estimating how many multiples of PI we will be reducing by, 
   // note that this is only an estimate because we're using low precision
   // arithmetic here to get a quick answer:
   //
   unsigned n = static_cast<unsigned>(arg / boost::math::constants::pi<mp_t>());
   //
   // Now that we have an estimate for N, we can up the working precision and obtain
   // a high precision value for PI, best to play safe and preserve the precision of the
   // source here.  Though note that expressions are evaluated at the highest precision
   // of any of their components: in this case that's the current working precision
   // returned by boost::math::constants::pi, and not the precision of arg.
   // However, should this function be called with assume_uniform_precision set
   // then all bets are off unless we do this:
   //
   scoped_mpfr_precision            scope_1(mp_t::thread_default_precision() * 2);
   scoped_mpfr_precision_options    scope_2(boost::multiprecision::variable_precision_options::preserve_source_precision);

   mp_t reduced = arg - n * boost::math::constants::pi<mp_t>();
   //
   // Since N was only an estimate, we may have subtracted one PI too many,
   // correct if that's the case now:
   //
   if (reduced < 0)
      reduced += boost::math::constants::pi<mp_t>();
   //
   // Our variable "reduced" now has the correct answer, but too many digits precision, 
   // we can either call its .precision() member function, or assign to a new variable
   // with variable_precision_options::preserve_target_precision set:
   //
   scope_1.reset();
   scope_2.reset(boost::multiprecision::variable_precision_options::preserve_target_precision);
   mp_t result = reduced;
   //
   // As with previous examples, returning the result may create temporaries, so lets
   // make sure that we preserve the precision of result.  Note that this isn't strictly
   // required if the calling context is always of uniform precision, but we can't be sure 
   // of our calling context:
   //
   scope_2.reset(boost::multiprecision::variable_precision_options::preserve_source_precision);
   return result;
}

/*`
And finally... we need to mention `preserve_component_precision`, `preserve_related_precision` and `preserve_all_precision`.

These form a hierarchy, with each inheriting the properties of those before it.
`preserve_component_precision` is used when dealing with complex or interval numbers, for example if we have:

   mpc_complex val = some_expression;

And `some_expression` contains scalar values of type `mpfr_float`, then the precision of these is ignored unless
we specify at least `preserve_component_precision`.

`preserve_related_precision` we'll somewhat skip over - it extends the range of types whose precision is
considered within an expression to related types - for example all instantiations of `number<mpfr_float_backend<N>, ET>`
when dealing with expression of type `mpfr_float`.  However, such situations are - and should be -  very rare.

The final option - `preserve_all_precision` - is used to preserve the precision of all the types in an expression, 
for example:

   // calculate 2^1000 - 1:
   mpz_int i(1);
   i <<= 1000;
   i -= 1;

   mp_t f = i;

The final assignment above will round the value in /i/ to the current working precision, unless `preserve_all_precision`
is set, in which case /f/ will end up with sufficient precision to store /i/ unchanged.

*/

//]

int main()
{
   mp_t::thread_default_precision(50);
   mp_t x{6541389046624379uLL};
   x /= 562949953421312uLL;
   mp_t v{2};
   mp_t err;
   mp_t J = Bessel_J_to_precision(v, x, 50);

   std::cout << std::setprecision(50) << J << std::endl;
   std::cout << J.precision() << std::endl;

   mp_t expected{"-9.31614245636402072613249153246313221710284959883647822724e-15"};
   std::cout << boost::math::relative_difference(expected, J) << std::endl;
   
   J = Bessel_J_to_precision_2(v, x, 50);
   std::cout << boost::math::relative_difference(expected, J) << std::endl;
   std::cout << J.precision() << std::endl;
   
   std::cout << reduce_n_pi(boost::math::float_next(boost::math::float_next(5 * boost::math::constants::pi<mp_t>()))) << std::endl;

   return 0;
}

