#include <type_traits>
#include "test.hpp"
#include <boost/multiprecision/mpc.hpp>
#include <boost/math/constants/constants.hpp>

using boost::multiprecision::mpc_complex_100;

template<class Complex>
void test_overloads()
{
  typedef typename Complex::value_type Real;
  Complex ya = {5.2, 7.4};
  Complex yb = {8.2, 7.3};
  Real h = 0.0001;
  auto I0 = (ya + yb)*h;
  Complex I1 = I0/2 + yb*h;

  //I1 = I0;  // not supposed to work.

  Complex z{2, 3};
  typename Complex::value_type theta = 0.2;
  int n = 2;
  using std::sin;
  Complex arg = z*sin(theta) - n*theta;


  using std::exp;
  Real v = 0.2;
  Real cotv = 7.8;
  Real cscv = 8.2;
  Complex den = z + v*cscv*exp(-v*cotv);


  boost::multiprecision::number<boost::multiprecision::backends::mpc_complex_backend<100> > a = 2;
  boost::multiprecision::number<boost::multiprecision::backends::mpc_complex_backend<100> > b = 3;
  /*
  if (a <= b) {
    b = a;
  }*/
}

template<class F, class Real>
typename std::result_of_t<F(Real)> some_functional(F f, Real a, Real b)
{
  if(a <= -boost::math::tools::max_value<Real>()) {
    return f(a);
  } 

  return f(b);

}

template<class Complex>
void test_functional()
{
  typedef typename Complex::value_type Real;
  auto f = [](Real x)->Complex { Complex z(x, 3); return z;};
  Real a = 0;
  Real b = 1;
  Complex result = some_functional(f, a, b);
}



int main()
{
  test_overloads<mpc_complex_100>();
  test_functional<mpc_complex_100>();
}
