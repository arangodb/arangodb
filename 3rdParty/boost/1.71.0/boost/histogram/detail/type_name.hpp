// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_DETAIL_TYPE_NAME_HPP
#define BOOST_HISTOGRAM_DETAIL_TYPE_NAME_HPP

#include <boost/core/typeinfo.hpp>
#include <boost/type.hpp>
#include <sstream>
#include <string>

namespace boost {
namespace histogram {
namespace detail {

template <class T>
std::ostream& type_name_impl(std::ostream& os, boost::type<T>) {
  os << boost::core::demangled_name(BOOST_CORE_TYPEID(T));
  return os;
}

template <class T>
std::ostream& type_name_impl(std::ostream& os, boost::type<const T>) {
  return type_name_impl(os, boost::type<T>{}) << " const";
}

template <class T>
std::ostream& type_name_impl(std::ostream& os, boost::type<T&>) {
  return type_name_impl(os, boost::type<T>{}) << "&";
}

template <class T>
std::ostream& type_name_impl(std::ostream& os, boost::type<T&&>) {
  return type_name_impl(os, boost::type<T>{}) << "&&";
}

template <class T>
std::string type_name() {
  std::ostringstream os;
  type_name_impl(os, boost::type<T>{});
  return os.str();
}

} // namespace detail
} // namespace histogram
} // namespace boost

#endif
