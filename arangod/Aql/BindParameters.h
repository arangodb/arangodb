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

#ifndef ARANGOD_AQL_BIND_PARAMETERS_H
#define ARANGOD_AQL_BIND_PARAMETERS_H 1

#include <velocypack/Slice.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace arangodb {
struct ResourceMonitor;

namespace velocypack {
class Builder;
}
namespace aql {

typedef std::unordered_map<std::string, std::pair<arangodb::velocypack::Slice, bool>> BindParametersType;

class BindParameters {
 public:
  BindParameters(BindParameters const&) = delete;
  BindParameters& operator=(BindParameters const&) = delete;

  explicit BindParameters(ResourceMonitor& resourceMonitor);

  /// @brief create the parameters
  explicit BindParameters(ResourceMonitor& resourceMonitor, std::shared_ptr<arangodb::velocypack::Builder> builder);

  /// @brief destroy the parameters
  ~BindParameters();

  /// @brief mark a bind parameter as "used", and return its value.
  /// will return VPackSlice::noneSlice() if the bind parameter does not exist!
  arangodb::velocypack::Slice markUsed(std::string const& name) noexcept;
 
  /// @brief run a visitor function on all bind parameters
  void visit(std::function<void(std::string const& key, arangodb::velocypack::Slice value, bool used)> const& visitor) const;

  /// @brief return the bind parameters as passed by the user
  std::shared_ptr<arangodb::velocypack::Builder> builder() const;

  /// @brief create a hash value for the bind parameters
  uint64_t hash() const;

  /// @brief strip collection name prefixes from the parameters
  /// the values must be a VelocyPack array
  static void stripCollectionNames(arangodb::velocypack::Slice keys,
                                   std::string const& collectionName,
                                   arangodb::velocypack::Builder& result);

 private:
  /// @brief process the parameters
  void process();

  /// @brief calculates memory usage for a bind parameter
  std::size_t memoryUsage(std::string const& key, arangodb::velocypack::Slice value) const noexcept;

 private:
  arangodb::ResourceMonitor& _resourceMonitor;

  /// @brief the parameter values
  std::shared_ptr<arangodb::velocypack::Builder> _builder;

  /// @brief pointer to collection parameters
  BindParametersType _parameters;

  /// @brief internal state
  bool _processed;
};
}  // namespace aql
}  // namespace arangodb

#endif
