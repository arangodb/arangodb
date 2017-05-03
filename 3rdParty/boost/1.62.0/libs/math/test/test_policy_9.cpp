#define BOOST_TEST_MAIN
// Copyright John Maddock 2007.

// Use, modification and distribution are subject to the
// Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/math/policies/policy.hpp>
#include <boost/math/policies/error_handling.hpp>
#include <boost/math/tools/precision.hpp>

void test()
{
   using namespace boost::math::policies;
   using namespace boost;

#if !defined(BOOST_NO_CXX11_CONSTEXPR) && !defined(BOOST_NO_CXX11_AUTO_DECLARATIONS) && !defined(BOOST_MATH_DISABLE_CONSTEXPR)

   constexpr auto p1 = make_policy();
   constexpr auto p2 = make_policy(promote_double<false>());
   constexpr auto p3 = make_policy(domain_error<user_error>(), pole_error<user_error>(), overflow_error<user_error>(), underflow_error<user_error>(),
      denorm_error<user_error>(), evaluation_error<user_error>(), rounding_error<user_error>(), indeterminate_result_error<user_error>());
   constexpr auto p4 = make_policy(promote_float<false>(), promote_double<false>(), assert_undefined<true>(), discrete_quantile<real>(),
      digits10<10>(), max_series_iterations<100>(), max_root_iterations<20>());
   constexpr auto p5 = make_policy(digits2<20>());

   constexpr int d = digits<double, policy<> >() + digits_base10<double, policy<> >();
   constexpr unsigned long s = get_max_series_iterations<policy<> >();
   constexpr unsigned long r = get_max_root_iterations<policy<> >();

   constexpr double ep = get_epsilon<double, policy<> >();
   constexpr double ep2 = get_epsilon<double, policy<digits10<7> > >();

   constexpr auto p6 = make_policy(domain_error<ignore_error>(), pole_error<ignore_error>(), overflow_error<ignore_error>(), underflow_error<ignore_error>(),
      denorm_error<ignore_error>(), evaluation_error<ignore_error>(), rounding_error<ignore_error>(), indeterminate_result_error<ignore_error>());

   constexpr double r1 = raise_domain_error<double>("foo", "Out of range", 0.0, p6);
   constexpr double r2 = raise_pole_error<double>("func", "msg", 0.0, p6);
   constexpr double r3 = raise_overflow_error<double>("func", "msg", p6);
   constexpr double r4 = raise_overflow_error("func", "msg", 0.0, p6);
   constexpr double r5 = raise_underflow_error<double>("func", "msg", p6);
   constexpr double r6 = raise_denorm_error("func", "msg", 0.0, p6);
   constexpr double r7 = raise_evaluation_error("func", "msg", 0.0, p6);
   constexpr float r8 = raise_rounding_error("func", "msg", 0.0, 0.0f, p6);
   constexpr float r9 = raise_indeterminate_result_error("func", "msg", 0.0, 0.0f, p6);

#endif

#ifndef BOOST_NO_CXX11_NOEXCEPT

   static_assert(noexcept(make_policy()), "This expression should be noexcept");
   static_assert(noexcept(make_policy(promote_double<false>())), "This expression should be noexcept");
   static_assert(noexcept(make_policy(domain_error<user_error>(), pole_error<user_error>(), overflow_error<user_error>(), underflow_error<user_error>(),
      denorm_error<user_error>(), evaluation_error<user_error>(), rounding_error<user_error>(), indeterminate_result_error<user_error>())), "This expression should be noexcept");
   static_assert(noexcept(make_policy(promote_float<false>(), promote_double<false>(), assert_undefined<true>(), discrete_quantile<real>(),
      digits10<10>(), max_series_iterations<100>(), max_root_iterations<20>())), "This expression should be noexcept");

   static_assert(noexcept(digits<double, policy<> >() + digits_base10<double, policy<> >()), "This expression should be noexcept");
   static_assert(noexcept(get_max_series_iterations<policy<> >()), "This expression should be noexcept");
   static_assert(noexcept(get_max_root_iterations<policy<> >()), "This expression should be noexcept");

   static_assert(noexcept(get_epsilon<double, policy<> >()), "This expression should be noexcept");

#endif
   
} // BOOST_AUTO_TEST_CASE( test_main )



