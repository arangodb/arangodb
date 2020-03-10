////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_QUERY_STRING_H
#define ARANGOD_AQL_QUERY_STRING_H 1

#include "Basics/Common.h"
#include <velocypack/StringRef.h>

#include <iosfwd>

namespace arangodb {
namespace aql {

/// View on a query string, does no memory management
class QueryString {
 public:
  QueryString(QueryString const& other) = default;
  QueryString& operator=(QueryString const& other) = default;

  QueryString(char const* data, size_t length)
      : _queryString(data, length), _hash(0), _hashed(false) {}

  explicit QueryString(arangodb::velocypack::StringRef const& ref)
      : QueryString(ref.data(), ref.size()) {}

  explicit QueryString(std::string const& val)
      : QueryString(val.data(), val.size()) {}

  explicit QueryString(std::string&& val)
      : _queryString(std::move(val)), _hash(0), _hashed(false) {}

  QueryString() : QueryString("", 0) {}

  ~QueryString() = default;

 public:
  std::string const& string() const noexcept { return _queryString; }
  char const* data() const { return _queryString.data(); }
  size_t size() const { return _queryString.size(); }
  size_t length() const { return _queryString.size(); }
  bool empty() const {
    return (_queryString.empty() || _queryString[0] == '\0');
  }
  void append(std::string& out) const;
  uint64_t hash() const;
  std::string extract(size_t maxLength) const;
  std::string extractRegion(int line, int column) const;

 private:
  std::string _queryString;
  mutable uint64_t _hash;
  mutable bool _hashed;
};

std::ostream& operator<<(std::ostream&, QueryString const&);
}  // namespace aql
}  // namespace arangodb

#endif
