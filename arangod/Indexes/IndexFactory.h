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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_INDEXES_INDEX_FACTORY_H
#define ARANGOD_INDEXES_INDEX_FACTORY_H 1

#include "Basics/Result.h"
#include "VocBase/voc-types.h"

namespace arangodb {

class Index;
class LogicalCollection;

namespace velocypack {
class Builder;
class Slice;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief factory for comparing/instantiating/normalizing a definition for a
///        specific Index type
////////////////////////////////////////////////////////////////////////////////
struct IndexTypeFactory {
  virtual ~IndexTypeFactory() = default; // define to silence warning

  //////////////////////////////////////////////////////////////////////////////
  /// @brief determine if the two Index definitions will result in the same
  ///        index once instantiated
  //////////////////////////////////////////////////////////////////////////////
  virtual bool equal(
    velocypack::Slice const& lhs,
    velocypack::Slice const& rhs
  ) const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief instantiate an Index definition
  //////////////////////////////////////////////////////////////////////////////
  virtual Result instantiate(
    std::shared_ptr<Index>& index,
    LogicalCollection& collection,
    velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
  ) const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief normalize an Index definition prior to instantiation/persistence
  //////////////////////////////////////////////////////////////////////////////
  virtual Result normalize(
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  ) const = 0;
};

class IndexFactory {
 public:
  virtual ~IndexFactory() = default;

  /// @return 'factory' for 'type' was added successfully
  Result emplace(std::string const& type, IndexTypeFactory const& factory);

  virtual Result enhanceIndexDefinition(
    velocypack::Slice const definition,
    velocypack::Builder& normalized,
    bool isCreation,
    bool isCoordinator
  ) const;

  /// @return factory for the specified type or a failing placeholder if no such type
  IndexTypeFactory const& factory(std::string const& type) const noexcept;

  std::shared_ptr<Index> prepareIndexFromSlice(
    velocypack::Slice definition,
    bool generateKey,
    LogicalCollection& collection,
    bool isClusterConstructor
  ) const;

  /// @brief used to display storage engine capabilities
  virtual std::vector<std::string> supportedIndexes() const;

  /// @brief create system indexes primary / edge
  virtual void fillSystemIndexes(
    arangodb::LogicalCollection& col,
    std::vector<std::shared_ptr<arangodb::Index>>& systemIndexes
  ) const = 0;

  /// @brief create indexes from a list of index definitions
  virtual void prepareIndexes(
    LogicalCollection& col,
    arangodb::velocypack::Slice const& indexesSlice,
    std::vector<std::shared_ptr<arangodb::Index>>& indexes
  ) const = 0;

 protected:
  /// @brief clear internal factory/normalizer maps
  void clear();

  static TRI_idx_iid_t validateSlice(arangodb::velocypack::Slice info, 
                                     bool generateKey, 
                                     bool isClusterConstructor);

 private:
  std::unordered_map<std::string, IndexTypeFactory const*> _factories;
};

}  // namespace arangodb

#endif