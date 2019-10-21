// Copyright 2015-2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_DETAIL_DETECT_HPP
#define BOOST_HISTOGRAM_DETAIL_DETECT_HPP

#include <boost/histogram/fwd.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/function.hpp>
#include <boost/mp11/utility.hpp>
#include <iterator>
#include <tuple>
#include <type_traits>

namespace boost {
namespace histogram {
namespace detail {

#define BOOST_HISTOGRAM_DETECT(name, cond)   \
  template <class T, class = decltype(cond)> \
  struct name##_impl {};                     \
  template <class T>                         \
  struct name : mp11::mp_valid<name##_impl, T>::type {}

#define BOOST_HISTOGRAM_DETECT_BINARY(name, cond)     \
  template <class T, class U, class = decltype(cond)> \
  struct name##_impl {};                              \
  template <class T, class U = T>                     \
  struct name : mp11::mp_valid<name##_impl, T, U>::type {}

// metadata has overloads, trying to get pmf in this case always fails
BOOST_HISTOGRAM_DETECT(has_method_metadata, (std::declval<T&>().metadata()));

// resize has overloads, trying to get pmf in this case always fails
BOOST_HISTOGRAM_DETECT(has_method_resize, (std::declval<T&>().resize(0)));

BOOST_HISTOGRAM_DETECT(has_method_size, &T::size);

BOOST_HISTOGRAM_DETECT(has_method_clear, &T::clear);

BOOST_HISTOGRAM_DETECT(has_method_lower, &T::lower);

BOOST_HISTOGRAM_DETECT(has_method_value, &T::value);

BOOST_HISTOGRAM_DETECT(has_method_update, (&T::update));

// reset has overloads, trying to get pmf in this case always fails
BOOST_HISTOGRAM_DETECT(has_method_reset, (std::declval<T>().reset(0)));

BOOST_HISTOGRAM_DETECT(has_method_options, (&T::options));

BOOST_HISTOGRAM_DETECT(has_allocator, &T::get_allocator);

BOOST_HISTOGRAM_DETECT(is_indexable, (std::declval<T&>()[0]));

BOOST_HISTOGRAM_DETECT(is_transform, (&T::forward, &T::inverse));

BOOST_HISTOGRAM_DETECT(is_indexable_container,
                       (std::declval<T>()[0], &T::size, std::begin(std::declval<T>()),
                        std::end(std::declval<T>())));

BOOST_HISTOGRAM_DETECT(is_vector_like,
                       (std::declval<T>()[0], &T::size, std::declval<T>().resize(0),
                        std::begin(std::declval<T>()), std::end(std::declval<T>())));

BOOST_HISTOGRAM_DETECT(is_array_like,
                       (std::declval<T>()[0], &T::size, std::tuple_size<T>::value,
                        std::begin(std::declval<T>()), std::end(std::declval<T>())));

BOOST_HISTOGRAM_DETECT(is_map_like,
                       (std::declval<typename T::key_type>(),
                        std::declval<typename T::mapped_type>(),
                        std::begin(std::declval<T>()), std::end(std::declval<T>())));

// ok: is_axis is false for axis::variant, because T::index is templated
BOOST_HISTOGRAM_DETECT(is_axis, (&T::size, &T::index));

BOOST_HISTOGRAM_DETECT(is_iterable,
                       (std::begin(std::declval<T&>()), std::end(std::declval<T&>())));

BOOST_HISTOGRAM_DETECT(is_iterator,
                       (typename std::iterator_traits<T>::iterator_category()));

BOOST_HISTOGRAM_DETECT(is_streamable,
                       (std::declval<std::ostream&>() << std::declval<T&>()));

BOOST_HISTOGRAM_DETECT(has_operator_preincrement, (++std::declval<T&>()));

BOOST_HISTOGRAM_DETECT_BINARY(has_operator_equal,
                              (std::declval<const T&>() == std::declval<const U>()));

BOOST_HISTOGRAM_DETECT_BINARY(has_operator_radd,
                              (std::declval<T&>() += std::declval<U>()));

BOOST_HISTOGRAM_DETECT_BINARY(has_operator_rsub,
                              (std::declval<T&>() -= std::declval<U>()));

BOOST_HISTOGRAM_DETECT_BINARY(has_operator_rmul,
                              (std::declval<T&>() *= std::declval<U>()));

BOOST_HISTOGRAM_DETECT_BINARY(has_operator_rdiv,
                              (std::declval<T&>() /= std::declval<U>()));

BOOST_HISTOGRAM_DETECT_BINARY(
    has_method_eq, (std::declval<const T>().operator==(std::declval<const U>())));

BOOST_HISTOGRAM_DETECT(has_threading_support, (T::has_threading_support));

template <typename T>
struct is_weight_impl : std::false_type {};

template <typename T>
struct is_weight_impl<weight_type<T>> : std::true_type {};

template <typename T>
using is_weight = is_weight_impl<std::decay_t<T>>;

template <typename T>
struct is_sample_impl : std::false_type {};

template <typename T>
struct is_sample_impl<sample_type<T>> : std::true_type {};

template <typename T>
using is_sample = is_sample_impl<std::decay_t<T>>;

template <typename T>
using is_storage = mp11::mp_and<is_indexable_container<T>, has_method_reset<T>,
                                has_threading_support<T>>;

template <class T>
using is_adaptible = mp11::mp_or<is_vector_like<T>, is_array_like<T>, is_map_like<T>>;

template <class T, class _ = std::decay_t<T>,
          class = std::enable_if_t<(is_storage<_>::value || is_adaptible<_>::value)>>
struct requires_storage_or_adaptible {};

template <typename T>
struct is_tuple_impl : std::false_type {};

template <typename... Ts>
struct is_tuple_impl<std::tuple<Ts...>> : std::true_type {};

template <typename T>
using is_tuple = typename is_tuple_impl<T>::type;

template <typename T>
struct is_axis_variant_impl : std::false_type {};

template <typename... Ts>
struct is_axis_variant_impl<axis::variant<Ts...>> : std::true_type {};

template <typename T>
using is_axis_variant = typename is_axis_variant_impl<T>::type;

template <typename T>
using is_any_axis = mp11::mp_or<is_axis<T>, is_axis_variant<T>>;

template <typename T>
using is_sequence_of_axis = mp11::mp_and<is_iterable<T>, is_axis<mp11::mp_first<T>>>;

template <typename T>
using is_sequence_of_axis_variant =
    mp11::mp_and<is_iterable<T>, is_axis_variant<mp11::mp_first<T>>>;

template <typename T>
using is_sequence_of_any_axis =
    mp11::mp_and<is_iterable<T>, is_any_axis<mp11::mp_first<T>>>;

// poor-mans concept checks
template <class T, class = std::enable_if_t<is_iterator<std::decay_t<T>>::value>>
struct requires_iterator {};

template <class T, class = std::enable_if_t<
                       is_iterable<std::remove_cv_t<std::remove_reference_t<T>>>::value>>
struct requires_iterable {};

template <class T, class = std::enable_if_t<is_axis<std::decay_t<T>>::value>>
struct requires_axis {};

template <class T, class = std::enable_if_t<is_any_axis<std::decay_t<T>>::value>>
struct requires_any_axis {};

template <class T, class = std::enable_if_t<is_sequence_of_axis<std::decay_t<T>>::value>>
struct requires_sequence_of_axis {};

template <class T,
          class = std::enable_if_t<is_sequence_of_axis_variant<std::decay_t<T>>::value>>
struct requires_sequence_of_axis_variant {};

template <class T,
          class = std::enable_if_t<is_sequence_of_any_axis<std::decay_t<T>>::value>>
struct requires_sequence_of_any_axis {};

template <class T,
          class = std::enable_if_t<is_any_axis<mp11::mp_first<std::decay_t<T>>>::value>>
struct requires_axes {};

template <class T, class U, class = std::enable_if_t<std::is_convertible<T, U>::value>>
struct requires_convertible {};

} // namespace detail
} // namespace histogram
} // namespace boost

#endif
