//  (C) Copyright Nick Thompson 2017.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MATH_SPECIAL_CHEBYSHEV_HPP
#define BOOST_MATH_SPECIAL_CHEBYSHEV_HPP
#include <cmath>
#include <boost/math/constants/constants.hpp>

#if (__cplusplus > 201103) || (defined(_CPPLIB_VER) && (_CPPLIB_VER >= 610))
#  define BOOST_MATH_CHEB_USE_STD_ACOSH
#endif

#ifndef BOOST_MATH_CHEB_USE_STD_ACOSH
#  include <boost/math/special_functions/acosh.hpp>
#endif

namespace boost { namespace math {

template<class T1, class T2, class T3>
inline typename tools::promote_args<T1, T2, T3>::type chebyshev_next(T1 const & x, T2 const & Tn, T3 const & Tn_1)
{
    return 2*x*Tn - Tn_1;
}

namespace detail {

template<class Real, bool second>
inline Real chebyshev_imp(unsigned n, Real const & x)
{
#ifdef BOOST_MATH_CHEB_USE_STD_ACOSH
    using std::acosh;
#else
   using boost::math::acosh;
#endif
    using std::cosh;
    using std::pow;
    using std::sqrt;
    Real T0 = 1;
    Real T1;
    if (second)
    {
        if (x > 1 || x < -1)
        {
            Real t = sqrt(x*x -1);
            return static_cast<Real>((pow(x+t, (int)(n+1)) - pow(x-t, (int)(n+1)))/(2*t));
        }
        T1 = 2*x;
    }
    else
    {
        if (x > 1)
        {
            return cosh(n*acosh(x));
        }
        if (x < -1)
        {
            if (n & 1)
            {
                return -cosh(n*acosh(-x));
            }
            else
            {
                return cosh(n*acosh(-x));
            }
        }
        T1 = x;
    }

    if (n == 0)
    {
        return T0;
    }

    unsigned l = 1;
    while(l < n)
    {
       std::swap(T0, T1);
       T1 = boost::math::chebyshev_next(x, T0, T1);
       ++l;
    }
    return T1;
}
} // namespace detail

template <class Real, class Policy>
inline typename tools::promote_args<Real>::type
chebyshev_t(unsigned n, Real const & x, const Policy&)
{
   typedef typename tools::promote_args<Real>::type result_type;
   typedef typename policies::evaluation<result_type, Policy>::type value_type;
   return policies::checked_narrowing_cast<result_type, Policy>(detail::chebyshev_imp<value_type, false>(n, static_cast<value_type>(x)), "boost::math::chebyshev_t<%1%>(unsigned, %1%)");
}

template<class Real>
inline typename tools::promote_args<Real>::type chebyshev_t(unsigned n, Real const & x)
{
    return chebyshev_t(n, x, policies::policy<>());
}

template <class Real, class Policy>
inline typename tools::promote_args<Real>::type
chebyshev_u(unsigned n, Real const & x, const Policy&)
{
   typedef typename tools::promote_args<Real>::type result_type;
   typedef typename policies::evaluation<result_type, Policy>::type value_type;
   return policies::checked_narrowing_cast<result_type, Policy>(detail::chebyshev_imp<value_type, true>(n, static_cast<value_type>(x)), "boost::math::chebyshev_u<%1%>(unsigned, %1%)");
}

template<class Real>
inline typename tools::promote_args<Real>::type chebyshev_u(unsigned n, Real const & x)
{
    return chebyshev_u(n, x, policies::policy<>());
}

template <class Real, class Policy>
inline typename tools::promote_args<Real>::type
chebyshev_t_prime(unsigned n, Real const & x, const Policy&)
{
   typedef typename tools::promote_args<Real>::type result_type;
   typedef typename policies::evaluation<result_type, Policy>::type value_type;
   if (n == 0)
   {
      return result_type(0);
   }
   return policies::checked_narrowing_cast<result_type, Policy>(n * detail::chebyshev_imp<value_type, true>(n - 1, static_cast<value_type>(x)), "boost::math::chebyshev_t_prime<%1%>(unsigned, %1%)");
}

template<class Real>
inline typename tools::promote_args<Real>::type chebyshev_t_prime(unsigned n, Real const & x)
{
   return chebyshev_t_prime(n, x, policies::policy<>());
}

/*
 * This is Algorithm 3.1 of
 * Gil, Amparo, Javier Segura, and Nico M. Temme.
 * Numerical methods for special functions.
 * Society for Industrial and Applied Mathematics, 2007.
 * https://www.siam.org/books/ot99/OT99SampleChapter.pdf
 * However, our definition of c0 differs by a factor of 1/2, as stated in the docs. . .
 */
template<class Real, class T2>
inline Real chebyshev_clenshaw_recurrence(const Real* const c, size_t length, const T2& x)
{
    using boost::math::constants::half;
    if (length < 2)
    {
        if (length == 0)
        {
            return 0;
        }
        return c[0]/2;
    }
    Real b2 = 0;
    Real b1 = c[length -1];
    for(size_t j = length - 2; j >= 1; --j)
    {
        Real tmp = 2*x*b1 - b2 + c[j];
        b2 = b1;
        b1 = tmp;
    }
    return x*b1 - b2 + half<Real>()*c[0];
}


}}
#endif
