///////////////////////////////////////////////////////////////////////////////
//  Copyright 2018 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MULTIPRECISION_MPC_HPP
#define BOOST_MULTIPRECISION_MPC_HPP

#include <boost/multiprecision/number.hpp>
#include <boost/cstdint.hpp>
#include <boost/multiprecision/detail/digits.hpp>
#include <boost/multiprecision/mpfr.hpp>
#include <boost/multiprecision/logged_adaptor.hpp>
#include <boost/functional/hash_fwd.hpp>
#include <mpc.h>
#include <cmath>
#include <algorithm>
#include <complex>

#ifndef BOOST_MULTIPRECISION_MPFI_DEFAULT_PRECISION
#  define BOOST_MULTIPRECISION_MPFI_DEFAULT_PRECISION 20
#endif

namespace boost{
namespace multiprecision{
namespace backends{

template <unsigned digits10>
struct mpc_complex_backend;

} // namespace backends

template <unsigned digits10>
struct number_category<backends::mpc_complex_backend<digits10> > : public mpl::int_<number_kind_complex>{};

namespace backends{

namespace detail{

template <unsigned digits10>
struct mpc_complex_imp
{
#ifdef BOOST_HAS_LONG_LONG
   typedef mpl::list<long, boost::long_long_type>                     signed_types;
   typedef mpl::list<unsigned long, boost::ulong_long_type>   unsigned_types;
#else
   typedef mpl::list<long>                                signed_types;
   typedef mpl::list<unsigned long>                       unsigned_types;
#endif
   typedef mpl::list<double, long double>                 float_types;
   typedef long                                           exponent_type;

   mpc_complex_imp()
   {
      mpc_init2(m_data, multiprecision::detail::digits10_2_2(digits10 ? digits10 : get_default_precision()));
      mpc_set_ui(m_data, 0u, GMP_RNDN);
   }
   mpc_complex_imp(unsigned prec)
   {
      mpc_init2(m_data, prec);
      mpc_set_ui(m_data, 0u, GMP_RNDN);
   }

   mpc_complex_imp(const mpc_complex_imp& o)
   {
      mpc_init2(m_data, multiprecision::detail::digits10_2_2(digits10 ? digits10 : get_default_precision()));
      if(o.m_data[0].re[0]._mpfr_d)
         mpc_set(m_data, o.m_data, GMP_RNDN);
   }
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
   mpc_complex_imp(mpc_complex_imp&& o) BOOST_NOEXCEPT
   {
      m_data[0] = o.m_data[0];
      o.m_data[0].re[0]._mpfr_d = 0;
   }
#endif
   mpc_complex_imp& operator = (const mpc_complex_imp& o)
   {
      if(m_data[0].re[0]._mpfr_d == 0)
         mpc_init2(m_data, multiprecision::detail::digits10_2_2(digits10 ? digits10 : get_default_precision()));
      if(o.m_data[0].re[0]._mpfr_d)
         mpc_set(m_data, o.m_data, GMP_RNDD);
      return *this;
   }
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
   mpc_complex_imp& operator = (mpc_complex_imp&& o) BOOST_NOEXCEPT
   {
      mpc_swap(m_data, o.m_data);
      return *this;
   }
#endif
#ifdef BOOST_HAS_LONG_LONG
#ifdef _MPFR_H_HAVE_INTMAX_T
   mpc_complex_imp& operator = (boost::ulong_long_type i)
   {
      if(m_data[0].re[0]._mpfr_d == 0)
         mpc_init2(m_data, multiprecision::detail::digits10_2_2(digits10 ? digits10 : get_default_precision()));
      mpc_set_uj(data(), i, GMP_RNDD);
      return *this;
   }
   mpc_complex_imp& operator = (boost::long_long_type i)
   {
      if(m_data[0].re[0]._mpfr_d == 0)
         mpc_init2(m_data, multiprecision::detail::digits10_2_2(digits10 ? digits10 : get_default_precision()));
      mpc_set_sj(data(), i, GMP_RNDD);
      return *this;
   }
#else
   mpc_complex_imp& operator = (boost::ulong_long_type i)
   {
      mpfr_float_backend<digits10> f;
      f = i;
      mpc_set_fr(this->data(), f.data(), GMP_RNDN);
      return *this;
   }
   mpc_complex_imp& operator = (boost::long_long_type i)
   {
      mpfr_float_backend<digits10> f;
      f = i;
      mpc_set_fr(this->data(), f.data(), GMP_RNDN);
      return *this;
   }
#endif
#endif
   mpc_complex_imp& operator = (unsigned long i)
   {
      if(m_data[0].re[0]._mpfr_d == 0)
         mpc_init2(m_data, multiprecision::detail::digits10_2_2(digits10 ? digits10 : get_default_precision()));
      mpc_set_ui(m_data, i, GMP_RNDN);
      return *this;
   }
   mpc_complex_imp& operator = (long i)
   {
      if(m_data[0].re[0]._mpfr_d == 0)
         mpc_init2(m_data, multiprecision::detail::digits10_2_2(digits10 ? digits10 : get_default_precision()));
      mpc_set_si(m_data, i, GMP_RNDN);
      return *this;
   }
   mpc_complex_imp& operator = (double d)
   {
      if(m_data[0].re[0]._mpfr_d == 0)
         mpc_init2(m_data, multiprecision::detail::digits10_2_2(digits10 ? digits10 : get_default_precision()));
      mpc_set_d(m_data, d, GMP_RNDN);
      return *this;
   }
   mpc_complex_imp& operator = (long double d)
   {
      if (m_data[0].re[0]._mpfr_d == 0)
         mpc_init2(m_data, multiprecision::detail::digits10_2_2(digits10 ? digits10 : get_default_precision()));
      mpc_set_ld(m_data, d, GMP_RNDN);
      return *this;
   }
   mpc_complex_imp& operator = (mpz_t i)
   {
      if (m_data[0].re[0]._mpfr_d == 0)
         mpc_init2(m_data, multiprecision::detail::digits10_2_2(digits10 ? digits10 : get_default_precision()));
      mpc_set_z(m_data, i, GMP_RNDN);
      return *this;
   }
   mpc_complex_imp& operator = (gmp_int i)
   {
      if (m_data[0].re[0]._mpfr_d == 0)
         mpc_init2(m_data, multiprecision::detail::digits10_2_2(digits10 ? digits10 : get_default_precision()));
      mpc_set_z(m_data, i.data(), GMP_RNDN);
      return *this;
   }
   
   mpc_complex_imp& operator = (const char* s)
   {
      using default_ops::eval_fpclassify;

      if(m_data[0].re[0]._mpfr_d == 0)
         mpc_init2(m_data, multiprecision::detail::digits10_2_2(digits10 ? digits10 : get_default_precision()));

      if(s && (*s == '('))
      {
         mpfr_float_backend<digits10> a, b;
         std::string part;
         const char* p = ++s;
         while(*p && (*p != ',') && (*p != ')'))
            ++p;
         part.assign(s + 1, p);
         a = part.c_str();
         s = p;
         if(*p && (*p != '}'))
         {
            ++p;
            while(*p && (*p != ',') && (*p != ')'))
               ++p;
            part.assign(s + 1, p);
         }
         else
            part.erase();
         b = part.c_str();

         if(eval_fpclassify(a) == (int)FP_NAN)
         {
            mpc_set_fr(this->data(), a.data(), GMP_RNDN);
         }
         else if(eval_fpclassify(b) == (int)FP_NAN)
         {
            mpc_set_fr(this->data(), b.data(), GMP_RNDN);
         }
         else
         {
            mpc_set_fr_fr(m_data, a.data(), b.data(), GMP_RNDN);
         }
      }
      else if(mpc_set_str(m_data, s, 10, GMP_RNDN) != 0)
      {
         BOOST_THROW_EXCEPTION(std::runtime_error(std::string("Unable to parse string \"") + s + std::string("\"as a valid floating point number.")));
      }
      return *this;
   }
   void swap(mpc_complex_imp& o) BOOST_NOEXCEPT
   {
      mpc_swap(m_data, o.m_data);
   }
   std::string str(std::streamsize digits, std::ios_base::fmtflags f)const
   {
      BOOST_ASSERT(m_data[0].re[0]._mpfr_d);

      mpfr_float_backend<digits10> a, b;

      mpc_real(a.data(), m_data, GMP_RNDD);
      mpc_imag(b.data(), m_data, GMP_RNDD);

      if(eval_is_zero(b))
         return a.str(digits, f);

      return "(" + a.str(digits, f) + "," + b.str(digits, f) + ")";
   }
   ~mpc_complex_imp() BOOST_NOEXCEPT
   {
      if(m_data[0].re[0]._mpfr_d)
         mpc_clear(m_data);
   }
   void negate() BOOST_NOEXCEPT
   {
      BOOST_ASSERT(m_data[0].re[0]._mpfr_d);
      mpc_neg(m_data, m_data, GMP_RNDD);
   }
   int compare(const mpc_complex_imp& o)const BOOST_NOEXCEPT
   {
      BOOST_ASSERT(m_data[0].re[0]._mpfr_d && o.m_data[0].re[0]._mpfr_d);
      return mpc_cmp(m_data, o.m_data);
   }
   int compare(long int i)const BOOST_NOEXCEPT
   {
      BOOST_ASSERT(m_data[0].re[0]._mpfr_d);
      return mpc_cmp_si(m_data, i);
   }
   int compare(unsigned long int i)const BOOST_NOEXCEPT
   {
      BOOST_ASSERT(m_data[0].re[0]._mpfr_d);
      static const unsigned long int max_val = (std::numeric_limits<long>::max)();
      if (i > max_val)
      {
         mpc_complex_imp d;
         d = i;
         return compare(d);
      }
      return mpc_cmp_si(m_data, (long)i);
   }
   template <class V>
   int compare(V v)const BOOST_NOEXCEPT
   {
      mpc_complex_imp d;
      d = v;
      return compare(d);
   }
   mpc_t& data() BOOST_NOEXCEPT
   {
      BOOST_ASSERT(m_data[0].re[0]._mpfr_d);
      return m_data;
   }
   const mpc_t& data()const BOOST_NOEXCEPT
   {
      BOOST_ASSERT(m_data[0].re[0]._mpfr_d);
      return m_data;
   }
protected:
   mpc_t m_data;
   static unsigned& get_default_precision() BOOST_NOEXCEPT
   {
      static unsigned val = BOOST_MULTIPRECISION_MPFI_DEFAULT_PRECISION;
      return val;
   }
};

} // namespace detail

template <unsigned digits10>
struct mpc_complex_backend : public detail::mpc_complex_imp<digits10>
{
   mpc_complex_backend() : detail::mpc_complex_imp<digits10>() {}
   mpc_complex_backend(const mpc_complex_backend& o) : detail::mpc_complex_imp<digits10>(o) {}
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
   mpc_complex_backend(mpc_complex_backend&& o) : detail::mpc_complex_imp<digits10>(static_cast<detail::mpc_complex_imp<digits10>&&>(o)) {}
#endif
   template <unsigned D>
   mpc_complex_backend(const mpc_complex_backend<D>& val, typename enable_if_c<D <= digits10>::type* = 0)
       : detail::mpc_complex_imp<digits10>()
   {
      mpc_set(this->m_data, val.data(), GMP_RNDN);
   }
   template <unsigned D>
   explicit mpc_complex_backend(const mpc_complex_backend<D>& val, typename disable_if_c<D <= digits10>::type* = 0)
       : detail::mpc_complex_imp<digits10>()
   {
      mpc_set(this->m_data, val.data(), GMP_RNDN);
   }
   template <unsigned D>
   mpc_complex_backend(const mpfr_float_backend<D>& val, typename enable_if_c<D <= digits10>::type* = 0)
       : detail::mpc_complex_imp<digits10>()
   {
      mpc_set_fr(this->m_data, val.data(), GMP_RNDN);
   }
   template <unsigned D>
   explicit mpc_complex_backend(const mpfr_float_backend<D>& val, typename disable_if_c<D <= digits10>::type* = 0)
       : detail::mpc_complex_imp<digits10>()
   {
      mpc_set(this->m_data, val.data(), GMP_RNDN);
   }
   mpc_complex_backend(const mpc_t val)
       : detail::mpc_complex_imp<digits10>()
   {
      mpc_set(this->m_data, val, GMP_RNDN);
   }
   mpc_complex_backend(const std::complex<float>& val)
       : detail::mpc_complex_imp<digits10>()
   {
      mpc_set_d_d(this->m_data, val.real(), val.imag(), GMP_RNDN);
   }
   mpc_complex_backend(const std::complex<double>& val)
       : detail::mpc_complex_imp<digits10>()
   {
      mpc_set_d_d(this->m_data, val.real(), val.imag(), GMP_RNDN);
   }
   mpc_complex_backend(const std::complex<long double>& val)
       : detail::mpc_complex_imp<digits10>()
   {
      mpc_set_ld_ld(this->m_data, val.real(), val.imag(), GMP_RNDN);
   }
   mpc_complex_backend(mpz_t val) : detail::mpc_complex_imp<digits10>()
   {
      mpc_set_z(this->m_data, val, GMP_RNDN);
   }
   mpc_complex_backend(gmp_int const& val) : detail::mpc_complex_imp<digits10>()
   {
      mpc_set_z(this->m_data, val.data(), GMP_RNDN);
   }
   mpc_complex_backend& operator=(const mpc_complex_backend& o)
   {
      *static_cast<detail::mpc_complex_imp<digits10>*>(this) = static_cast<detail::mpc_complex_imp<digits10> const&>(o);
      return *this;
   }
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
   mpc_complex_backend& operator=(mpc_complex_backend&& o) BOOST_NOEXCEPT
   {
      *static_cast<detail::mpc_complex_imp<digits10>*>(this) = static_cast<detail::mpc_complex_imp<digits10>&&>(o);
      return *this;
   }
#endif
   template <class V>
   mpc_complex_backend& operator=(const V& v)
   {
      *static_cast<detail::mpc_complex_imp<digits10>*>(this) = v;
      return *this;
   }
   mpc_complex_backend& operator=(const mpc_t val)
   {
      mpc_set(this->m_data, val, GMP_RNDN);
      return *this;
   }
   mpc_complex_backend& operator=(const std::complex<float>& val)
   {
      mpc_set_d_d(this->m_data, val.real(), val.imag(), GMP_RNDN);
      return *this;
   }
   mpc_complex_backend& operator=(const std::complex<double>& val)
   {
      mpc_set_d_d(this->m_data, val.real(), val.imag(), GMP_RNDN);
      return *this;
   }
   mpc_complex_backend& operator=(const std::complex<long double>& val)
   {
      mpc_set_ld_ld(this->m_data, val.real(), val.imag(), GMP_RNDN);
      return *this;
   }
   // We don't change our precision here, this is a fixed precision type:
   template <unsigned D>
   mpc_complex_backend& operator=(const mpc_complex_backend<D>& val)
   {
      mpc_set(this->m_data, val.data());
      return *this;
   }
   template <unsigned D>
   mpc_complex_backend& operator=(const mpfr_float_backend<D>& val)
   {
      mpc_set_fr(this->m_data, val.data(), GMP_RNDN);
      return *this;
   }
};

template <>
struct mpc_complex_backend<0> : public detail::mpc_complex_imp<0>
{
   mpc_complex_backend() : detail::mpc_complex_imp<0>() {}
   mpc_complex_backend(const mpc_t val)
      : detail::mpc_complex_imp<0>(mpc_get_prec(val))
   {
      mpc_set(this->m_data, val, GMP_RNDN);
   }
   mpc_complex_backend(const mpc_complex_backend& o) : detail::mpc_complex_imp<0>(o) {}
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
   mpc_complex_backend(mpc_complex_backend&& o) BOOST_NOEXCEPT : detail::mpc_complex_imp<0>(static_cast<detail::mpc_complex_imp<0>&&>(o)) {}
#endif
   mpc_complex_backend(const mpc_complex_backend& o, unsigned digits10)
      : detail::mpc_complex_imp<0>(digits10)
   {
      *this = o;
   }
   template <unsigned D>
   mpc_complex_backend(const mpc_complex_backend<D>& val)
      : detail::mpc_complex_imp<0>(mpc_get_prec(val.data()))
   {
      mpc_set(this->m_data, val.data(), GMP_RNDN);
   }
   template <unsigned D>
   mpc_complex_backend(const mpfr_float_backend<D>& val)
      : detail::mpc_complex_imp<0>(mpfr_get_prec(val.data()))
   {
      mpc_set_fr(this->m_data, val.data(), GMP_RNDN);
   }
   mpc_complex_backend(mpz_t val) : detail::mpc_complex_imp<0>() 
   {
      mpc_set_z(this->m_data, val, GMP_RNDN);
   }
   mpc_complex_backend(gmp_int const& val) : detail::mpc_complex_imp<0>() 
   {
      mpc_set_z(this->m_data, val.data(), GMP_RNDN);
   }
   mpc_complex_backend(const std::complex<float>& val)
      : detail::mpc_complex_imp<0>()
   {
      mpc_set_d_d(this->m_data, val.real(), val.imag(), GMP_RNDN);
   }
   mpc_complex_backend(const std::complex<double>& val)
      : detail::mpc_complex_imp<0>()
   {
      mpc_set_d_d(this->m_data, val.real(), val.imag(), GMP_RNDN);
   }
   mpc_complex_backend(const std::complex<long double>& val)
      : detail::mpc_complex_imp<0>()
   {
      mpc_set_ld_ld(this->m_data, val.real(), val.imag(), GMP_RNDN);
   }

   mpc_complex_backend& operator=(const mpc_complex_backend& o)
   {
      if (this != &o)
      {
         mpc_set_prec(this->m_data, mpc_get_prec(o.data()));
         mpc_set(this->m_data, o.data(), GMP_RNDN);
      }
      return *this;
   }
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
   mpc_complex_backend& operator=(mpc_complex_backend&& o) BOOST_NOEXCEPT
   {
      *static_cast<detail::mpc_complex_imp<0>*>(this) = static_cast<detail::mpc_complex_imp<0> &&>(o);
      return *this;
   }
#endif
   template <class V>
   mpc_complex_backend& operator=(const V& v)
   {
      *static_cast<detail::mpc_complex_imp<0>*>(this) = v;
      return *this;
   }
   mpc_complex_backend& operator=(const mpc_t val)
   {
      mpc_set_prec(this->m_data, mpc_get_prec(val));
      mpc_set(this->m_data, val, GMP_RNDN);
      return *this;
   }
   template <unsigned D>
   mpc_complex_backend& operator=(const mpc_complex_backend<D>& val)
   {
      mpc_set_prec(this->m_data, mpc_get_prec(val.data()));
      mpc_set(this->m_data, val.data(), GMP_RNDN);
      return *this;
   }
   template <unsigned D>
   mpc_complex_backend& operator=(const mpfr_float_backend<D>& val)
   {
      mpc_set_prec(this->m_data, mpfr_get_prec(val.data()));
      mpc_set_fr(this->m_data, val.data(), GMP_RNDN);
      return *this;
   }
   mpc_complex_backend& operator=(const std::complex<float>& val)
   {
      mpc_set_d_d(this->m_data, val.real(), val.imag(), GMP_RNDN);
      return *this;
   }
   mpc_complex_backend& operator=(const std::complex<double>& val)
   {
      mpc_set_d_d(this->m_data, val.real(), val.imag(), GMP_RNDN);
      return *this;
   }
   mpc_complex_backend& operator=(const std::complex<long double>& val)
   {
      mpc_set_ld_ld(this->m_data, val.real(), val.imag(), GMP_RNDN);
      return *this;
   }
   static unsigned default_precision() BOOST_NOEXCEPT
   {
      return get_default_precision();
   }
   static void default_precision(unsigned v) BOOST_NOEXCEPT
   {
      get_default_precision() = v;
   }
   unsigned precision()const BOOST_NOEXCEPT
   {
      return multiprecision::detail::digits2_2_10(mpc_get_prec(this->m_data));
   }
   void precision(unsigned digits10) BOOST_NOEXCEPT
   {
      mpc_set_prec(this->m_data, multiprecision::detail::digits10_2_2((digits10)));
   }
};

template <unsigned digits10, class T>
inline typename enable_if<is_arithmetic<T>, bool>::type eval_eq(const mpc_complex_backend<digits10>& a, const T& b) BOOST_NOEXCEPT
{
   return a.compare(b) == 0;
}
template <unsigned digits10, class T>
inline typename enable_if<is_arithmetic<T>, bool>::type eval_lt(const mpc_complex_backend<digits10>& a, const T& b) BOOST_NOEXCEPT
{
   return a.compare(b) < 0;
}
template <unsigned digits10, class T>
inline typename enable_if<is_arithmetic<T>, bool>::type eval_gt(const mpc_complex_backend<digits10>& a, const T& b) BOOST_NOEXCEPT
{
   return a.compare(b) > 0;
}

template <unsigned D1, unsigned D2>
inline void eval_add(mpc_complex_backend<D1>& result, const mpc_complex_backend<D2>& o)
{
   mpc_add(result.data(), result.data(), o.data(), GMP_RNDD);
}
template <unsigned D1, unsigned D2>
inline void eval_add(mpc_complex_backend<D1>& result, const mpfr_float_backend<D2>& o)
{
   mpc_add_fr(result.data(), result.data(), o.data(), GMP_RNDD);
}
template <unsigned D1, unsigned D2>
inline void eval_subtract(mpc_complex_backend<D1>& result, const mpc_complex_backend<D2>& o)
{
   mpc_sub(result.data(), result.data(), o.data(), GMP_RNDD);
}
template <unsigned D1, unsigned D2>
inline void eval_subtract(mpc_complex_backend<D1>& result, const mpfr_float_backend<D2>& o)
{
   mpc_sub_fr(result.data(), result.data(), o.data(), GMP_RNDD);
}
template <unsigned D1, unsigned D2>
inline void eval_multiply(mpc_complex_backend<D1>& result, const mpc_complex_backend<D2>& o)
{
   if((void*)&result == (void*)&o)
      mpc_sqr(result.data(), o.data(), GMP_RNDN);
   else
      mpc_mul(result.data(), result.data(), o.data(), GMP_RNDN);
}
template <unsigned D1, unsigned D2>
inline void eval_multiply(mpc_complex_backend<D1>& result, const mpfr_float_backend<D2>& o)
{
   mpc_mul_fr(result.data(), result.data(), o.data(), GMP_RNDN);
}
template <unsigned D1, unsigned D2>
inline void eval_divide(mpc_complex_backend<D1>& result, const mpc_complex_backend<D2>& o)
{
   mpc_div(result.data(), result.data(), o.data(), GMP_RNDD);
}
template <unsigned D1, unsigned D2>
inline void eval_divide(mpc_complex_backend<D1>& result, const mpfr_float_backend<D2>& o)
{
   mpc_div_fr(result.data(), result.data(), o.data(), GMP_RNDD);
}
template <unsigned digits10>
inline void eval_add(mpc_complex_backend<digits10>& result, unsigned long i)
{
   mpc_add_ui(result.data(), result.data(), i, GMP_RNDN);
}
template <unsigned digits10>
inline void eval_subtract(mpc_complex_backend<digits10>& result, unsigned long i)
{
   mpc_sub_ui(result.data(), result.data(), i, GMP_RNDN);
}
template <unsigned digits10>
inline void eval_multiply(mpc_complex_backend<digits10>& result, unsigned long i)
{
   mpc_mul_ui(result.data(), result.data(), i, GMP_RNDN);
}
template <unsigned digits10>
inline void eval_divide(mpc_complex_backend<digits10>& result, unsigned long i)
{
   mpc_div_ui(result.data(), result.data(), i, GMP_RNDN);
}
template <unsigned digits10>
inline void eval_add(mpc_complex_backend<digits10>& result, long i)
{
   if(i > 0)
      mpc_add_ui(result.data(), result.data(), i, GMP_RNDN);
   else
      mpc_sub_ui(result.data(), result.data(), boost::multiprecision::detail::unsigned_abs(i), GMP_RNDN);
}
template <unsigned digits10>
inline void eval_subtract(mpc_complex_backend<digits10>& result, long i)
{
   if(i > 0)
      mpc_sub_ui(result.data(), result.data(), i, GMP_RNDN);
   else
      mpc_add_ui(result.data(), result.data(), boost::multiprecision::detail::unsigned_abs(i), GMP_RNDN);
}
template <unsigned digits10>
inline void eval_multiply(mpc_complex_backend<digits10>& result, long i)
{
   mpc_mul_ui(result.data(), result.data(), boost::multiprecision::detail::unsigned_abs(i), GMP_RNDN);
   if(i < 0)
      mpc_neg(result.data(), result.data(), GMP_RNDN);
}
template <unsigned digits10>
inline void eval_divide(mpc_complex_backend<digits10>& result, long i)
{
   mpc_div_ui(result.data(), result.data(), boost::multiprecision::detail::unsigned_abs(i), GMP_RNDN);
   if(i < 0)
      mpc_neg(result.data(), result.data(), GMP_RNDN);
}
//
// Specialised 3 arg versions of the basic operators:
//
template <unsigned D1, unsigned D2, unsigned D3>
inline void eval_add(mpc_complex_backend<D1>& a, const mpc_complex_backend<D2>& x, const mpc_complex_backend<D3>& y)
{
   mpc_add(a.data(), x.data(), y.data(), GMP_RNDD);
}
template <unsigned D1, unsigned D2, unsigned D3>
inline void eval_add(mpc_complex_backend<D1>& a, const mpc_complex_backend<D2>& x, const mpfr_float_backend<D3>& y)
{
   mpc_add_fr(a.data(), x.data(), y.data(), GMP_RNDD);
}
template <unsigned D1, unsigned D2, unsigned D3>
inline void eval_add(mpc_complex_backend<D1>& a, const mpfr_float_backend<D2>& x, const mpc_complex_backend<D3>& y)
{
   mpc_add_fr(a.data(), y.data(), x.data(), GMP_RNDD);
}
template <unsigned D1, unsigned D2>
inline void eval_add(mpc_complex_backend<D1>& a, const mpc_complex_backend<D2>& x, unsigned long y)
{
   mpc_add_ui(a.data(), x.data(), y, GMP_RNDD);
}
template <unsigned D1, unsigned D2>
inline void eval_add(mpc_complex_backend<D1>& a, const mpc_complex_backend<D2>& x, long y)
{
   if(y < 0)
      mpc_sub_ui(a.data(), x.data(), boost::multiprecision::detail::unsigned_abs(y), GMP_RNDD);
   else
      mpc_add_ui(a.data(), x.data(), y, GMP_RNDD);
}
template <unsigned D1, unsigned D2>
inline void eval_add(mpc_complex_backend<D1>& a, unsigned long x, const mpc_complex_backend<D2>& y)
{
   mpc_add_ui(a.data(), y.data(), x, GMP_RNDD);
}
template <unsigned D1, unsigned D2>
inline void eval_add(mpc_complex_backend<D1>& a, long x, const mpc_complex_backend<D2>& y)
{
   if(x < 0)
   {
      mpc_ui_sub(a.data(), boost::multiprecision::detail::unsigned_abs(x), y.data(), GMP_RNDN);
      mpc_neg(a.data(), a.data(), GMP_RNDD);
   }
   else
      mpc_add_ui(a.data(), y.data(), x, GMP_RNDD);
}
template <unsigned D1, unsigned D2, unsigned D3>
inline void eval_subtract(mpc_complex_backend<D1>& a, const mpc_complex_backend<D2>& x, const mpc_complex_backend<D3>& y)
{
   mpc_sub(a.data(), x.data(), y.data(), GMP_RNDD);
}
template <unsigned D1, unsigned D2, unsigned D3>
inline void eval_subtract(mpc_complex_backend<D1>& a, const mpc_complex_backend<D2>& x, const mpfr_float_backend<D3>& y)
{
   mpc_sub_fr(a.data(), x.data(), y.data(), GMP_RNDD);
}
template <unsigned D1, unsigned D2, unsigned D3>
inline void eval_subtract(mpc_complex_backend<D1>& a, const mpfr_float_backend<D2>& x, const mpc_complex_backend<D3>& y)
{
   mpc_fr_sub(a.data(), x.data(), y.data(), GMP_RNDD);
}
template <unsigned D1, unsigned D2>
inline void eval_subtract(mpc_complex_backend<D1>& a, const mpc_complex_backend<D2>& x, unsigned long y)
{
   mpc_sub_ui(a.data(), x.data(), y, GMP_RNDD);
}
template <unsigned D1, unsigned D2>
inline void eval_subtract(mpc_complex_backend<D1>& a, const mpc_complex_backend<D2>& x, long y)
{
   if(y < 0)
      mpc_add_ui(a.data(), x.data(), boost::multiprecision::detail::unsigned_abs(y), GMP_RNDD);
   else
      mpc_sub_ui(a.data(), x.data(), y, GMP_RNDD);
}
template <unsigned D1, unsigned D2>
inline void eval_subtract(mpc_complex_backend<D1>& a, unsigned long x, const mpc_complex_backend<D2>& y)
{
   mpc_ui_sub(a.data(), x, y.data(), GMP_RNDN);
}
template <unsigned D1, unsigned D2>
inline void eval_subtract(mpc_complex_backend<D1>& a, long x, const mpc_complex_backend<D2>& y)
{
   if(x < 0)
   {
      mpc_add_ui(a.data(), y.data(), boost::multiprecision::detail::unsigned_abs(x), GMP_RNDD);
      mpc_neg(a.data(), a.data(), GMP_RNDD);
   }
   else
      mpc_ui_sub(a.data(), x, y.data(), GMP_RNDN);
}

template <unsigned D1, unsigned D2, unsigned D3>
inline void eval_multiply(mpc_complex_backend<D1>& a, const mpc_complex_backend<D2>& x, const mpc_complex_backend<D3>& y)
{
   if((void*)&x == (void*)&y)
      mpc_sqr(a.data(), x.data(), GMP_RNDD);
   else
      mpc_mul(a.data(), x.data(), y.data(), GMP_RNDD);
}
template <unsigned D1, unsigned D2, unsigned D3>
inline void eval_multiply(mpc_complex_backend<D1>& a, const mpc_complex_backend<D2>& x, const mpfr_float_backend<D3>& y)
{
   mpc_mul_fr(a.data(), x.data(), y.data(), GMP_RNDD);
}
template <unsigned D1, unsigned D2, unsigned D3>
inline void eval_multiply(mpc_complex_backend<D1>& a, const mpfr_float_backend<D2>& x, const mpc_complex_backend<D3>& y)
{
   mpc_mul_fr(a.data(), y.data(), x.data(), GMP_RNDD);
}
template <unsigned D1, unsigned D2>
inline void eval_multiply(mpc_complex_backend<D1>& a, const mpc_complex_backend<D2>& x, unsigned long y)
{
   mpc_mul_ui(a.data(), x.data(), y, GMP_RNDD);
}
template <unsigned D1, unsigned D2>
inline void eval_multiply(mpc_complex_backend<D1>& a, const mpc_complex_backend<D2>& x, long y)
{
   if(y < 0)
   {
      mpc_mul_ui(a.data(), x.data(), boost::multiprecision::detail::unsigned_abs(y), GMP_RNDD);
      a.negate();
   }
   else
      mpc_mul_ui(a.data(), x.data(), y, GMP_RNDD);
}
template <unsigned D1, unsigned D2>
inline void eval_multiply(mpc_complex_backend<D1>& a, unsigned long x, const mpc_complex_backend<D2>& y)
{
   mpc_mul_ui(a.data(), y.data(), x, GMP_RNDD);
}
template <unsigned D1, unsigned D2>
inline void eval_multiply(mpc_complex_backend<D1>& a, long x, const mpc_complex_backend<D2>& y)
{
   if(x < 0)
   {
      mpc_mul_ui(a.data(), y.data(), boost::multiprecision::detail::unsigned_abs(x), GMP_RNDD);
      mpc_neg(a.data(), a.data(), GMP_RNDD);
   }
   else
      mpc_mul_ui(a.data(), y.data(), x, GMP_RNDD);
}

template <unsigned D1, unsigned D2, unsigned D3>
inline void eval_divide(mpc_complex_backend<D1>& a, const mpc_complex_backend<D2>& x, const mpc_complex_backend<D3>& y)
{
   mpc_div(a.data(), x.data(), y.data(), GMP_RNDD);
}
template <unsigned D1, unsigned D2, unsigned D3>
inline void eval_divide(mpc_complex_backend<D1>& a, const mpc_complex_backend<D2>& x, const mpfr_float_backend<D3>& y)
{
   mpc_div_fr(a.data(), x.data(), y.data(), GMP_RNDD);
}
template <unsigned D1, unsigned D2, unsigned D3>
inline void eval_divide(mpc_complex_backend<D1>& a, const mpfr_float_backend<D2>& x, const mpc_complex_backend<D3>& y)
{
   mpc_fr_div(a.data(), x.data(), y.data(), GMP_RNDD);
}
template <unsigned D1, unsigned D2>
inline void eval_divide(mpc_complex_backend<D1>& a, const mpc_complex_backend<D2>& x, unsigned long y)
{
   mpc_div_ui(a.data(), x.data(), y, GMP_RNDD);
}
template <unsigned D1, unsigned D2>
inline void eval_divide(mpc_complex_backend<D1>& a, const mpc_complex_backend<D2>& x, long y)
{
   if(y < 0)
   {
      mpc_div_ui(a.data(), x.data(), boost::multiprecision::detail::unsigned_abs(y), GMP_RNDD);
      a.negate();
   }
   else
      mpc_div_ui(a.data(), x.data(), y, GMP_RNDD);
}
template <unsigned D1, unsigned D2>
inline void eval_divide(mpc_complex_backend<D1>& a, unsigned long x, const mpc_complex_backend<D2>& y)
{
   mpc_ui_div(a.data(), x, y.data(), GMP_RNDD);
}
template <unsigned D1, unsigned D2>
inline void eval_divide(mpc_complex_backend<D1>& a, long x, const mpc_complex_backend<D2>& y)
{
   if(x < 0)
   {
      mpc_ui_div(a.data(), boost::multiprecision::detail::unsigned_abs(x), y.data(), GMP_RNDD);
      mpc_neg(a.data(), a.data(), GMP_RNDD);
   }
   else
      mpc_ui_div(a.data(), x, y.data(), GMP_RNDD);
}

template <unsigned digits10>
inline bool eval_is_zero(const mpc_complex_backend<digits10>& val) BOOST_NOEXCEPT
{
   return (0 != mpfr_zero_p(mpc_realref(val.data()))) && (0 != mpfr_zero_p(mpc_imagref(val.data())));
}
template <unsigned digits10>
inline int eval_get_sign(const mpc_complex_backend<digits10>&)
{
   BOOST_STATIC_ASSERT_MSG(digits10 == UINT_MAX, "Complex numbers have no sign bit."); // designed to always fail
   return 0;
}

template <unsigned digits10>
inline void eval_convert_to(unsigned long* result, const mpc_complex_backend<digits10>& val)
{
   if (0 == mpfr_zero_p(mpc_imagref(val.data())))
   {
      BOOST_THROW_EXCEPTION(std::runtime_error("Could not convert imaginary number to scalar."));
   }
   mpfr_float_backend<digits10> t;
   mpc_real(t.data(), val.data(), GMP_RNDN);
   eval_convert_to(result, t);
}
template <unsigned digits10>
inline void eval_convert_to(long* result, const mpc_complex_backend<digits10>& val)
{
   if (0 == mpfr_zero_p(mpc_imagref(val.data())))
   {
      BOOST_THROW_EXCEPTION(std::runtime_error("Could not convert imaginary number to scalar."));
   }
   mpfr_float_backend<digits10> t;
   mpc_real(t.data(), val.data(), GMP_RNDN);
   eval_convert_to(result, t);
}
#ifdef _MPFR_H_HAVE_INTMAX_T
template <unsigned digits10>
inline void eval_convert_to(boost::ulong_long_type* result, const mpc_complex_backend<digits10>& val)
{
   if (0 == mpfr_zero_p(mpc_imagref(val.data())))
   {
      BOOST_THROW_EXCEPTION(std::runtime_error("Could not convert imaginary number to scalar."));
   }
   mpfr_float_backend<digits10> t;
   mpc_real(t.data(), val.data(), GMP_RNDN);
   eval_convert_to(result, t);
}
template <unsigned digits10>
inline void eval_convert_to(boost::long_long_type* result, const mpc_complex_backend<digits10>& val)
{
   if (0 == mpfr_zero_p(mpc_imagref(val.data())))
   {
      BOOST_THROW_EXCEPTION(std::runtime_error("Could not convert imaginary number to scalar."));
   }
   mpfr_float_backend<digits10> t;
   mpc_real(t.data(), val.data(), GMP_RNDN);
   eval_convert_to(result, t);
}
#endif
template <unsigned digits10>
inline void eval_convert_to(double* result, const mpc_complex_backend<digits10>& val) BOOST_NOEXCEPT
{
   if (0 == mpfr_zero_p(mpc_imagref(val.data())))
   {
      BOOST_THROW_EXCEPTION(std::runtime_error("Could not convert imaginary number to scalar."));
   }
   mpfr_float_backend<digits10> t;
   mpc_real(t.data(), val.data(), GMP_RNDN);
   eval_convert_to(result, t);
}
template <unsigned digits10>
inline void eval_convert_to(long double* result, const mpc_complex_backend<digits10>& val) BOOST_NOEXCEPT
{
   if (0 == mpfr_zero_p(mpc_imagref(val.data())))
   {
      BOOST_THROW_EXCEPTION(std::runtime_error("Could not convert imaginary number to scalar."));
   }
   mpfr_float_backend<digits10> t;
   mpc_real(t.data(), val.data(), GMP_RNDN);
   eval_convert_to(result, t);
}

template <unsigned D1, unsigned D2, mpfr_allocation_type AllocationType>
inline void assign_components(mpc_complex_backend<D1>& result, const mpfr_float_backend<D2, AllocationType>& a, const mpfr_float_backend<D2, AllocationType>& b)
{
   using default_ops::eval_fpclassify;
   if(eval_fpclassify(a) == (int)FP_NAN)
   {
      mpc_set_fr(result.data(), a.data(), GMP_RNDN);
   }
   else if(eval_fpclassify(b) == (int)FP_NAN)
   {
      mpc_set_fr(result.data(), b.data(), GMP_RNDN);
   }
   else
   {
      mpc_set_fr_fr(result.data(), a.data(), b.data(), GMP_RNDN);
   }
}

template <unsigned D1, unsigned D2, mpfr_allocation_type AllocationType>
inline void assign_components(mpc_complex_backend<D1>& result, unsigned long a, unsigned long b)
{
   mpc_set_ui_ui(result.data(), a, b, GMP_RNDN);
}

template <unsigned D1, unsigned D2, mpfr_allocation_type AllocationType>
inline void assign_components(mpc_complex_backend<D1>& result, long a, long b)
{
   mpc_set_si_si(result.data(), a, b, GMP_RNDN);
}

#if defined(BOOST_HAS_LONG_LONG) && defined(_MPFR_H_HAVE_INTMAX_T)
template <unsigned D1, unsigned D2, mpfr_allocation_type AllocationType>
inline void assign_components(mpc_complex_backend<D1>& result, unsigned long long a, unsigned long long b)
{
   mpc_set_uj_uj(result.data(), a, b, GMP_RNDN);
}

template <unsigned D1, unsigned D2, mpfr_allocation_type AllocationType>
inline void assign_components(mpc_complex_backend<D1>& result, long long a, long long b)
{
   mpc_set_sj_sj(result.data(), a, b, GMP_RNDN);
}
#endif

template <unsigned D1, unsigned D2, mpfr_allocation_type AllocationType>
inline void assign_components(mpc_complex_backend<D1>& result, double a, double b)
{
   if ((boost::math::isnan)(a))
   {
      mpc_set_d(result.data(), a, GMP_RNDN);
   }
   else if ((boost::math::isnan)(b))
   {
      mpc_set_d(result.data(), b, GMP_RNDN);
   }
   else
   {
      mpc_set_d_d(result.data(), a, b, GMP_RNDN);
   }
}

template <unsigned D1, unsigned D2, mpfr_allocation_type AllocationType>
inline void assign_components(mpc_complex_backend<D1>& result, long double a, long double b)
{
   if ((boost::math::isnan)(a))
   {
      mpc_set_d(result.data(), a, GMP_RNDN);
   }
   else if ((boost::math::isnan)(b))
   {
      mpc_set_d(result.data(), b, GMP_RNDN);
   }
   else
   {
      mpc_set_ld_ld(result.data(), a, b, GMP_RNDN);
   }
}

//
// Native non-member operations:
//
template <unsigned Digits10>
inline void eval_sqrt(mpc_complex_backend<Digits10>& result, const mpc_complex_backend<Digits10>& val)
{
   mpc_sqrt(result.data(), val.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_pow(mpc_complex_backend<Digits10>& result, const mpc_complex_backend<Digits10>& b, const mpc_complex_backend<Digits10>& e)
{
   mpc_pow(result.data(), b.data(), e.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_exp(mpc_complex_backend<Digits10>& result, const mpc_complex_backend<Digits10>& arg)
{
   mpc_exp(result.data(), arg.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_log(mpc_complex_backend<Digits10>& result, const mpc_complex_backend<Digits10>& arg)
{
   mpc_log(result.data(), arg.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_log10(mpc_complex_backend<Digits10>& result, const mpc_complex_backend<Digits10>& arg)
{
   mpc_log10(result.data(), arg.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_sin(mpc_complex_backend<Digits10>& result, const mpc_complex_backend<Digits10>& arg)
{
   mpc_sin(result.data(), arg.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_cos(mpc_complex_backend<Digits10>& result, const mpc_complex_backend<Digits10>& arg)
{
   mpc_cos(result.data(), arg.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_tan(mpc_complex_backend<Digits10>& result, const mpc_complex_backend<Digits10>& arg)
{
   mpc_tan(result.data(), arg.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_asin(mpc_complex_backend<Digits10>& result, const mpc_complex_backend<Digits10>& arg)
{
   mpc_asin(result.data(), arg.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_acos(mpc_complex_backend<Digits10>& result, const mpc_complex_backend<Digits10>& arg)
{
   mpc_acos(result.data(), arg.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_atan(mpc_complex_backend<Digits10>& result, const mpc_complex_backend<Digits10>& arg)
{
   mpc_atan(result.data(), arg.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_sinh(mpc_complex_backend<Digits10>& result, const mpc_complex_backend<Digits10>& arg)
{
   mpc_sinh(result.data(), arg.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_cosh(mpc_complex_backend<Digits10>& result, const mpc_complex_backend<Digits10>& arg)
{
   mpc_cosh(result.data(), arg.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_tanh(mpc_complex_backend<Digits10>& result, const mpc_complex_backend<Digits10>& arg)
{
   mpc_tanh(result.data(), arg.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_asinh(mpc_complex_backend<Digits10>& result, const mpc_complex_backend<Digits10>& arg)
{
   mpc_asinh(result.data(), arg.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_acosh(mpc_complex_backend<Digits10>& result, const mpc_complex_backend<Digits10>& arg)
{
   mpc_acosh(result.data(), arg.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_atanh(mpc_complex_backend<Digits10>& result, const mpc_complex_backend<Digits10>& arg)
{
   mpc_atanh(result.data(), arg.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_conj(mpc_complex_backend<Digits10>& result, const mpc_complex_backend<Digits10>& arg)
{
   mpc_conj(result.data(), arg.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_proj(mpc_complex_backend<Digits10>& result, const mpc_complex_backend<Digits10>& arg)
{
   mpc_proj(result.data(), arg.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_real(mpfr_float_backend<Digits10>& result, const mpc_complex_backend<Digits10>& arg)
{
   mpc_real(result.data(), arg.data(), GMP_RNDN);
}
template <unsigned Digits10>
inline void eval_imag(mpfr_float_backend<Digits10>& result, const mpc_complex_backend<Digits10>& arg)
{
   mpc_imag(result.data(), arg.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_set_imag(mpc_complex_backend<Digits10>& result, const mpfr_float_backend<Digits10>& arg)
{
   mpfr_set(mpc_imagref(result.data()), arg.data(), GMP_RNDN);
}

template <unsigned Digits10>
inline void eval_set_real(mpc_complex_backend<Digits10>& result, const mpfr_float_backend<Digits10>& arg)
{
   mpfr_set(mpc_realref(result.data()), arg.data(), GMP_RNDN);
}
template <unsigned Digits10>
inline void eval_set_real(mpc_complex_backend<Digits10>& result, const gmp_int& arg)
{
   mpfr_set_z(mpc_realref(result.data()), arg.data(), GMP_RNDN);
}
template <unsigned Digits10>
inline void eval_set_real(mpc_complex_backend<Digits10>& result, const gmp_rational& arg)
{
   mpfr_set_q(mpc_realref(result.data()), arg.data(), GMP_RNDN);
}
template <unsigned Digits10>
inline void eval_set_real(mpc_complex_backend<Digits10>& result, const unsigned& arg)
{
   mpfr_set_ui(mpc_realref(result.data()), arg, GMP_RNDN);
}
template <unsigned Digits10>
inline void eval_set_real(mpc_complex_backend<Digits10>& result, const unsigned long& arg)
{
   mpfr_set_ui(mpc_realref(result.data()), arg, GMP_RNDN);
}
template <unsigned Digits10>
inline void eval_set_real(mpc_complex_backend<Digits10>& result, const int& arg)
{
   mpfr_set_si(mpc_realref(result.data()), arg, GMP_RNDN);
}
template <unsigned Digits10>
inline void eval_set_real(mpc_complex_backend<Digits10>& result, const long& arg)
{
   mpfr_set_si(mpc_realref(result.data()), arg, GMP_RNDN);
}
template <unsigned Digits10>
inline void eval_set_real(mpc_complex_backend<Digits10>& result, const float& arg)
{
   mpfr_set_flt(mpc_realref(result.data()), arg, GMP_RNDN);
}
template <unsigned Digits10>
inline void eval_set_real(mpc_complex_backend<Digits10>& result, const double& arg)
{
   mpfr_set_d(mpc_realref(result.data()), arg, GMP_RNDN);
}
template <unsigned Digits10>
inline void eval_set_real(mpc_complex_backend<Digits10>& result, const long double& arg)
{
   mpfr_set_ld(mpc_realref(result.data()), arg, GMP_RNDN);
}
#if defined(BOOST_HAS_LONG_LONG) && defined(_MPFR_H_HAVE_INTMAX_T)
template <unsigned Digits10>
inline void eval_set_real(mpc_complex_backend<Digits10>& result, const unsigned long long& arg)
{
   mpfr_set_uj(mpc_realref(result.data()), arg, GMP_RNDN);
}
template <unsigned Digits10>
inline void eval_set_real(mpc_complex_backend<Digits10>& result, const long long& arg)
{
   mpfr_set_sj(mpc_realref(result.data()), arg, GMP_RNDN);
}
#endif

template <unsigned Digits10>
inline void eval_set_imag(mpc_complex_backend<Digits10>& result, const gmp_int& arg)
{
   mpfr_set_z(mpc_imagref(result.data()), arg.data(), GMP_RNDN);
}
template <unsigned Digits10>
inline void eval_set_imag(mpc_complex_backend<Digits10>& result, const gmp_rational& arg)
{
   mpfr_set_q(mpc_imagref(result.data()), arg.data(), GMP_RNDN);
}
template <unsigned Digits10>
inline void eval_set_imag(mpc_complex_backend<Digits10>& result, const unsigned& arg)
{
   mpfr_set_ui(mpc_imagref(result.data()), arg, GMP_RNDN);
}
template <unsigned Digits10>
inline void eval_set_imag(mpc_complex_backend<Digits10>& result, const unsigned long& arg)
{
   mpfr_set_ui(mpc_imagref(result.data()), arg, GMP_RNDN);
}
template <unsigned Digits10>
inline void eval_set_imag(mpc_complex_backend<Digits10>& result, const int& arg)
{
   mpfr_set_si(mpc_imagref(result.data()), arg, GMP_RNDN);
}
template <unsigned Digits10>
inline void eval_set_imag(mpc_complex_backend<Digits10>& result, const long& arg)
{
   mpfr_set_si(mpc_imagref(result.data()), arg, GMP_RNDN);
}
template <unsigned Digits10>
inline void eval_set_imag(mpc_complex_backend<Digits10>& result, const float& arg)
{
   mpfr_set_flt(mpc_imagref(result.data()), arg, GMP_RNDN);
}
template <unsigned Digits10>
inline void eval_set_imag(mpc_complex_backend<Digits10>& result, const double& arg)
{
   mpfr_set_d(mpc_imagref(result.data()), arg, GMP_RNDN);
}
template <unsigned Digits10>
inline void eval_set_imag(mpc_complex_backend<Digits10>& result, const long double& arg)
{
   mpfr_set_ld(mpc_imagref(result.data()), arg, GMP_RNDN);
}
#if defined(BOOST_HAS_LONG_LONG) && defined(_MPFR_H_HAVE_INTMAX_T)
template <unsigned Digits10>
inline void eval_set_imag(mpc_complex_backend<Digits10>& result, const unsigned long long& arg)
{
   mpfr_set_uj(mpc_imagref(result.data()), arg, GMP_RNDN);
}
template <unsigned Digits10>
inline void eval_set_imag(mpc_complex_backend<Digits10>& result, const long long& arg)
{
   mpfr_set_sj(mpc_imagref(result.data()), arg, GMP_RNDN);
}
#endif

template <unsigned Digits10>
inline std::size_t hash_value(const mpc_complex_backend<Digits10>& val)
{
   std::size_t result = 0;
   std::size_t len = val.data()[0].re[0]._mpfr_prec / mp_bits_per_limb;
   if(val.data()[0].re[0]._mpfr_prec % mp_bits_per_limb)
      ++len;
   for(std::size_t i = 0; i < len; ++i)
      boost::hash_combine(result, val.data()[0].re[0]._mpfr_d[i]);
   boost::hash_combine(result, val.data()[0].re[0]._mpfr_exp);
   boost::hash_combine(result, val.data()[0].re[0]._mpfr_sign);

   len = val.data()[0].im[0]._mpfr_prec / mp_bits_per_limb;
   if(val.data()[0].im[0]._mpfr_prec % mp_bits_per_limb)
      ++len;
   for(std::size_t i = 0; i < len; ++i)
      boost::hash_combine(result, val.data()[0].im[0]._mpfr_d[i]);
   boost::hash_combine(result, val.data()[0].im[0]._mpfr_exp);
   boost::hash_combine(result, val.data()[0].im[0]._mpfr_sign);
   return result;
}

} // namespace backends

#ifdef BOOST_NO_SFINAE_EXPR

namespace detail{

template<unsigned D1, unsigned D2>
struct is_explicitly_convertible<backends::mpc_complex_backend<D1>, backends::mpc_complex_backend<D2> > : public mpl::true_ {};

}
#endif

template<>
struct number_category<detail::canonical<mpc_t, backends::mpc_complex_backend<0> >::type> : public mpl::int_<number_kind_floating_point>{};

using boost::multiprecision::backends::mpc_complex_backend;

typedef number<mpc_complex_backend<50> >    mpc_complex_50;
typedef number<mpc_complex_backend<100> >   mpc_complex_100;
typedef number<mpc_complex_backend<500> >   mpc_complex_500;
typedef number<mpc_complex_backend<1000> >  mpc_complex_1000;
typedef number<mpc_complex_backend<0> >     mpc_complex;

template <unsigned Digits10, expression_template_option ExpressionTemplates>
struct component_type<number<mpc_complex_backend<Digits10>, ExpressionTemplates> >
{
   typedef number<mpfr_float_backend<Digits10>, ExpressionTemplates> type;
};

template <unsigned Digits10, expression_template_option ExpressionTemplates>
struct component_type<number<logged_adaptor<mpc_complex_backend<Digits10> >, ExpressionTemplates> >
{
   typedef number<mpfr_float_backend<Digits10>, ExpressionTemplates> type;
};

template <unsigned Digits10, expression_template_option ExpressionTemplates>
struct complex_result_from_scalar<number<mpfr_float_backend<Digits10>, ExpressionTemplates> >
{
   typedef number<mpc_complex_backend<Digits10>, ExpressionTemplates> type;
};

} // namespace multiprecision

}  // namespaces

#endif
