// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana.hpp>
#include <boost/hana/ext/std/integral_constant.hpp>

#include <boost/mpl/copy_if.hpp>
#include <boost/mpl/equal.hpp>
#include <boost/mpl/less.hpp>
#include <boost/mpl/min_element.hpp>
#include <boost/mpl/or.hpp>
#include <boost/mpl/placeholders.hpp>
#include <boost/mpl/sizeof.hpp>
#include <boost/mpl/vector.hpp>

#include <boost/fusion/include/at_key.hpp>
#include <boost/fusion/include/equal_to.hpp>
#include <boost/fusion/include/filter_if.hpp>
#include <boost/fusion/include/make_map.hpp>
#include <boost/fusion/include/make_vector.hpp>

#include <string>
#include <type_traits>
#include <vector>
namespace fusion = boost::fusion;
namespace mpl = boost::mpl;
namespace hana = boost::hana;
using namespace hana::literals;


template <int n>
struct storage { char weight[n]; };

int main() {

{

//! [tuple]
auto types = hana::make_tuple(hana::type_c<int*>, hana::type_c<char&>, hana::type_c<void>);
auto char_ref = types[1_c];

BOOST_HANA_CONSTANT_CHECK(char_ref == hana::type_c<char&>);
//! [tuple]

}{

//! [filter.MPL]
using types = mpl::vector<int, char&, void*>;
using ts = mpl::copy_if<types, mpl::or_<std::is_pointer<mpl::_1>,
                                        std::is_reference<mpl::_1>>>::type;
//                             ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//                             placeholder expression

static_assert(mpl::equal<ts, mpl::vector<char&, void*>>::value, "");
//! [filter.MPL]

}{

using hana::traits::is_pointer;   // the traits namespace was not introduced
using hana::traits::is_reference; // yet, so we use unqualified names for now

//! [filter.Hana]
auto types = hana::tuple_t<int*, char&, void>;

auto ts = hana::filter(types, [](auto t) {
  return is_pointer(t) || is_reference(t);
});

BOOST_HANA_CONSTANT_CHECK(ts == hana::tuple_t<int*, char&>);
//! [filter.Hana]

}{

//! [single_library.then]
// types (MPL)
using types = mpl::vector<int*, char&, void>;
using ts = mpl::copy_if<types, mpl::or_<std::is_pointer<mpl::_1>,
                                        std::is_reference<mpl::_1>>>::type;

// values (Fusion)
auto values = fusion::make_vector(1, 'c', nullptr, 3.5);
auto vs = fusion::filter_if<std::is_integral<mpl::_1>>(values);
//! [single_library.then]

static_assert(mpl::equal<ts, mpl::vector<int*, char&>>::value, "");
BOOST_HANA_RUNTIME_CHECK(vs == fusion::make_vector(1, 'c'));

}{

using hana::traits::is_pointer;
using hana::traits::is_reference;
using hana::traits::is_integral;

//! [single_library.Hana]
// types
auto types = hana::tuple_t<int*, char&, void>;
auto ts = hana::filter(types, [](auto t) {
  return is_pointer(t) || is_reference(t);
});

// values
auto values = hana::make_tuple(1, 'c', nullptr, 3.5);
auto vs = hana::filter(values, [](auto const& t) {
  return is_integral(hana::typeid_(t));
});
//! [single_library.Hana]

BOOST_HANA_CONSTANT_CHECK(ts == hana::tuple_t<int*, char&>);
BOOST_HANA_RUNTIME_CHECK(vs == hana::make_tuple(1, 'c'));

}{

//! [make_map.Fusion]
auto map = fusion::make_map<char, int, long, float, double, void>(
  "char", "int", "long", "float", "double", "void"
);

std::string Int = fusion::at_key<int>(map);
BOOST_HANA_RUNTIME_CHECK(Int == "int");
//! [make_map.Fusion]

}{

//! [make_map.Hana]
auto map = hana::make_map(
  hana::make_pair(hana::type_c<char>,   "char"),
  hana::make_pair(hana::type_c<int>,    "int"),
  hana::make_pair(hana::type_c<long>,   "long"),
  hana::make_pair(hana::type_c<float>,  "float"),
  hana::make_pair(hana::type_c<double>, "double")
);

std::string Int = map[hana::type_c<int>];
BOOST_HANA_RUNTIME_CHECK(Int == "int");
//! [make_map.Hana]

}{

using hana::traits::add_pointer;

//! [skip_first_step]
auto types = hana::tuple_t<int*, char&, void>; // first step skipped

auto pointers = hana::transform(types, [](auto t) {
  return add_pointer(t);
});
//! [skip_first_step]

BOOST_HANA_CONSTANT_CHECK(pointers == hana::tuple_t<int**, char*, void*>);

}{

//! [traits]
BOOST_HANA_CONSTANT_CHECK(hana::traits::add_pointer(hana::type_c<int>) == hana::type_c<int*>);
BOOST_HANA_CONSTANT_CHECK(hana::traits::common_type(hana::type_c<int>, hana::type_c<long>) == hana::type_c<long>);
BOOST_HANA_CONSTANT_CHECK(hana::traits::is_integral(hana::type_c<int>));

auto types = hana::tuple_t<int, char, long>;
BOOST_HANA_CONSTANT_CHECK(hana::all_of(types, hana::traits::is_integral));
//! [traits]

}{

//! [extent]
auto extent = [](auto t, auto n) {
  return std::extent<typename decltype(t)::type, hana::value(n)>{};
};

BOOST_HANA_CONSTANT_CHECK(extent(hana::type_c<char>, hana::int_c<1>) == hana::size_c<0>);
BOOST_HANA_CONSTANT_CHECK(extent(hana::type_c<char[1][2]>, hana::int_c<1>) == hana::size_c<2>);
//! [extent]

}

}

namespace mpl_based {
//! [smallest.MPL]
template <typename ...T>
struct smallest
  : mpl::deref<
    typename mpl::min_element<
      mpl::vector<T...>,
      mpl::less<mpl::sizeof_<mpl::_1>, mpl::sizeof_<mpl::_2>>
    >::type
  >
{ };

template <typename ...T>
using smallest_t = typename smallest<T...>::type;

static_assert(std::is_same<
  smallest_t<char, long, long double>,
  char
>::value, "");
//! [smallest.MPL]

static_assert(std::is_same<
  smallest_t<storage<3>, storage<1>, storage<2>>,
  storage<1>
>::value, "");
} // end namespace mpl_based

namespace hana_based {
//! [smallest.Hana]
template <typename ...T>
auto smallest = hana::minimum(hana::make_tuple(hana::type_c<T>...), [](auto t, auto u) {
  return hana::sizeof_(t) < hana::sizeof_(u);
});

template <typename ...T>
using smallest_t = typename decltype(smallest<T...>)::type;

static_assert(std::is_same<
  smallest_t<char, long, long double>, char
>::value, "");
//! [smallest.Hana]

static_assert(std::is_same<
  smallest_t<storage<3>, storage<1>, storage<2>>,
  storage<1>
>::value, "");
} // end namespace hana_based


namespace metafunction1 {
//! [metafunction1]
template <template <typename> class F, typename T>
constexpr auto metafunction(hana::basic_type<T> const&)
{ return hana::type_c<typename F<T>::type>; }

auto t = hana::type_c<int>;
BOOST_HANA_CONSTANT_CHECK(metafunction<std::add_pointer>(t) == hana::type_c<int*>);
//! [metafunction1]
}

namespace metafunction2 {
//! [metafunction2]
template <template <typename ...> class F, typename ...T>
constexpr auto metafunction(hana::basic_type<T> const& ...)
{ return hana::type_c<typename F<T...>::type>; }

BOOST_HANA_CONSTANT_CHECK(
  metafunction<std::common_type>(hana::type_c<int>, hana::type_c<long>) == hana::type_c<long>
);
//! [metafunction2]
}

namespace _template {
//! [template_]
template <template <typename ...> class F, typename ...T>
constexpr auto template_(hana::basic_type<T> const& ...)
{ return hana::type_c<F<T...>>; }

BOOST_HANA_CONSTANT_CHECK(
  template_<std::vector>(hana::type_c<int>) == hana::type_c<std::vector<int>>
);
//! [template_]
}
