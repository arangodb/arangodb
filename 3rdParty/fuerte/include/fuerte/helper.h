////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef ARANGO_CXX_DRIVER_HELPER
#define ARANGO_CXX_DRIVER_HELPER
#include <fuerte/message.h>
#include <fuerte/types.h>
#include <sstream>
#include <string>
#include <vector>

namespace arangodb { namespace fuerte { inline namespace v1 {
namespace _detail {
template <typename IteratorType>
std::string arrayToString(IteratorType begin, IteratorType end) {
  std::stringstream ss;
  if (begin == end) {
    ss << "[]";
  } else {
    ss << "[ ";
    for (IteratorType it = begin; it != end; ++it) {
      ss << "\"" << *it << "\"" << ((std::next(it) == end) ? " " : ", ");
    }
    ss << "]";
  }
  ss << "\n";
  return ss.str();
}

template <typename IteratorType>
std::string mapToKeys(IteratorType begin, IteratorType end) {
  std::stringstream ss;
  if (begin == end) {
    ss << "Keys[]";
  } else {
    ss << "Keys[ ";
    for (IteratorType it = begin; it != end; ++it) {
      ss << "\"" << it->first << "\"" << ((std::next(it) == end) ? " " : ", ");
    }
    ss << "]";
  }
  ss << "\n";
  return ss.str();
}

template <typename IteratorType>
std::string mapToString(IteratorType begin, IteratorType end) {
  std::stringstream ss;
  if (begin == end) {
    ss << "{}";
  } else {
    ss << "{ ";
    for (IteratorType it = begin; it != end; ++it) {
      ss << "\"" << it->first << "\" : \"" << it->second
         << ((std::next(it) == end) ? " " : ", ");
    }
    ss << "}";
  }
  ss << "\n";
  return ss.str();
}
}  // namespace _detail

std::string to_string(velocypack::Slice const& slice);
std::string to_string(std::vector<velocypack::Slice> const& payload);
std::string to_string(Message& message);
StringMap sliceToStringMap(velocypack::Slice const&);

template <typename K, typename V, typename A>
std::string mapToString(std::map<K, V, A> map) {
  return _detail::mapToString(map.begin(), map.end());
}

template <typename K, typename V, typename A>
std::string mapToKeys(std::map<K, V, A> map) {
  return _detail::mapToKeys(map.begin(), map.end());
}

template <typename K, typename V, typename A>
std::string mapToString(std::unordered_map<K, V, A> map) {
  return _detail::mapToString(map.begin(), map.end());
}

template <typename K, typename V, typename A>
std::string mapToKeys(std::unordered_map<K, V, A> map) {
  return _detail::mapToKeys(map.begin(), map.end());
}

std::string encodeBase64(std::string const&);
std::string encodeBase64U(std::string const&);
}}}  // namespace arangodb::fuerte::v1
#endif
