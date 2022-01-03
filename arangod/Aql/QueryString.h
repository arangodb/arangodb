////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Basics/Common.h"

#include <iosfwd>
#include <string>
#include <string_view>

namespace arangodb {
namespace aql {

/// View on a query string
class QueryString {
 public:
  QueryString(QueryString const& other) = default;
  QueryString& operator=(QueryString const& other) = delete;
  QueryString(QueryString&& other) = default;
  QueryString& operator=(QueryString&& other) = delete;

  QueryString(char const* data, size_t length)
      : _queryString(data, length), _hash(computeHash()) {}

  explicit QueryString(std::string_view val)
      : QueryString(val.data(), val.size()) {}

  explicit QueryString(std::string val)
      : _queryString(std::move(val)), _hash(computeHash()) {}

  QueryString() : QueryString("", 0) {}

  ~QueryString() = default;

 public:
  std::string const& string() const noexcept { return _queryString; }
  char const* data() const noexcept { return _queryString.data(); }
  size_t size() const noexcept { return _queryString.size(); }
  size_t length() const noexcept { return _queryString.size(); }
  bool empty() const noexcept {
    return (_queryString.empty() || _queryString[0] == '\0');
  }
  uint64_t hash() const noexcept { return _hash; }
  void append(std::string& out) const;
  std::string extract(size_t maxLength) const;
  std::string extractRegion(int line, int column) const;

 private:
  uint64_t computeHash() const noexcept;

  std::string const _queryString;
  uint64_t const _hash;
};

std::ostream& operator<<(std::ostream&, QueryString const&);
}  // namespace aql
}  // namespace arangodb
