////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Result.h"
#include "Indexes/IndexType.h"

#include <velocypack/Slice.h>

namespace arangodb {

struct Database;

namespace velocypack {

class Builder;

}  // namespace velocypack

/// @brief determine if two index definitions of the given type are equal
bool indexDefinitionsEqual(IndexType type, velocypack::Slice lhs,
                           velocypack::Slice rhs, bool attributeOrderMatters);

// deliberately not related to IndexTypeFactory (IndexFactory.h) by
// inheritance: a definition can never instantiate an index
struct IndexDefinition {
  explicit IndexDefinition(IndexType type) : _type(type) {}
  virtual ~IndexDefinition() = default;

  /// @brief determine if the two Index definitions will result in the same
  ///        index once instantiated
  virtual bool equal(velocypack::Slice lhs, velocypack::Slice rhs,
                     std::string const&) const {
    return indexDefinitionsEqual(_type, lhs, rhs, true);
  }

  /// @brief normalize an Index definition prior to instantiation/persistence
  virtual Result normalize(velocypack::Builder& normalized,
                           velocypack::Slice definition, bool isCreation,
                           Database const& vocbase) const = 0;

  /// @brief the order of attributes matters by default
  virtual bool attributeOrderMatters() const {
    // can be overridden by specific indexes
    return true;
  }

  IndexType const _type;
};

}  // namespace arangodb
