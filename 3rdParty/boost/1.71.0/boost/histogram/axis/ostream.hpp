// Copyright 2015-2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// String representations here evaluate correctly in Python.

#ifndef BOOST_HISTOGRAM_AXIS_OSTREAM_HPP
#define BOOST_HISTOGRAM_AXIS_OSTREAM_HPP

#include <boost/assert.hpp>
#include <boost/histogram/axis/regular.hpp>
#include <boost/histogram/detail/cat.hpp>
#include <boost/histogram/detail/static_if.hpp>
#include <boost/histogram/detail/type_name.hpp>
#include <boost/histogram/fwd.hpp>
#include <boost/throw_exception.hpp>
#include <iomanip>
#include <iosfwd>
#include <stdexcept>
#include <type_traits>

/**
  \file boost/histogram/axis/ostream.hpp
  Simple streaming operators for the builtin axis types.

  The text representation is not guaranteed to be stable between versions of
  Boost.Histogram. This header is only included by
  [boost/histogram/ostream.hpp](histogram/reference.html#header.boost.histogram.ostream_hpp).
  To you use your own, include your own implementation instead of this header and do not
  include
  [boost/histogram/ostream.hpp](histogram/reference.html#header.boost.histogram.ostream_hpp).
 */

#ifndef BOOST_HISTOGRAM_DOXYGEN_INVOKED

namespace boost {
namespace histogram {

namespace detail {
inline const char* axis_suffix(const axis::transform::id&) { return ""; }
inline const char* axis_suffix(const axis::transform::log&) { return "_log"; }
inline const char* axis_suffix(const axis::transform::sqrt&) { return "_sqrt"; }
inline const char* axis_suffix(const axis::transform::pow&) { return "_pow"; }

template <class OStream, class T>
void stream_metadata(OStream& os, const T& t) {
  detail::static_if<detail::is_streamable<T>>(
      [&os](const auto& t) {
        std::ostringstream oss;
        oss << t;
        if (!oss.str().empty()) { os << ", metadata=" << std::quoted(oss.str()); }
      },
      [&os](const auto&) { os << ", metadata=" << detail::type_name<T>(); }, t);
}

template <class OStream>
void stream_options(OStream& os, const unsigned bits) {
  os << ", options=";
  bool first = true;

#define BOOST_HISTOGRAM_AXIS_OPTION_OSTREAM(x) \
  if (bits & axis::option::x) {                \
    if (first)                                 \
      first = false;                           \
    else {                                     \
      os << " | ";                             \
    }                                          \
    os << #x;                                  \
  }

  BOOST_HISTOGRAM_AXIS_OPTION_OSTREAM(underflow);
  BOOST_HISTOGRAM_AXIS_OPTION_OSTREAM(overflow);
  BOOST_HISTOGRAM_AXIS_OPTION_OSTREAM(circular);
  BOOST_HISTOGRAM_AXIS_OPTION_OSTREAM(growth);

#undef BOOST_HISTOGRAM_AXIS_OPTION_OSTREAM

  if (first) os << "none";
}

template <class OStream, class T>
void stream_transform(OStream&, const T&) {}

template <class OStream>
void stream_transform(OStream& os, const axis::transform::pow& t) {
  os << ", power=" << t.power;
}

template <class OStream, class T>
void stream_value(OStream& os, const T& t) {
  os << t;
}

template <class OStream, class... Ts>
void stream_value(OStream& os, const std::basic_string<Ts...>& t) {
  os << std::quoted(t);
}

} // namespace detail

namespace axis {

template <class T>
class polymorphic_bin;

template <class... Ts>
std::basic_ostream<Ts...>& operator<<(std::basic_ostream<Ts...>& os, const null_type&) {
  return os; // do nothing
}

template <class... Ts, class U>
std::basic_ostream<Ts...>& operator<<(std::basic_ostream<Ts...>& os,
                                      const interval_view<U>& i) {
  os << "[" << i.lower() << ", " << i.upper() << ")";
  return os;
}

template <class... Ts, class U>
std::basic_ostream<Ts...>& operator<<(std::basic_ostream<Ts...>& os,
                                      const polymorphic_bin<U>& i) {
  if (i.is_discrete())
    os << static_cast<double>(i);
  else
    os << "[" << i.lower() << ", " << i.upper() << ")";
  return os;
}

template <class... Ts, class... Us>
std::basic_ostream<Ts...>& operator<<(std::basic_ostream<Ts...>& os,
                                      const regular<Us...>& a) {
  os << "regular" << detail::axis_suffix(a.transform()) << "(" << a.size() << ", "
     << a.value(0) << ", " << a.value(a.size());
  detail::stream_metadata(os, a.metadata());
  detail::stream_options(os, a.options());
  detail::stream_transform(os, a.transform());
  os << ")";
  return os;
}

template <class... Ts, class... Us>
std::basic_ostream<Ts...>& operator<<(std::basic_ostream<Ts...>& os,
                                      const integer<Us...>& a) {
  os << "integer(" << a.value(0) << ", " << a.value(a.size());
  detail::stream_metadata(os, a.metadata());
  detail::stream_options(os, a.options());
  os << ")";
  return os;
}

template <class... Ts, class... Us>
std::basic_ostream<Ts...>& operator<<(std::basic_ostream<Ts...>& os,
                                      const variable<Us...>& a) {
  os << "variable(" << a.value(0);
  for (index_type i = 1, n = a.size(); i <= n; ++i) { os << ", " << a.value(i); }
  detail::stream_metadata(os, a.metadata());
  detail::stream_options(os, a.options());
  os << ")";
  return os;
}

template <class... Ts, class... Us>
std::basic_ostream<Ts...>& operator<<(std::basic_ostream<Ts...>& os,
                                      const category<Us...>& a) {
  os << "category(";
  for (index_type i = 0, n = a.size(); i < n; ++i) {
    detail::stream_value(os, a.value(i));
    os << (i == (a.size() - 1) ? "" : ", ");
  }
  detail::stream_metadata(os, a.metadata());
  detail::stream_options(os, a.options());
  os << ")";
  return os;
}

template <class... Ts, class... Us>
std::basic_ostream<Ts...>& operator<<(std::basic_ostream<Ts...>& os,
                                      const variant<Us...>& v) {
  visit(
      [&os](const auto& x) {
        using A = std::decay_t<decltype(x)>;
        detail::static_if<detail::is_streamable<A>>(
            [&os](const auto& x) { os << x; },
            [](const auto&) {
              BOOST_THROW_EXCEPTION(std::runtime_error(
                  detail::cat(detail::type_name<A>(), " is not streamable")));
            },
            x);
      },
      v);
  return os;
}

} // namespace axis
} // namespace histogram
} // namespace boost

#endif // BOOST_HISTOGRAM_DOXYGEN_INVOKED

#endif
