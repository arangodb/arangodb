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

#ifndef ARANGOD_AQL_BIND_PARAMETERS_H
#define ARANGOD_AQL_BIND_PARAMETERS_H 1

#include "Basics/Common.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace aql {

typedef std::unordered_map<std::string, std::pair<arangodb::velocypack::Slice, bool>> BindParametersType;

class BindParameters {
 public:
  BindParameters(BindParameters const&) = delete;
  BindParameters& operator=(BindParameters const&) = delete;
  BindParameters() = delete;

  /// @brief create the parameters
  explicit BindParameters(std::shared_ptr<arangodb::velocypack::Builder> builder) 
    : _builder(builder), _parameters(), _processed(false) {}

  /// @brief destroy the parameters
  ~BindParameters() = default;

 public:
  /// @brief return all parameters
  BindParametersType& get() {
    process();
    return _parameters;
  }

  /// @brief create a hash value for the bind parameters
  uint64_t hash() const;

  /// @brief strip collection name prefixes from the parameters
  /// the values must be a VelocyPack array
  static arangodb::velocypack::Builder StripCollectionNames(
      arangodb::velocypack::Slice const&, char const*);

 private:
  /// @brief process the parameters
  void process();

 private:
  /// @brief the parameter values
  std::shared_ptr<arangodb::velocypack::Builder> _builder;

  /// @brief pointer to collection parameters
  BindParametersType _parameters;

  /// @brief internal state
  bool _processed;
};
}
}

#endif
