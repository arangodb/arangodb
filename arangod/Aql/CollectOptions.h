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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_COLLECT_OPTIONS_H
#define ARANGOD_AQL_COLLECT_OPTIONS_H 1

#include "Basics/Common.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
namespace aql {

/// @brief CollectOptions
struct CollectOptions {
  /// @brief selected aggregation method
  enum class CollectMethod { UNDEFINED, HASH, SORTED, DISTINCT, COUNT };

  /// @brief constructor, using default values
  CollectOptions() : method(CollectMethod::UNDEFINED) {}

  CollectOptions(CollectOptions const& other) : method(other.method) {}

  CollectOptions& operator=(CollectOptions const& other) {
    method = other.method;
    return *this;
  }

  /// @brief constructor
  explicit CollectOptions(arangodb::velocypack::Slice const&);

  /// @brief whether or not the method can be used
  bool canUseMethod(CollectMethod method) const;

  /// @brief whether or not the method should be used
  bool shouldUseMethod(CollectMethod method) const;

  /// @brief convert the options to VelocyPack
  void toVelocyPack(arangodb::velocypack::Builder&) const;

  /// @brief get the aggregation method from a string
  static CollectMethod methodFromString(std::string const&);

  /// @brief stringify the aggregation method
  static std::string methodToString(CollectOptions::CollectMethod method);

  CollectMethod method;
};

}  // namespace aql
}  // namespace arangodb

#endif
