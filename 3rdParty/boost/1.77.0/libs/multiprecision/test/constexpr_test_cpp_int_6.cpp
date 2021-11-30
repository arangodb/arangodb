//  (C) Copyright John Maddock 2019.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "boost/multiprecision/cpp_int.hpp"
#include "test.hpp"

template <class T, unsigned Order>
struct const_polynomial
{
 public:
   T data[Order + 1];

 public:
   constexpr const_polynomial(T val = 0) : data{val} {}
   constexpr const_polynomial(const const_polynomial&) = default;
   constexpr const_polynomial(const std::initializer_list<T>& init) : data{}
   {
      if (init.size() > Order + 1)
         throw std::range_error("Too many initializers in list");
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
   constexpr T operator()(U val) const
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
   constexpr const T& operator[](std::size_t N) const
   {
      return m_data[N];
   }
   template <class U>
   constexpr T operator()(U val) const
   {
      return m_data(val);
   }
};

template <class T>
class hermite_polynomial<T, 0>
{
   const_polynomial<T, 0> m_data;

 public:
   constexpr       hermite_polynomial() : m_data{1} {}
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
   constexpr       hermite_polynomial() : m_data{0, 2} {}
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



int main()
{
   using namespace boost::multiprecision::literals;

   typedef boost::multiprecision::checked_int1024_t  int_backend;

   // 8192 x^13 - 319488 x^11 + 4392960 x^9 - 26357760 x^7 + 69189120 x^5 - 69189120 x^3 + 17297280 x
   constexpr hermite_polynomial<int_backend, 13> h;

   static_assert(h[0] == 0);
   static_assert(h[1] == 17297280);
   static_assert(h[2] == 0);
   static_assert(h[3] == -69189120);
   static_assert(h[4] == 0);
   static_assert(h[5] == 69189120);
   static_assert(h[6] == 0);
   static_assert(h[7] == -26357760);
   static_assert(h[8] == 0);
   static_assert(h[9] == 4392960);
   static_assert(h[10] == 0);
   static_assert(h[11] == -319488);
   static_assert(h[12] == 0);
   static_assert(h[13] == 8192);

   return boost::report_errors();
}

