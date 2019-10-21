
///////////////////////////////////////////////////////////////////////////////
// Copyright Christopher Kormanyos 2013 - 2014.
// Copyright John Maddock 2013.
// Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// This work is based on an earlier work:
// "Algorithm 910: A Portable C++ Multiple-Precision System for Special-Function Calculations",
// in ACM TOMS, {VOL 37, ISSUE 4, (February 2011)} (C) ACM, 2011. http://doi.acm.org/10.1145/1916461.1916469
//

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <vector>
#include <boost/math/constants/constants.hpp>
#include <boost/noncopyable.hpp>

//#define USE_CPP_BIN_FLOAT
#define USE_CPP_DEC_FLOAT
//#define USE_MPFR

#if !defined(DIGIT_COUNT)
#define DIGIT_COUNT 100
#endif

#if !defined(BOOST_NO_CXX11_HDR_CHRONO)
  #include <chrono>
  #define STD_CHRONO std::chrono
#else
  #include <boost/chrono.hpp>
  #define STD_CHRONO boost::chrono
#endif

#if defined(USE_CPP_BIN_FLOAT)
  #include <boost/multiprecision/cpp_bin_float.hpp>
  typedef boost::multiprecision::number<boost::multiprecision::cpp_bin_float<DIGIT_COUNT + 10> > mp_type;
#elif defined(USE_CPP_DEC_FLOAT)
  #include <boost/multiprecision/cpp_dec_float.hpp>
  typedef boost::multiprecision::number<boost::multiprecision::cpp_dec_float<DIGIT_COUNT + 10> > mp_type;
#elif defined(USE_MPFR)
  #include <boost/multiprecision/mpfr.hpp>
  typedef boost::multiprecision::number<boost::multiprecision::mpfr_float_backend<DIGIT_COUNT + 10> > mp_type;
#else
  #error no multiprecision floating type is defined
#endif

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

  duration_type elapsed() const
  {
    return (clock_type::now() - m_start);
  }

  void reset()
  {
    m_start = clock_type::now();
  }

private:
  typename clock_type::time_point m_start;
};

namespace my_math
{
  template<class T> T chebyshev_t(const std::int32_t n, const T& x);

  template<class T> T chebyshev_t(const std::uint32_t n, const T& x, std::vector<T>* vp);

  template<class T> bool isneg(const T& x) { return (x < T(0)); }

  template<class T> const T& zero() { static const T value_zero(0); return value_zero; }
  template<class T> const T& one () { static const T value_one (1); return value_one; }
  template<class T> const T& two () { static const T value_two (2); return value_two; }
}

namespace orthogonal_polynomial_series
{
  template<typename T> static inline T orthogonal_polynomial_template(const T& x, const std::uint32_t n, std::vector<T>* const vp = static_cast<std::vector<T>*>(0u))
  {
    // Compute the value of an orthogonal chebyshev polinomial.
    // Use stable upward recursion.

    if(vp != nullptr)
    {
      vp->clear();
      vp->reserve(static_cast<std::size_t>(n + 1u));
    }

    T y0 = my_math::one<T>();

    if(vp != nullptr) { vp->push_back(y0); }

    if(n == static_cast<std::uint32_t>(0u))
    {
      return y0;
    }

    T y1 = x;

    if(vp != nullptr) { vp->push_back(y1); }

    if(n == static_cast<std::uint32_t>(1u))
    {
      return y1;
    }

    T a = my_math::two <T>();
    T b = my_math::zero<T>();
    T c = my_math::one <T>();

    T yk;

    // Calculate higher orders using the recurrence relation.
    // The direction of stability is upward recursion.
    for(std::int32_t k = static_cast<std::int32_t>(2); k <= static_cast<std::int32_t>(n); ++k)
    {
      yk = (((a * x) + b) * y1) - (c * y0);

      y0 = y1;
      y1 = yk;

      if(vp != nullptr) { vp->push_back(yk); }
    }

    return yk;
  }
}

template<class T> T my_math::chebyshev_t(const std::int32_t n, const T& x)
{
  if(my_math::isneg(x))
  {
    const bool b_negate = ((n % static_cast<std::int32_t>(2)) != static_cast<std::int32_t>(0));

    const T y = chebyshev_t(n, -x);

    return (!b_negate ? y : -y);
  }

  if(n < static_cast<std::int32_t>(0))
  {
    const std::int32_t nn = static_cast<std::int32_t>(-n);

    return chebyshev_t(nn, x);
  }
  else
  {
    return orthogonal_polynomial_series::orthogonal_polynomial_template(x, static_cast<std::uint32_t>(n));
  }
}

template<class T> T my_math::chebyshev_t(const std::uint32_t n, const T& x, std::vector<T>* const vp) { return orthogonal_polynomial_series::orthogonal_polynomial_template(x, static_cast<std::int32_t>(n),  vp); }

namespace util
{
  template <class T> float digit_scale()
  {
    const int d = ((std::max)(std::numeric_limits<T>::digits10, 15));
    return static_cast<float>(d) / 300.0F;
  }
}

namespace examples
{
  namespace nr_006
  {
    template<typename T> class hypergeometric_pfq_base : private boost::noncopyable
    {
    public:
      virtual ~hypergeometric_pfq_base() { }

      virtual void ccoef() const = 0;

      virtual T series() const
      {
        using my_math::chebyshev_t;

        // Compute the Chebyshev coefficients.
        // Get the values of the shifted Chebyshev polynomials.
        std::vector<T> chebyshev_t_shifted_values;
        const T z_shifted = ((Z / W) * static_cast<std::int32_t>(2)) - static_cast<std::int32_t>(1);

        chebyshev_t(static_cast<std::uint32_t>(C.size()),
                    z_shifted,
                    &chebyshev_t_shifted_values);

        // Luke: C     ---------- COMPUTE SCALE FACTOR                       ----------
        // Luke: C
        // Luke: C     ---------- SCALE THE COEFFICIENTS                     ----------
        // Luke: C

        // The coefficient scaling is preformed after the Chebyshev summation,
        // and it is carried out with a single division operation.
        bool b_neg = false;

        const T scale = std::accumulate(C.begin(),
                                        C.end(),
                                        T(0),
                                        [&b_neg](T& scale_sum, const T& ck) -> T
                                        {
                                          ((!b_neg) ? (scale_sum += ck) : (scale_sum -= ck));
                                          b_neg = (!b_neg);
                                          return scale_sum;
                                        });

        // Compute the result of the series expansion using unscaled coefficients.
        const T sum = std::inner_product(C.begin(),
                                         C.end(),
                                         chebyshev_t_shifted_values.begin(),
                                         T(0));

        // Return the properly scaled result.
        return sum / scale;
      }

    protected:
      const   T             Z;
      const   T             W;
      mutable std::deque<T> C;

      hypergeometric_pfq_base(const T& z,
                              const T& w) : Z(z),
                                            W(w),
                                            C(0u) { }

      virtual std::int32_t N() const { return static_cast<std::int32_t>(util::digit_scale<T>() * 500.0F); }
    };

    template<typename T> class ccoef4_hypergeometric_0f1 : public hypergeometric_pfq_base<T>
    {
    public:
      ccoef4_hypergeometric_0f1(const T& c,
                                const T& z,
                                const T& w) : hypergeometric_pfq_base<T>(z, w),
                                              CP(c) { }

      virtual ~ccoef4_hypergeometric_0f1() { }

      virtual void ccoef() const
      {
        // See Luke 1977 page 80.
        const std::int32_t N1 = static_cast<std::int32_t>(this->N() + static_cast<std::int32_t>(1));
        const std::int32_t N2 = static_cast<std::int32_t>(this->N() + static_cast<std::int32_t>(2));

        // Luke: C     ---------- START COMPUTING COEFFICIENTS USING         ----------
        // Luke: C     ---------- BACKWARD RECURRENCE SCHEME                 ----------
        // Luke: C
        T A3(0);
        T A2(0);
        T A1(boost::math::tools::root_epsilon<T>());

        hypergeometric_pfq_base<T>::C.resize(1u, A1);

        std::int32_t X1 = N2;

        T C1 = T(1) - CP;

        const T Z1 = T(4) / hypergeometric_pfq_base<T>::W;

        for(std::int32_t k = static_cast<std::int32_t>(0); k < N1; ++k)
        {
          const T DIVFAC = T(1) / X1;

          --X1;

          // The terms have been slightly re-arranged resulting in lower complexity.
          // Parentheses have been added to avoid reliance on operator precedence.
          const T term =   (A2 - ((A3 * DIVFAC) * X1))
                         + ((A2 * X1) * ((1 + (C1 + X1)) * Z1))
                         + ((A1 * X1) * ((DIVFAC - (C1 * Z1)) + (X1 * Z1)));

          hypergeometric_pfq_base<T>::C.push_front(term);

          A3 = A2;
          A2 = A1;
          A1 = hypergeometric_pfq_base<T>::C.front();
        }

        hypergeometric_pfq_base<T>::C.front() /= static_cast<std::int32_t>(2);
      }

    private:
      const T CP;
    };

    template<typename T> class ccoef1_hypergeometric_1f0 : public hypergeometric_pfq_base<T>
    {
    public:
      ccoef1_hypergeometric_1f0(const T& a,
                                const T& z,
                                const T& w) : hypergeometric_pfq_base<T>(z, w),
                                              AP(a) { }

      virtual ~ccoef1_hypergeometric_1f0() { }

      virtual void ccoef() const
      {
        // See Luke 1977 page 67.
        const std::int32_t N1 = static_cast<std::int32_t>(N() + static_cast<std::int32_t>(1));
        const std::int32_t N2 = static_cast<std::int32_t>(N() + static_cast<std::int32_t>(2));

        // Luke: C     ---------- START COMPUTING COEFFICIENTS USING         ----------
        // Luke: C     ---------- BACKWARD RECURRENCE SCHEME                 ----------
        // Luke: C
        T A2(0);
        T A1(boost::math::tools::root_epsilon<T>());

        hypergeometric_pfq_base<T>::C.resize(1u, A1);

        std::int32_t X1 = N2;

        T V1 = T(1) - AP;

        // Here, we have corrected what appears to be an error in Luke's code.

        // Luke's original code listing has:
        //  AFAC = 2 + FOUR/W
        // But it appears as though the correct form is:
        //  AFAC = 2 - FOUR/W.

        const T AFAC = 2 - (T(4) / hypergeometric_pfq_base<T>::W);

        for(std::int32_t k = static_cast<std::int32_t>(0); k < N1; ++k)
        {
          --X1;

          // The terms have been slightly re-arranged resulting in lower complexity.
          // Parentheses have been added to avoid reliance on operator precedence.
          const T term = -(((X1 * AFAC) * A1) + ((X1 + V1) * A2)) / (X1 - V1);

          hypergeometric_pfq_base<T>::C.push_front(term);

          A2 = A1;
          A1 = hypergeometric_pfq_base<T>::C.front();
        }

        hypergeometric_pfq_base<T>::C.front() /= static_cast<std::int32_t>(2);
      }

    private:
      const T AP;

      virtual std::int32_t N() const { return static_cast<std::int32_t>(util::digit_scale<T>() * 1600.0F); }
    };

    template<typename T> class ccoef3_hypergeometric_1f1 : public hypergeometric_pfq_base<T>
    {
    public:
      ccoef3_hypergeometric_1f1(const T& a,
                                const T& c,
                                const T& z,
                                const T& w) : hypergeometric_pfq_base<T>(z, w),
                                              AP(a),
                                              CP(c) { }

      virtual ~ccoef3_hypergeometric_1f1() { }

      virtual void ccoef() const
      {
        // See Luke 1977 page 74.
        const std::int32_t N1 = static_cast<std::int32_t>(this->N() + static_cast<std::int32_t>(1));
        const std::int32_t N2 = static_cast<std::int32_t>(this->N() + static_cast<std::int32_t>(2));

        // Luke: C     ---------- START COMPUTING COEFFICIENTS USING         ----------
        // Luke: C     ---------- BACKWARD RECURRENCE SCHEME                 ----------
        // Luke: C
        T A3(0);
        T A2(0);
        T A1(boost::math::tools::root_epsilon<T>());

        hypergeometric_pfq_base<T>::C.resize(1u, A1);

        std::int32_t X  = N1;
        std::int32_t X1 = N2;

        T XA  =  X + AP;
        T X3A = (X + 3) - AP;

        const T Z1 = T(4) / hypergeometric_pfq_base<T>::W;

        for(std::int32_t k = static_cast<std::int32_t>(0); k < N1; ++k)
        {
          --X;
          --X1;
          --XA;
          --X3A;

          const T X3A_over_X2 = X3A / static_cast<std::int32_t>(X + 2);

          // The terms have been slightly re-arranged resulting in lower complexity.
          // Parentheses have been added to avoid reliance on operator precedence.
          const T PART1 =  A1 * (((X + CP) * Z1) - X3A_over_X2);
          const T PART2 =  A2 * (Z1 * ((X + 3) - CP) + (XA / X1));
          const T PART3 =  A3 * X3A_over_X2;

          const T term = (((PART1 + PART2) + PART3) * X1) / XA;

          hypergeometric_pfq_base<T>::C.push_front(term);

          A3 = A2;
          A2 = A1;
          A1 = hypergeometric_pfq_base<T>::C.front();
        }

        hypergeometric_pfq_base<T>::C.front() /= static_cast<std::int32_t>(2);
      }

    private:
      const T AP;
      const T CP;
    };

    template<typename T> class ccoef6_hypergeometric_1f2 : public hypergeometric_pfq_base<T>
    {
    public:
      ccoef6_hypergeometric_1f2(const T& a,
                                const T& b,
                                const T& c,
                                const T& z,
                                const T& w) : hypergeometric_pfq_base<T>(z, w),
                                              AP(a),
                                              BP(b),
                                              CP(c) { }

      virtual ~ccoef6_hypergeometric_1f2() { }

      virtual void ccoef() const
      {
        // See Luke 1977 page 85.
        const std::int32_t N1 = static_cast<std::int32_t>(this->N() + static_cast<std::int32_t>(1));

        // Luke: C     ---------- START COMPUTING COEFFICIENTS USING         ----------
        // Luke: C     ---------- BACKWARD RECURRENCE SCHEME                 ----------
        // Luke: C
        T A4(0);
        T A3(0);
        T A2(0);
        T A1(boost::math::tools::root_epsilon<T>());

        hypergeometric_pfq_base<T>::C.resize(1u, A1);

        std::int32_t X  = N1;
        T            PP = X + AP;

        const T Z1 = T(4) / hypergeometric_pfq_base<T>::W;

        for(std::int32_t k = static_cast<std::int32_t>(0); k < N1; ++k)
        {
          --X;
          --PP;

          const std::int32_t TWO_X    = static_cast<std::int32_t>(X * 2);
          const std::int32_t X_PLUS_1 = static_cast<std::int32_t>(X + 1);
          const std::int32_t X_PLUS_3 = static_cast<std::int32_t>(X + 3);
          const std::int32_t X_PLUS_4 = static_cast<std::int32_t>(X + 4);

          const T QQ = T(TWO_X + 3) / static_cast<std::int32_t>(TWO_X + static_cast<std::int32_t>(5));
          const T SS = (X + BP) * (X + CP);

          // The terms have been slightly re-arranged resulting in lower complexity.
          // Parentheses have been added to avoid reliance on operator precedence.
          const T PART1 =   A1 * (((PP - (QQ * (PP + 1))) * 2) + (SS * Z1));
          const T PART2 =  (A2 * (X + 2)) * ((((TWO_X + 1) * PP) / X_PLUS_1) - ((QQ * 4) * (PP + 1)) + (((TWO_X + 3) * (PP + 2)) / X_PLUS_3) + ((Z1 * 2) * (SS - (QQ * (X_PLUS_1 + BP)) * (X_PLUS_1 + CP))));
          const T PART3 =   A3 * ((((X_PLUS_3 - AP) - (QQ * (X_PLUS_4 - AP))) * 2) + (((QQ * Z1) * (X_PLUS_4 - BP)) * (X_PLUS_4 - CP)));
          const T PART4 = ((A4 * QQ) * (X_PLUS_4 - AP)) / X_PLUS_3;

          const T term = (((PART1 - PART2) + (PART3 - PART4)) * X_PLUS_1) / PP;

          hypergeometric_pfq_base<T>::C.push_front(term);

          A4 = A3;
          A3 = A2;
          A2 = A1;
          A1 = hypergeometric_pfq_base<T>::C.front();
        }

        hypergeometric_pfq_base<T>::C.front() /= static_cast<std::int32_t>(2);
      }

    private:
      const T AP;
      const T BP;
      const T CP;
    };

    template<typename T> class ccoef2_hypergeometric_2f1 : public hypergeometric_pfq_base<T>
    {
    public:
      ccoef2_hypergeometric_2f1(const T& a,
                                const T& b,
                                const T& c,
                                const T& z,
                                const T& w) : hypergeometric_pfq_base<T>(z, w),
                                              AP(a),
                                              BP(b),
                                              CP(c) { }

      virtual ~ccoef2_hypergeometric_2f1() { }

      virtual void ccoef() const
      {
        // See Luke 1977 page 59.
        const std::int32_t N1 = static_cast<std::int32_t>(N() + static_cast<std::int32_t>(1));
        const std::int32_t N2 = static_cast<std::int32_t>(N() + static_cast<std::int32_t>(2));

        // Luke: C     ---------- START COMPUTING COEFFICIENTS USING         ----------
        // Luke: C     ---------- BACKWARD RECURRENCE SCHEME                 ----------
        // Luke: C
        T A3(0);
        T A2(0);
        T A1(boost::math::tools::root_epsilon<T>());

        hypergeometric_pfq_base<T>::C.resize(1u, A1);

        std::int32_t X  = N1;
        std::int32_t X1 = N2;
        std::int32_t X3 = static_cast<std::int32_t>((X * 2) + 3);

        T X3A = (X + 3) - AP;
        T X3B = (X + 3) - BP;

        const T Z1 = T(4) / hypergeometric_pfq_base<T>::W;

        for(std::int32_t k = static_cast<std::int32_t>(0); k < N1; ++k)
        {
          --X;
          --X1;
          --X3A;
          --X3B;
          X3 -= 2;

          const std::int32_t X_PLUS_2 = static_cast<std::int32_t>(X + 2);

          const T XAB = T(1) / ((X + AP) * (X + BP));

          // The terms have been slightly re-arranged resulting in lower complexity.
          // Parentheses have been added to avoid reliance on operator precedence.
          const T PART1 = (A1 * X1) * (2 - (((AP + X1) * (BP + X1)) * ((T(X3) / X_PLUS_2) * XAB)) + ((CP + X) * (XAB * Z1)));
          const T PART2 = (A2 * XAB) * ((X3A * X3B) - (X3 * ((X3A + X3B) - 1)) + (((3 - CP) + X) * (X1 * Z1)));
          const T PART3 = (A3 * X1) * (X3A / X_PLUS_2) * (X3B * XAB);

          const T term = (PART1 + PART2) - PART3;

          hypergeometric_pfq_base<T>::C.push_front(term);

          A3 = A2;
          A2 = A1;
          A1 = hypergeometric_pfq_base<T>::C.front();
        }

        hypergeometric_pfq_base<T>::C.front() /= static_cast<std::int32_t>(2);
      }

    private:
      const T AP;
      const T BP;
      const T CP;

      virtual std::int32_t N() const { return static_cast<std::int32_t>(util::digit_scale<T>() * 1600.0F); }
    };

    template<class T> T luke_ccoef4_hypergeometric_0f1(const T& a, const T& x);
    template<class T> T luke_ccoef1_hypergeometric_1f0(const T& a, const T& x);
    template<class T> T luke_ccoef3_hypergeometric_1f1(const T& a, const T& b, const T& x);
    template<class T> T luke_ccoef6_hypergeometric_1f2(const T& a, const T& b, const T& c, const T& x);
    template<class T> T luke_ccoef2_hypergeometric_2f1(const T& a, const T& b, const T& c, const T& x);
  }
}

template<class T>
T examples::nr_006::luke_ccoef4_hypergeometric_0f1(const T& a, const T& x)
{
  const ccoef4_hypergeometric_0f1<T> hypergeometric_0f1_object(a, x, T(-20));

  hypergeometric_0f1_object.ccoef();

  return hypergeometric_0f1_object.series();
}

template<class T>
T examples::nr_006::luke_ccoef1_hypergeometric_1f0(const T& a, const T& x)
{
  const ccoef1_hypergeometric_1f0<T> hypergeometric_1f0_object(a, x, T(-20));

  hypergeometric_1f0_object.ccoef();

  return hypergeometric_1f0_object.series();
}

template<class T>
T examples::nr_006::luke_ccoef3_hypergeometric_1f1(const T& a, const T& b, const T& x)
{
  const ccoef3_hypergeometric_1f1<T> hypergeometric_1f1_object(a, b, x, T(-20));

  hypergeometric_1f1_object.ccoef();

  return hypergeometric_1f1_object.series();
}

template<class T>
T examples::nr_006::luke_ccoef6_hypergeometric_1f2(const T& a, const T& b, const T& c, const T& x)
{
  const ccoef6_hypergeometric_1f2<T> hypergeometric_1f2_object(a, b, c, x, T(-20));

  hypergeometric_1f2_object.ccoef();

  return hypergeometric_1f2_object.series();
}

template<class T>
T examples::nr_006::luke_ccoef2_hypergeometric_2f1(const T& a, const T& b, const T& c, const T& x)
{
  const ccoef2_hypergeometric_2f1<T> hypergeometric_2f1_object(a, b, c, x, T(-20));

  hypergeometric_2f1_object.ccoef();

  return hypergeometric_2f1_object.series();
}

int main()
{
  stopwatch<STD_CHRONO::high_resolution_clock> my_stopwatch;
  float total_time = 0.0F;

  std::vector<mp_type> hypergeometric_0f1_results(20U);
  std::vector<mp_type> hypergeometric_1f0_results(20U);
  std::vector<mp_type> hypergeometric_1f1_results(20U);
  std::vector<mp_type> hypergeometric_2f1_results(20U);
  std::vector<mp_type> hypergeometric_1f2_results(20U);

  const mp_type a(mp_type(3) / 7);
  const mp_type b(mp_type(2) / 3);
  const mp_type c(mp_type(1) / 4);

  std::int_least16_t i;

  std::cout << "test hypergeometric_0f1." << std::endl;
  i = 1U;
  my_stopwatch.reset();

  // Generate a table of values of Hypergeometric0F1.
  // Compare with the Mathematica command:
  // Table[N[HypergeometricPFQ[{}, {3/7}, -(i*EulerGamma)], 100], {i, 1, 20, 1}]
  std::for_each(hypergeometric_0f1_results.begin(),
                hypergeometric_0f1_results.end(),
                [&i, &a](mp_type& new_value)
                {
                  const mp_type x(-(boost::math::constants::euler<mp_type>() * i));

                  new_value = examples::nr_006::luke_ccoef4_hypergeometric_0f1(a, x);

                  ++i;
                });

  total_time += STD_CHRONO::duration_cast<STD_CHRONO::duration<float> >(my_stopwatch.elapsed()).count();

  // Print the values of Hypergeometric0F1.
  std::for_each(hypergeometric_0f1_results.begin(),
                hypergeometric_0f1_results.end(),
                [](const mp_type& h)
                {
                  std::cout << std::setprecision(DIGIT_COUNT) << h << std::endl;
                });

  std::cout << "test hypergeometric_1f0." << std::endl;
  i = 1U;
  my_stopwatch.reset();

  // Generate a table of values of Hypergeometric1F0.
  // Compare with the Mathematica command:
  // Table[N[HypergeometricPFQ[{3/7}, {}, -(i*EulerGamma)], 100], {i, 1, 20, 1}]
  std::for_each(hypergeometric_1f0_results.begin(),
                hypergeometric_1f0_results.end(),
                [&i, &a](mp_type& new_value)
                {
                  const mp_type x(-(boost::math::constants::euler<mp_type>() * i));

                  new_value = examples::nr_006::luke_ccoef1_hypergeometric_1f0(a, x);

                  ++i;
                });

  total_time += STD_CHRONO::duration_cast<STD_CHRONO::duration<float> >(my_stopwatch.elapsed()).count();

  // Print the values of Hypergeometric1F0.
  std::for_each(hypergeometric_1f0_results.begin(),
                hypergeometric_1f0_results.end(),
                [](const mp_type& h)
                {
                  std::cout << std::setprecision(DIGIT_COUNT) << h << std::endl;
                });

  std::cout << "test hypergeometric_1f1." << std::endl;
  i = 1U;
  my_stopwatch.reset();

  // Generate a table of values of Hypergeometric1F1.
  // Compare with the Mathematica command:
  // Table[N[HypergeometricPFQ[{3/7}, {2/3}, -(i*EulerGamma)], 100], {i, 1, 20, 1}]
  std::for_each(hypergeometric_1f1_results.begin(),
                hypergeometric_1f1_results.end(),
                [&i, &a, &b](mp_type& new_value)
                {
                  const mp_type x(-(boost::math::constants::euler<mp_type>() * i));

                  new_value = examples::nr_006::luke_ccoef3_hypergeometric_1f1(a, b, x);

                  ++i;
                });

  total_time += STD_CHRONO::duration_cast<STD_CHRONO::duration<float> >(my_stopwatch.elapsed()).count();

  // Print the values of Hypergeometric1F1.
  std::for_each(hypergeometric_1f1_results.begin(),
                hypergeometric_1f1_results.end(),
                [](const mp_type& h)
                {
                  std::cout << std::setprecision(DIGIT_COUNT) << h << std::endl;
                });

  std::cout << "test hypergeometric_1f2." << std::endl;
  i = 1U;
  my_stopwatch.reset();

  // Generate a table of values of Hypergeometric1F2.
  // Compare with the Mathematica command:
  // Table[N[HypergeometricPFQ[{3/7}, {2/3, 1/4}, -(i*EulerGamma)], 100], {i, 1, 20, 1}]
  std::for_each(hypergeometric_1f2_results.begin(),
                hypergeometric_1f2_results.end(),
                [&i, &a, &b, &c](mp_type& new_value)
                {
                  const mp_type x(-(boost::math::constants::euler<mp_type>() * i));

                  new_value = examples::nr_006::luke_ccoef6_hypergeometric_1f2(a, b, c, x);

                  ++i;
                });

  total_time += STD_CHRONO::duration_cast<STD_CHRONO::duration<float> >(my_stopwatch.elapsed()).count();

  // Print the values of Hypergeometric1F2.
  std::for_each(hypergeometric_1f2_results.begin(),
                hypergeometric_1f2_results.end(),
                [](const mp_type& h)
                {
                  std::cout << std::setprecision(DIGIT_COUNT) << h << std::endl;
                });

  std::cout << "test hypergeometric_2f1." << std::endl;
  i = 1U;
  my_stopwatch.reset();

  // Generate a table of values of Hypergeometric2F1.
  // Compare with the Mathematica command:
  // Table[N[HypergeometricPFQ[{3/7, 2/3}, {1/4}, -(i * EulerGamma)], 100], {i, 1, 20, 1}]
  std::for_each(hypergeometric_2f1_results.begin(),
                hypergeometric_2f1_results.end(),
                [&i, &a, &b, &c](mp_type& new_value)
                {
                  const mp_type x(-(boost::math::constants::euler<mp_type>() * i));

                  new_value = examples::nr_006::luke_ccoef2_hypergeometric_2f1(a, b, c, x);

                  ++i;
                });

  total_time += STD_CHRONO::duration_cast<STD_CHRONO::duration<float> >(my_stopwatch.elapsed()).count();

  // Print the values of Hypergeometric2F1.
  std::for_each(hypergeometric_2f1_results.begin(),
                hypergeometric_2f1_results.end(),
                [](const mp_type& h)
                {
                  std::cout << std::setprecision(DIGIT_COUNT) << h << std::endl;
                });

  std::cout << "Total execution time = " << std::setprecision(3) << total_time << "s" << std::endl;
}
