// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/integral_c.hpp>
#include <boost/mpl/minus.hpp>
#include <boost/mpl/multiplies.hpp>
#include <boost/mpl/pair.hpp>
#include <boost/mpl/plus.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/concept/constant.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/minus.hpp>
#include <boost/hana/mult.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/plus.hpp>

#include <type_traits>
namespace hana = boost::hana;


template <typename T, typename = std::enable_if_t<
  !hana::Constant<T>::value
>>
constexpr T sqrt(T x) {
  T inf = 0, sup = (x == 1 ? 1 : x/2);
  while (!((sup - inf) <= 1 || ((sup*sup <= x) && ((sup+1)*(sup+1) > x)))) {
    T mid = (inf + sup) / 2;
    bool take_inf = mid*mid > x ? 1 : 0;
    inf = take_inf ? inf : mid;
    sup = take_inf ? mid : sup;
  }

  return sup*sup <= x ? sup : inf;
}

template <typename T, typename = std::enable_if_t<
  hana::Constant<T>::value
>>
constexpr auto sqrt(T const&) {
  return hana::integral_c<typename T::value_type, sqrt(T::value)>;
}


namespace then {
namespace mpl = boost::mpl;

template <typename N>
struct sqrt
  : mpl::integral_c<typename N::value_type, ::sqrt(N::value)>
{ };

template <typename X, typename Y>
struct point {
  using x = X;
  using y = Y;
};

//! [distance-mpl]
template <typename P1, typename P2>
struct distance {
  using xs = typename mpl::minus<typename P1::x,
                                 typename P2::x>::type;
  using ys = typename mpl::minus<typename P1::y,
                                 typename P2::y>::type;
  using type = typename sqrt<
    typename mpl::plus<
      typename mpl::multiplies<xs, xs>::type,
      typename mpl::multiplies<ys, ys>::type
    >::type
  >::type;
};

static_assert(mpl::equal_to<
  distance<point<mpl::int_<3>, mpl::int_<5>>,
           point<mpl::int_<7>, mpl::int_<2>>>::type,
  mpl::int_<5>
>::value, "");
//! [distance-mpl]
}


namespace now {
namespace hana = boost::hana;
using namespace hana::literals;

template <typename X, typename Y>
struct _point {
  X x;
  Y y;
};
template <typename X, typename Y>
constexpr _point<X, Y> point(X x, Y y) { return {x, y}; }

//! [distance-hana]
template <typename P1, typename P2>
constexpr auto distance(P1 p1, P2 p2) {
  auto xs = p1.x - p2.x;
  auto ys = p1.y - p2.y;
  return sqrt(xs*xs + ys*ys);
}

BOOST_HANA_CONSTANT_CHECK(distance(point(3_c, 5_c), point(7_c, 2_c)) == 5_c);
//! [distance-hana]

void test() {

//! [distance-dynamic]
auto p1 = point(3, 5); // dynamic values now
auto p2 = point(7, 2); //
BOOST_HANA_RUNTIME_CHECK(distance(p1, p2) == 5); // same function works!
//! [distance-dynamic]

}
}


int main() {
  now::test();
}
