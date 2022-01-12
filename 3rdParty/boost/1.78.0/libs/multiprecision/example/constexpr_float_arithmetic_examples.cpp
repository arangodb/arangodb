//  (C) Copyright John Maddock 2019.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Contains Quickbook snippets used by boost/libs/multiprecision/doc/multiprecision.qbk,
// section Literal Types and constexpr Support.

#include <iostream>
#include <boost/config.hpp>

#ifdef BOOST_HAS_FLOAT128
#include <boost/multiprecision/float128.hpp>
#endif

//[constexpr_circle
#include <boost/math/constants/constants.hpp> // For constant pi with full precision of type T.
// using  boost::math::constants::pi;

template <class T>
inline constexpr T circumference(T radius)
{
   return 2 * boost::math::constants::pi<T>() * radius;
}

template <class T>
inline constexpr T area(T radius)
{
   return boost::math::constants::pi<T>() * radius * radius;
}
//] [/constexpr_circle]

template <class T, unsigned Order>
struct const_polynomial
{
 public:
   T data[Order + 1];

 public:
   constexpr const_polynomial(T val = 0) : data{val} {}
   constexpr const_polynomial(const std::initializer_list<T>& init) : data{}
   {
      if (init.size() > Order + 1)
         throw std::range_error("Too many initializers in list!");
      for (unsigned i = 0; i < init.size(); ++i)
         data[i] = init.begin()[i];
   }
   constexpr T& operator[](std::size_t N)
   {
      return data[N];
   }
   constexpr const T& operator[](std::size_t N) const
   {
      return data[N];
   }
   template <class U>
   constexpr T operator()(U val)const
   {
      T result = data[Order];
      for (unsigned i = Order; i > 0; --i)
      {
         result *= val;
         result += data[i - 1];
      }
      return result;
   }
   constexpr const_polynomial<T, Order - 1> derivative() const
   {
      const_polynomial<T, Order - 1> result;
      for (unsigned i = 1; i <= Order; ++i)
      {
         result[i - 1] = (*this)[i] * i;
      }
      return result;
   }
   constexpr const_polynomial operator-()
   {
      const_polynomial t(*this);
      for (unsigned i = 0; i <= Order; ++i)
         t[i] = -t[i];
      return t;
   }
   template <class U>
   constexpr const_polynomial& operator*=(U val)
   {
      for (unsigned i = 0; i <= Order; ++i)
         data[i] = data[i] * val;
      return *this;
   }
   template <class U>
   constexpr const_polynomial& operator/=(U val)
   {
      for (unsigned i = 0; i <= Order; ++i)
         data[i] = data[i] / val;
      return *this;
   }
   template <class U>
   constexpr const_polynomial& operator+=(U val)
   {
      data[0] += val;
      return *this;
   }
   template <class U>
   constexpr const_polynomial& operator-=(U val)
   {
      data[0] -= val;
      return *this;
   }
};

template <class T, unsigned Order1, unsigned Order2>
inline constexpr const_polynomial<T, (Order1 > Order2 ? Order1 : Order2)> operator+(const const_polynomial<T, Order1>& a, const const_polynomial<T, Order2>& b)
{
   if
      constexpr(Order1 > Order2)
      {
         const_polynomial<T, Order1> result(a);
         for (unsigned i = 0; i <= Order2; ++i)
            result[i] += b[i];
         return result;
      }
   else
   {
      const_polynomial<T, Order2> result(b);
      for (unsigned i = 0; i <= Order1; ++i)
         result[i] += a[i];
      return result;
   }
}
template <class T, unsigned Order1, unsigned Order2>
inline constexpr const_polynomial<T, (Order1 > Order2 ? Order1 : Order2)> operator-(const const_polynomial<T, Order1>& a, const const_polynomial<T, Order2>& b)
{
   if
      constexpr(Order1 > Order2)
      {
         const_polynomial<T, Order1> result(a);
         for (unsigned i = 0; i <= Order2; ++i)
            result[i] -= b[i];
         return result;
      }
   else
   {
      const_polynomial<T, Order2> result(b);
      for (unsigned i = 0; i <= Order1; ++i)
         result[i] = a[i] - b[i];
      return result;
   }
}
template <class T, unsigned Order1, unsigned Order2>
inline constexpr const_polynomial<T, Order1 + Order2> operator*(const const_polynomial<T, Order1>& a, const const_polynomial<T, Order2>& b)
{
   const_polynomial<T, Order1 + Order2> result;
   for (unsigned i = 0; i <= Order1; ++i)
   {
      for (unsigned j = 0; j <= Order2; ++j)
      {
         result[i + j] += a[i] * b[j];
      }
   }
   return result;
}
template <class T, unsigned Order, class U>
inline constexpr const_polynomial<T, Order> operator*(const const_polynomial<T, Order>& a, const U& b)
{
   const_polynomial<T, Order> result(a);
   for (unsigned i = 0; i <= Order; ++i)
   {
      result[i] *= b;
   }
   return result;
}
template <class U, class T, unsigned Order>
inline constexpr const_polynomial<T, Order> operator*(const U& b, const const_polynomial<T, Order>& a)
{
   const_polynomial<T, Order> result(a);
   for (unsigned i = 0; i <= Order; ++i)
   {
      result[i] *= b;
   }
   return result;
}
template <class T, unsigned Order, class U>
inline constexpr const_polynomial<T, Order> operator/(const const_polynomial<T, Order>& a, const U& b)
{
   const_polynomial<T, Order> result;
   for (unsigned i = 0; i <= Order; ++i)
   {
      result[i] /= b;
   }
   return result;
}

//[hermite_example
template <class T, unsigned Order>
class hermite_polynomial
{
   const_polynomial<T, Order> m_data;

 public:
   constexpr hermite_polynomial() : m_data(hermite_polynomial<T, Order - 1>().data() * const_polynomial<T, 1>{0, 2} - hermite_polynomial<T, Order - 1>().data().derivative())
   {
   }
   constexpr const const_polynomial<T, Order>& data() const
   {
      return m_data;
   }
   constexpr const T& operator[](std::size_t N)const
   {
      return m_data[N];
   }
   template <class U>
   constexpr T operator()(U val)const
   {
      return m_data(val);
   }
};
//] [/hermite_example]

//[hermite_example2
template <class T>
class hermite_polynomial<T, 0>
{
   const_polynomial<T, 0> m_data;

 public:
   constexpr hermite_polynomial() : m_data{1} {}
   constexpr const const_polynomial<T, 0>& data() const
   {
      return m_data;
   }
   constexpr const T& operator[](std::size_t N) const
   {
      return m_data[N];
   }
   template <class U>
   constexpr T operator()(U val)
   {
      return m_data(val);
   }
};

template <class T>
class hermite_polynomial<T, 1>
{
   const_polynomial<T, 1> m_data;

 public:
   constexpr hermite_polynomial() : m_data{0, 2} {}
   constexpr const const_polynomial<T, 1>& data() const
   {
      return m_data;
   }
   constexpr const T& operator[](std::size_t N) const
   {
      return m_data[N];
   }
   template <class U>
   constexpr T operator()(U val)
   {
      return m_data(val);
   }
};
//] [/hermite_example2]


void test_double()
{
   constexpr double radius = 2.25;
   constexpr double c      = circumference(radius);
   constexpr double a      = area(radius);

   std::cout << "Circumference = " << c << std::endl;
   std::cout << "Area = " << a << std::endl;

   constexpr const_polynomial<double, 2> pa = {3, 4};
   constexpr const_polynomial<double, 2> pb = {5, 6};
   static_assert(pa[0] == 3);
   static_assert(pa[1] == 4);
   constexpr auto pc = pa * 2;
   static_assert(pc[0] == 6);
   static_assert(pc[1] == 8);
   constexpr auto pd = 3 * pa;
   static_assert(pd[0] == 3 * 3);
   static_assert(pd[1] == 4 * 3);
   constexpr auto pe = pa + pb;
   static_assert(pe[0] == 3 + 5);
   static_assert(pe[1] == 4 + 6);
   constexpr auto pf = pa - pb;
   static_assert(pf[0] == 3 - 5);
   static_assert(pf[1] == 4 - 6);
   constexpr auto pg = pa * pb;
   static_assert(pg[0] == 15);
   static_assert(pg[1] == 38);
   static_assert(pg[2] == 24);

   #if defined(__clang__) && (__clang_major__ < 6)
   #else
   constexpr hermite_polynomial<double, 2> h1;
   static_assert(h1[0] == -2);
   static_assert(h1[1] == 0);
   static_assert(h1[2] == 4);

   constexpr hermite_polynomial<double, 3> h3;
   static_assert(h3[0] == 0);
   static_assert(h3[1] == -12);
   static_assert(h3[2] == 0);
   static_assert(h3[3] == 8);

   constexpr hermite_polynomial<double, 9> h9;
   static_assert(h9[0] == 0);
   static_assert(h9[1] == 30240);
   static_assert(h9[2] == 0);
   static_assert(h9[3] == -80640);
   static_assert(h9[4] == 0);
   static_assert(h9[5] == 48384);
   static_assert(h9[6] == 0);
   static_assert(h9[7] == -9216);
   static_assert(h9[8] == 0);
   static_assert(h9[9] == 512);

   static_assert(h9(0.5) == 6481);
   #endif
}

void test_float128()
{
#ifdef BOOST_HAS_FLOAT128
//[constexpr_circle_usage

   using boost::multiprecision::float128;

   constexpr float128 radius = 2.25;
   constexpr float128 c      = circumference(radius);
   constexpr float128 a      = area(radius);

   std::cout << "Circumference = " << c << std::endl;
   std::cout << "Area = " << a << std::endl;

 //]   [/constexpr_circle_usage]


   constexpr hermite_polynomial<float128, 2> h1;
   static_assert(h1[0] == -2);
   static_assert(h1[1] == 0);
   static_assert(h1[2] == 4);

   constexpr hermite_polynomial<float128, 3> h3;
   static_assert(h3[0] == 0);
   static_assert(h3[1] == -12);
   static_assert(h3[2] == 0);
   static_assert(h3[3] == 8);

   //[hermite_example3
   constexpr hermite_polynomial<float128, 9> h9;
   //
   // Verify that the polynomial's coefficients match the known values:
   //
   static_assert(h9[0] == 0);
   static_assert(h9[1] == 30240);
   static_assert(h9[2] == 0);
   static_assert(h9[3] == -80640);
   static_assert(h9[4] == 0);
   static_assert(h9[5] == 48384);
   static_assert(h9[6] == 0);
   static_assert(h9[7] == -9216);
   static_assert(h9[8] == 0);
   static_assert(h9[9] == 512);
   //
   // Define an abscissa value to evaluate at:
   constexpr float128 abscissa(0.5);
   //
   // Evaluate H_9(0.5) using all constexpr arithmetic, and check that it has the expected result:
   static_assert(h9(abscissa) == 6481);
   //]
#endif
}

int main()
{
   test_double();
   test_float128();
   std::cout << "Done!" << std::endl;
}
