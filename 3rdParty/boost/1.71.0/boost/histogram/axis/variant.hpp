// Copyright 2015-2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_AXIS_VARIANT_HPP
#define BOOST_HISTOGRAM_AXIS_VARIANT_HPP

#include <boost/histogram/axis/iterator.hpp>
#include <boost/histogram/axis/polymorphic_bin.hpp>
#include <boost/histogram/axis/traits.hpp>
#include <boost/histogram/detail/cat.hpp>
#include <boost/histogram/detail/relaxed_equal.hpp>
#include <boost/histogram/detail/static_if.hpp>
#include <boost/histogram/detail/type_name.hpp>
#include <boost/histogram/fwd.hpp>
#include <boost/mp11/function.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/throw_exception.hpp>
#include <boost/variant2/variant.hpp>
#include <ostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace boost {
namespace histogram {
namespace axis {

/// Polymorphic axis type
template <class... Ts>
class variant : public iterator_mixin<variant<Ts...>> {
  using impl_type = boost::variant2::variant<Ts...>;

  template <class T>
  using is_bounded_type = mp11::mp_contains<variant, std::decay_t<T>>;

  template <typename T>
  using requires_bounded_type = std::enable_if_t<is_bounded_type<T>::value>;

  // maybe metadata_type or const metadata_type, if bounded type is const
  using metadata_type = std::remove_reference_t<decltype(
      traits::metadata(std::declval<std::remove_pointer_t<mp11::mp_first<variant>>>()))>;

public:
  // cannot import ctors with using directive, it breaks gcc and msvc
  variant() = default;
  variant(const variant&) = default;
  variant& operator=(const variant&) = default;
  variant(variant&&) = default;
  variant& operator=(variant&&) = default;

  template <class T, class = requires_bounded_type<T>>
  variant(T&& t) : impl(std::forward<T>(t)) {}

  template <class T, class = requires_bounded_type<T>>
  variant& operator=(T&& t) {
    impl = std::forward<T>(t);
    return *this;
  }

  template <class... Us>
  variant(const variant<Us...>& u) {
    this->operator=(u);
  }

  template <class... Us>
  variant& operator=(const variant<Us...>& u) {
    visit(
        [this](const auto& u) {
          using U = std::decay_t<decltype(u)>;
          detail::static_if<is_bounded_type<U>>(
              [this](const auto& u) { this->operator=(u); },
              [](const auto&) {
                BOOST_THROW_EXCEPTION(std::runtime_error(detail::cat(
                    detail::type_name<U>(), " is not convertible to a bounded type of ",
                    detail::type_name<variant>())));
              },
              u);
        },
        u);
    return *this;
  }

  /// Return size of axis.
  index_type size() const {
    return visit([](const auto& a) { return a.size(); }, *this);
  }

  /// Return options of axis or option::none_t if axis has no options.
  unsigned options() const {
    return visit([](const auto& a) { return axis::traits::options(a); }, *this);
  }

  /// Return reference to const metadata or instance of null_type if axis has no
  /// metadata.
  const metadata_type& metadata() const {
    return visit(
        [](const auto& a) -> const metadata_type& {
          using M = decltype(traits::metadata(a));
          return detail::static_if<std::is_same<M, const metadata_type&>>(
              [](const auto& a) -> const metadata_type& { return traits::metadata(a); },
              [](const auto&) -> const metadata_type& {
                BOOST_THROW_EXCEPTION(std::runtime_error(
                    detail::cat("cannot return metadata of type ", detail::type_name<M>(),
                                " through axis::variant interface which uses type ",
                                detail::type_name<metadata_type>(),
                                "; use boost::histogram::axis::get to obtain a reference "
                                "of this axis type")));
              },
              a);
        },
        *this);
  }

  /// Return reference to metadata or instance of null_type if axis has no
  /// metadata.
  metadata_type& metadata() {
    return visit(
        [](auto& a) -> metadata_type& {
          using M = decltype(traits::metadata(a));
          return detail::static_if<std::is_same<M, metadata_type&>>(
              [](auto& a) -> metadata_type& { return traits::metadata(a); },
              [](auto&) -> metadata_type& {
                BOOST_THROW_EXCEPTION(std::runtime_error(
                    detail::cat("cannot return metadata of type ", detail::type_name<M>(),
                                " through axis::variant interface which uses type ",
                                detail::type_name<metadata_type>(),
                                "; use boost::histogram::axis::get to obtain a reference "
                                "of this axis type")));
              },
              a);
        },
        *this);
  }

  /** Return index for value argument.

    Throws std::invalid_argument if axis has incompatible call signature.
  */
  template <class U>
  index_type index(const U& u) const {
    return visit([&u](const auto& a) { return traits::index(a, u); }, *this);
  }

  /** Return value for index argument.

    Only works for axes with value method that returns something convertible
    to double and will throw a runtime_error otherwise, see
    axis::traits::value().
  */
  double value(real_index_type idx) const {
    return visit([idx](const auto& a) { return traits::value_as<double>(a, idx); },
                 *this);
  }

  /** Return bin for index argument.

    Only works for axes with value method that returns something convertible
    to double and will throw a runtime_error otherwise, see
    axis::traits::value().
  */
  auto bin(index_type idx) const {
    return visit(
        [idx](const auto& a) {
          return detail::value_method_switch(
              [idx](const auto& a) { // axis is discrete
                const double x = traits::value_as<double>(a, idx);
                return polymorphic_bin<double>(x, x);
              },
              [idx](const auto& a) { // axis is continuous
                const double x1 = traits::value_as<double>(a, idx);
                const double x2 = traits::value_as<double>(a, idx + 1);
                return polymorphic_bin<double>(x1, x2);
              },
              a);
        },
        *this);
  }

  /** Compare two variants.

    Return true if the variants point to the same concrete axis type and the types compare
    equal. Otherwise return false.
  */
  template <class... Us>
  bool operator==(const variant<Us...>& u) const {
    return visit([&u](const auto& x) { return u == x; }, *this);
  }

  /** Compare variant with a concrete axis type.

    Return true if the variant point to the same concrete axis type and the types compare
    equal. Otherwise return false.
  */
  template <class T>
  bool operator==(const T& t) const {
    return detail::static_if_c<(mp11::mp_contains<impl_type, T>::value ||
                                mp11::mp_contains<impl_type, T*>::value ||
                                mp11::mp_contains<impl_type, const T*>::value)>(
        [&](const auto& t) {
          using U = std::decay_t<decltype(t)>;
          const U* tp = detail::variant_access::template get_if<U>(this);
          return tp && detail::relaxed_equal(*tp, t);
        },
        [&](const auto&) { return false; }, t);
  }

  /// The negation of operator==.
  template <class T>
  bool operator!=(const T& t) const {
    return !operator==(t);
  }

private:
  impl_type impl;

  friend struct detail::variant_access;
  friend struct boost::histogram::unsafe_access;
};

// specialization for empty argument list, useful for meta-programming
template <>
class variant<> {};

/// Apply visitor to variant (reference).
template <class Visitor, class... Us>
decltype(auto) visit(Visitor&& vis, variant<Us...>& var) {
  return detail::variant_access::visit(vis, var);
}

/// Apply visitor to variant (movable reference).
template <class Visitor, class... Us>
decltype(auto) visit(Visitor&& vis, variant<Us...>&& var) {
  return detail::variant_access::visit(vis, std::move(var));
}

/// Apply visitor to variant (const reference).
template <class Visitor, class... Us>
decltype(auto) visit(Visitor&& vis, const variant<Us...>& var) {
  return detail::variant_access::visit(vis, var);
}

/// Returns pointer to T in variant or null pointer if type does not match.
template <class T, class... Us>
T* get_if(variant<Us...>* v) {
  return detail::variant_access::template get_if<T>(v);
}

/// Returns pointer to const T in variant or null pointer if type does not match.
template <class T, class... Us>
const T* get_if(const variant<Us...>* v) {
  return detail::variant_access::template get_if<T>(v);
}

/// Return reference to T, throws std::runtime_error if type does not match.
template <class T, class... Us>
decltype(auto) get(variant<Us...>& v) {
  auto tp = get_if<T>(&v);
  if (!tp) BOOST_THROW_EXCEPTION(std::runtime_error("T is not the held type"));
  return *tp;
}

/// Return movable reference to T, throws unspecified exception if type does not match.
template <class T, class... Us>
decltype(auto) get(variant<Us...>&& v) {
  auto tp = get_if<T>(&v);
  if (!tp) BOOST_THROW_EXCEPTION(std::runtime_error("T is not the held type"));
  return std::move(*tp);
}

/// Return const reference to T, throws unspecified exception if type does not match.
template <class T, class... Us>
decltype(auto) get(const variant<Us...>& v) {
  auto tp = get_if<T>(&v);
  if (!tp) BOOST_THROW_EXCEPTION(std::runtime_error("T is not the held type"));
  return *tp;
}

// pass-through version of get for generic programming
template <class T, class U>
decltype(auto) get(U&& u) {
  return std::forward<U>(u);
}

// pass-through version of get_if for generic programming
template <class T, class U>
T* get_if(U* u) {
  return std::is_same<T, std::decay_t<U>>::value ? reinterpret_cast<T*>(u) : nullptr;
}

// pass-through version of get_if for generic programming
template <class T, class U>
const T* get_if(const U* u) {
  return std::is_same<T, std::decay_t<U>>::value ? reinterpret_cast<const T*>(u)
                                                 : nullptr;
}

} // namespace axis
} // namespace histogram
} // namespace boost

#endif
