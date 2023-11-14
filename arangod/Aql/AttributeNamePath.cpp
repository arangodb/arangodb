////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Aql/AttributeNamePath.h"
#include "Basics/StaticStrings.h"
#include "Basics/debugging.h"
#include "Basics/fasthash.h"

#include <algorithm>
#include <iostream>

namespace arangodb {
namespace aql {

AttributeNamePath::AttributeNamePath(arangodb::ResourceMonitor& resourceMonitor)
    : _resourceMonitor(resourceMonitor) {
  // if this throws, we are ok. now dtor called
  _resourceMonitor.increaseMemoryUsage(memoryUsage());
}

AttributeNamePath::AttributeNamePath(std::string attribute,
                                     arangodb::ResourceMonitor& resourceMonitor)
    : _resourceMonitor(resourceMonitor) {
  _path.emplace_back(std::move(attribute));

  // if this throws, we are ok. now dtor called
  _resourceMonitor.increaseMemoryUsage(memoryUsage());
}

AttributeNamePath::AttributeNamePath(std::vector<std::string> path,
                                     arangodb::ResourceMonitor& resourceMonitor)
    : _resourceMonitor(resourceMonitor), _path(std::move(path)) {
  // if this throws, we are ok. now dtor called
  _resourceMonitor.increaseMemoryUsage(memoryUsage());
}

AttributeNamePath::AttributeNamePath(AttributeNamePath const& other)
    : _resourceMonitor(other._resourceMonitor), _path(other._path) {
  _resourceMonitor.increaseMemoryUsage(memoryUsage());
}

AttributeNamePath& AttributeNamePath::operator=(
    AttributeNamePath const& other) {
  if (this != &other) {
    _resourceMonitor.decreaseMemoryUsage(memoryUsage());
    _path.clear();
    _path = other._path;
    try {
      _resourceMonitor.increaseMemoryUsage(memoryUsage());
    } catch (...) {
      _path.clear();
      throw;
    }
  }
  return *this;
}

AttributeNamePath::AttributeNamePath(AttributeNamePath&& other) noexcept
    : _resourceMonitor(other._resourceMonitor), _path(std::move(other._path)) {
  other._path.clear();
  // no change in overall memory usage
}

AttributeNamePath& AttributeNamePath::operator=(
    AttributeNamePath&& other) noexcept {
  if (this != &other) {
    _resourceMonitor.decreaseMemoryUsage(memoryUsage());
    _path = std::move(other._path);
    other._path.clear();
    // no increase in overall memory usage
  }
  return *this;
}

AttributeNamePath::~AttributeNamePath() {
  _resourceMonitor.decreaseMemoryUsage(memoryUsage());
}

bool AttributeNamePath::empty() const noexcept { return _path.empty(); }

size_t AttributeNamePath::size() const noexcept { return _path.size(); }

AttributeNamePath::Type AttributeNamePath::type() const noexcept {
  TRI_ASSERT(!empty());

  Type type = Type::SingleAttribute;
  if (size() == 1) {
    if (_path[0] == std::string_view{StaticStrings::IdString}) {
      type = Type::IdAttribute;
    } else if (_path[0] == std::string_view{StaticStrings::KeyString}) {
      type = Type::KeyAttribute;
    } else if (_path[0] == std::string_view{StaticStrings::FromString}) {
      type = Type::FromAttribute;
    } else if (_path[0] == std::string_view{StaticStrings::ToString}) {
      type = Type::ToAttribute;
    }
  } else {
    TRI_ASSERT(size() > 1);
    type = Type::MultiAttribute;
  }

  TRI_ASSERT(size() == 1 || type == Type::MultiAttribute);
  return type;
}

size_t AttributeNamePath::hash() const noexcept {
  // intentionally not use std::hash() here, because its results are
  // platform-dependent. however, we need portable hash values because we are
  // testing for them in unit tests.
  uint64_t hash = 0x0404b00b1e5;
  for (auto const& it : _path) {
    hash = fasthash64(it.data(), it.size(), hash);
  }
  return static_cast<size_t>(hash);
}

std::string_view AttributeNamePath::operator[](size_t index) const noexcept {
  TRI_ASSERT(index < _path.size());
  return std::string_view{_path[index]};
}

bool AttributeNamePath::operator==(
    AttributeNamePath const& other) const noexcept {
  if (_path.size() != other._path.size()) {
    return false;
  }
  for (size_t i = 0; i < _path.size(); ++i) {
    if (_path[i] != other._path[i]) {
      return false;
    }
  }
  return true;
}

bool AttributeNamePath::operator<(
    AttributeNamePath const& other) const noexcept {
  size_t const commonLength = std::min(size(), other.size());
  for (size_t i = 0; i < commonLength; ++i) {
    if (_path[i] < other[i]) {
      return true;
    } else if (_path[i] > other[i]) {
      return false;
    }
  }
  return (size() < other.size());
}

std::vector<std::string> const& AttributeNamePath::get() const noexcept {
  return _path;
}

void AttributeNamePath::clear() noexcept {
  _resourceMonitor.decreaseMemoryUsage(memoryUsage());
  _path.clear();
}

void AttributeNamePath::add(std::string part) {
  auto previous = memoryUsage();
  _path.emplace_back(std::move(part));
  auto now = memoryUsage();
  try {
    _resourceMonitor.increaseMemoryUsage(now - previous);
  } catch (...) {
    _path.pop_back();
    throw;
  }
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
void AttributeNamePath::pop() {
  auto previous = memoryUsage();
  TRI_ASSERT(!_path.empty());
  _path.pop_back();
  auto now = memoryUsage();
  _resourceMonitor.decreaseMemoryUsage(previous - now);
}
#endif

AttributeNamePath& AttributeNamePath::reverse() {
  // no change in memory usage
  std::reverse(_path.begin(), _path.end());
  return *this;
}

size_t AttributeNamePath::memoryUsage() const noexcept {
  size_t memoryUsage = 0;
  for (size_t i = 0; i < _path.size(); ++i) {
    memoryUsage += sizeof(std::string) + _path.size();
  }
  return memoryUsage;
}

/// @brief shorten the attributes in the path to the specified length
AttributeNamePath& AttributeNamePath::shortenTo(size_t length) {
  if (length >= size()) {
    return *this;
  }
  auto previous = memoryUsage();
  _path.erase(_path.begin() + length, _path.end());
  auto now = memoryUsage();
  _resourceMonitor.decreaseMemoryUsage(previous - now);

  return *this;
}

/*static*/ size_t AttributeNamePath::commonPrefixLength(
    AttributeNamePath const& lhs, AttributeNamePath const& rhs) {
  size_t numEqual = 0;
  size_t commonLength = std::min(lhs.size(), rhs.size());
  for (size_t i = 0; i < commonLength; ++i) {
    if (lhs[i] != rhs[i]) {
      break;
    }
    ++numEqual;
  }
  return numEqual;
}

bool AttributeNamePath::isPrefixOf(
    const AttributeNamePath& other) const noexcept {
  return commonPrefixLength(*this, other) == size();
}

std::ostream& operator<<(std::ostream& stream, AttributeNamePath const& path) {
  stream << "[";
  for (auto const& it : path.get()) {
    stream << ' ' << std::string_view{it};
  }
  stream << " ]";
  return stream;
}

}  // namespace aql
}  // namespace arangodb
