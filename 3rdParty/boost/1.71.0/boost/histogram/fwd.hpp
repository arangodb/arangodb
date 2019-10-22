// Copyright 2015-2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_FWD_HPP
#define BOOST_HISTOGRAM_FWD_HPP

/**
  \file boost/histogram/fwd.hpp
  Forward declarations, tag types and type aliases.
*/

#include <boost/core/use_default.hpp>
#include <boost/histogram/detail/attribute.hpp> // BOOST_HISTOGRAM_NODISCARD
#include <vector>

namespace boost {
namespace histogram {

/// Tag type to indicate use of a default type
using boost::use_default;

namespace axis {

/// Integral type for axis indices
using index_type = int;

/// Real type for axis indices
using real_index_type = double;

/// Empty metadata type
struct null_type {};

#ifndef BOOST_HISTOGRAM_DOXYGEN_INVOKED

namespace transform {
struct id;
struct log;
struct sqrt;
struct pow;
} // namespace transform

template <class Value = double, class Transform = use_default,
          class MetaData = use_default, class Options = use_default>
class regular;

template <class Value = int, class MetaData = use_default, class Options = use_default>
class integer;

template <class Value = double, class MetaData = use_default, class Options = use_default,
          class Allocator = std::allocator<Value>>
class variable;

template <class Value = int, class MetaData = use_default, class Options = use_default,
          class Allocator = std::allocator<Value>>
class category;

template <class... Ts>
class variant;

#endif // BOOST_HISTOGRAM_DOXYGEN_INVOKED

} // namespace axis

#ifndef BOOST_HISTOGRAM_DOXYGEN_INVOKED

template <class T>
struct weight_type;

template <class T>
struct sample_type;

namespace accumulators {
template <class Value = double>
class sum;
template <class Value = double>
class weighted_sum;
template <class Value = double>
class mean;
template <class Value = double>
class weighted_mean;

template <class T>
class thread_safe;

template <class T>
struct is_thread_safe : std::false_type {};
template <class T>
struct is_thread_safe<thread_safe<T>> : std::true_type {};
} // namespace accumulators

struct unsafe_access;

template <class Allocator = std::allocator<char>>
class unlimited_storage;

template <class T>
class storage_adaptor;

#endif // BOOST_HISTOGRAM_DOXYGEN_INVOKED

/// Vector-like storage for fast zero-overhead access to cells.
template <class T, class A = std::allocator<T>>
using dense_storage = storage_adaptor<std::vector<T, A>>;

/// Default storage, optimized for unweighted histograms
using default_storage = unlimited_storage<>;

/// Dense storage which tracks sums of weights and a variance estimate.
using weight_storage = dense_storage<accumulators::weighted_sum<>>;

/// Dense storage which tracks means of samples in each cell.
using profile_storage = dense_storage<accumulators::mean<>>;

/// Dense storage which tracks means of weighted samples in each cell.
using weighted_profile_storage = dense_storage<accumulators::weighted_mean<>>;

#ifndef BOOST_HISTOGRAM_DOXYGEN_INVOKED

template <class Axes, class Storage = default_storage>
class BOOST_HISTOGRAM_NODISCARD histogram;

#endif
} // namespace histogram
} // namespace boost

#endif
