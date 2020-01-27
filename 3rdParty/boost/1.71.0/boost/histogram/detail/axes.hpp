// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_DETAIL_AXES_HPP
#define BOOST_HISTOGRAM_DETAIL_AXES_HPP

#include <array>
#include <boost/assert.hpp>
#include <boost/histogram/axis/traits.hpp>
#include <boost/histogram/axis/variant.hpp>
#include <boost/histogram/detail/static_if.hpp>
#include <boost/histogram/fwd.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/tuple.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/throw_exception.hpp>
#include <stdexcept>
#include <tuple>
#include <type_traits>

/* Most of the histogram code is generic and works for any number of axes. Buffers with a
 * fixed maximum capacity are used in some places, which have a size equal to the rank of
 * a histogram. The buffers are statically allocated to improve performance, which means
 * that they need a preset maximum capacity. 32 seems like a safe upper limit for the rank
 * (you can nevertheless increase it here if necessary): the simplest non-trivial axis has
 * 2 bins; even if counters are used which need only a byte of storage per bin, this still
 * corresponds to 4 GB of storage.
 */
#ifndef BOOST_HISTOGRAM_DETAIL_AXES_LIMIT
#define BOOST_HISTOGRAM_DETAIL_AXES_LIMIT 32
#endif

namespace boost {
namespace histogram {
namespace detail {

template <class T>
unsigned axes_rank(const T& axes) {
  using std::begin;
  using std::end;
  return static_cast<unsigned>(std::distance(begin(axes), end(axes)));
}

template <class... Ts>
constexpr unsigned axes_rank(const std::tuple<Ts...>&) {
  return static_cast<unsigned>(sizeof...(Ts));
}

template <unsigned N, class... Ts>
decltype(auto) axis_get(std::tuple<Ts...>& axes) {
  return std::get<N>(axes);
}

template <unsigned N, class... Ts>
decltype(auto) axis_get(const std::tuple<Ts...>& axes) {
  return std::get<N>(axes);
}

template <unsigned N, class T>
decltype(auto) axis_get(T& axes) {
  return axes[N];
}

template <unsigned N, class T>
decltype(auto) axis_get(const T& axes) {
  return axes[N];
}

template <class... Ts>
decltype(auto) axis_get(std::tuple<Ts...>& axes, unsigned i) {
  using namespace boost::mp11;
  constexpr auto S = sizeof...(Ts);
  using V = mp_unique<axis::variant<Ts*...>>;
  return mp_with_index<S>(i, [&axes](auto i) { return V(&std::get<i>(axes)); });
}

template <class... Ts>
decltype(auto) axis_get(const std::tuple<Ts...>& axes, unsigned i) {
  using namespace boost::mp11;
  constexpr auto S = sizeof...(Ts);
  using V = mp_unique<axis::variant<const Ts*...>>;
  return mp_with_index<S>(i, [&axes](auto i) { return V(&std::get<i>(axes)); });
}

template <class T>
decltype(auto) axis_get(T& axes, unsigned i) {
  return axes.at(i);
}

template <class T>
decltype(auto) axis_get(const T& axes, unsigned i) {
  return axes.at(i);
}

template <class... Ts, class... Us>
bool axes_equal(const std::tuple<Ts...>& ts, const std::tuple<Us...>& us) {
  return static_if<std::is_same<mp11::mp_list<Ts...>, mp11::mp_list<Us...>>>(
      [](const auto& ts, const auto& us) {
        using N = mp11::mp_size<std::decay_t<decltype(ts)>>;
        bool equal = true;
        mp11::mp_for_each<mp11::mp_iota<N>>(
            [&](auto I) { equal &= relaxed_equal(std::get<I>(ts), std::get<I>(us)); });
        return equal;
      },
      [](const auto&, const auto&) { return false; }, ts, us);
}

template <class T, class... Us>
bool axes_equal(const T& t, const std::tuple<Us...>& u) {
  if (t.size() != sizeof...(Us)) return false;
  bool equal = true;
  mp11::mp_for_each<mp11::mp_iota_c<sizeof...(Us)>>(
      [&](auto I) { equal &= t[I] == std::get<I>(u); });
  return equal;
}

template <class... Ts, class U>
bool axes_equal(const std::tuple<Ts...>& t, const U& u) {
  return axes_equal(u, t);
}

template <class T, class U>
bool axes_equal(const T& t, const U& u) {
  if (t.size() != u.size()) return false;
  return std::equal(t.begin(), t.end(), u.begin());
}

template <class... Ts, class... Us>
void axes_assign(std::tuple<Ts...>& t, const std::tuple<Us...>& u) {
  static_if<std::is_same<mp11::mp_list<Ts...>, mp11::mp_list<Us...>>>(
      [](auto& a, const auto& b) { a = b; },
      [](auto&, const auto&) {
        BOOST_THROW_EXCEPTION(
            std::invalid_argument("cannot assign axes, types do not match"));
      },
      t, u);
}

template <class... Ts, class U>
void axes_assign(std::tuple<Ts...>& t, const U& u) {
  mp11::mp_for_each<mp11::mp_iota_c<sizeof...(Ts)>>([&](auto I) {
    using T = mp11::mp_at_c<std::tuple<Ts...>, I>;
    std::get<I>(t) = axis::get<T>(u[I]);
  });
}

template <class T, class... Us>
void axes_assign(T& t, const std::tuple<Us...>& u) {
  // resize instead of reserve, because t may not be empty and we want exact capacity
  t.resize(sizeof...(Us));
  mp11::mp_for_each<mp11::mp_iota_c<sizeof...(Us)>>(
      [&](auto I) { t[I] = std::get<I>(u); });
}

template <typename T, typename U>
void axes_assign(T& t, const U& u) {
  t.assign(u.begin(), u.end());
}

// create empty dynamic axis which can store any axes types from the argument
template <class T>
auto make_empty_dynamic_axes(const T& axes) {
  return make_default(axes);
}

template <class... Ts>
auto make_empty_dynamic_axes(const std::tuple<Ts...>&) {
  using namespace ::boost::mp11;
  using L = mp_unique<axis::variant<Ts...>>;
  // return std::vector<axis::variant<Axis0, Axis1, ...>> or std::vector<Axis0>
  return std::vector<mp_if_c<(mp_size<L>::value == 1), mp_first<L>, L>>{};
}

template <typename T>
void axis_index_is_valid(const T& axes, const unsigned N) {
  BOOST_ASSERT_MSG(N < axes_rank(axes), "index out of range");
}

template <typename F, typename T>
void for_each_axis_impl(std::true_type, const T& axes, F&& f) {
  for (const auto& x : axes) { axis::visit(std::forward<F>(f), x); }
}

template <typename F, typename T>
void for_each_axis_impl(std::false_type, const T& axes, F&& f) {
  for (const auto& x : axes) std::forward<F>(f)(x);
}

template <typename F, typename T>
void for_each_axis(const T& axes, F&& f) {
  using U = mp11::mp_first<T>;
  for_each_axis_impl(is_axis_variant<U>(), axes, std::forward<F>(f));
}

template <typename F, typename... Ts>
void for_each_axis(const std::tuple<Ts...>& axes, F&& f) {
  mp11::tuple_for_each(axes, std::forward<F>(f));
}

template <typename T>
std::size_t bincount(const T& axes) {
  std::size_t n = 1;
  for_each_axis(axes, [&n](const auto& a) {
    const auto old = n;
    const auto s = axis::traits::extent(a);
    n *= s;
    if (s > 0 && n < old) BOOST_THROW_EXCEPTION(std::overflow_error("bincount overflow"));
  });
  return n;
}

template <class T>
using tuple_size_t = typename std::tuple_size<T>::type;

template <class T>
using buffer_size = mp11::mp_eval_or<
    std::integral_constant<std::size_t, BOOST_HISTOGRAM_DETAIL_AXES_LIMIT>, tuple_size_t,
    T>;

template <class T, std::size_t N>
class sub_array : public std::array<T, N> {
  using base_type = std::array<T, N>;

public:
  explicit sub_array(std::size_t s) noexcept(
      std::is_nothrow_default_constructible<T>::value)
      : size_(s) {
    BOOST_ASSERT_MSG(size_ <= N, "requested size exceeds size of static buffer");
  }

  sub_array(std::size_t s,
            const T& value) noexcept(std::is_nothrow_copy_constructible<T>::value)
      : size_(s) {
    BOOST_ASSERT_MSG(size_ <= N, "requested size exceeds size of static buffer");
    std::array<T, N>::fill(value);
  }

  // need to override both versions of std::array
  auto end() noexcept { return base_type::begin() + size_; }
  auto end() const noexcept { return base_type::begin() + size_; }

  auto size() const noexcept { return size_; }

private:
  std::size_t size_;
};

template <class U, class T>
using stack_buffer = sub_array<U, buffer_size<T>::value>;

template <class U, class T, class... Ts>
auto make_stack_buffer(const T& t, const Ts&... ts) {
  return stack_buffer<U, T>(axes_rank(t), ts...);
}

} // namespace detail
} // namespace histogram
} // namespace boost

#endif
