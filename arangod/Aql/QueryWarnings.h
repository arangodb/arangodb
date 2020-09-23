////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_QUERY_WARNINGS_H
#define ARANGOD_AQL_QUERY_WARNINGS_H 1

#include <mutex>
#include <string>
#include <vector>

namespace arangodb {
namespace velocypack {
class Builder;
}

namespace aql {

struct QueryOptions;

class QueryWarnings final {
  QueryWarnings(QueryWarnings const&) = delete;
  QueryWarnings& operator=(QueryWarnings const&) = delete;
  
public:

  explicit QueryWarnings();
  ~QueryWarnings() = default;

  /// @brief register an error
  /// this also makes the query abort
  [[noreturn]] void registerError(int, char const* = nullptr);

  /// @brief register a warning
  void registerWarning(int, char const* = nullptr);

  /// @brief register a warning (convenience overload)
  void registerWarning(int code, std::string const& details);
      
  void toVelocyPack(arangodb::velocypack::Builder& b) const;
  
  bool empty() const;
  
  void updateOptions(QueryOptions const&);
  
  std::vector<std::pair<int, std::string>> all() const;

  static std::string buildFormattedString(int code, char const* details);
  
 private:
  
  mutable std::mutex _mutex;
  
  /// @brief warnings collected during execution
  std::vector<std::pair<int, std::string>> _list;
  
  size_t _maxWarningCount;
  bool _failOnWarning;
};

}  // namespace aql
}  // namespace arangodb

#endif
