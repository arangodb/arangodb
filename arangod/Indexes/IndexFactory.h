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

class IndexFactory {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief typedef for a Index factory function
  /// This typedef is used when registering the factory function for any index
  /// type. The factory function is called when a index is first created or
  /// re-opened after a server restart. The VelocyPack Slice will contain all
  /// information about the indexs' general and implementation-specific
  /// properties.
  //////////////////////////////////////////////////////////////////////////////
  typedef std::function<std::shared_ptr<Index>(
    LogicalCollection* collection,
    velocypack::Slice const& definition, // index definition
    TRI_idx_iid_t id,
    bool isClusterConstructor
  )> IndexTypeFactory;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief typedef for a Index definition normalizer function
  /// This typedef is used when registering the normalizer function for any
  /// index type. The normalizer function is called when a index definition
  /// needs to be normalized before using it to create the index. The resulting
  /// VelocyBuilder will contain the 'type' that should be used for index
  /// factory lookup (if no normalizer is registered then use the original type)
  //////////////////////////////////////////////////////////////////////////////
  typedef std::function<Result(
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  )> IndexNormalizer;

  IndexFactory() = default;
  IndexFactory(IndexFactory const&) = delete;
  IndexFactory& operator=(IndexFactory const&) = delete;

  virtual ~IndexFactory() = default;

  /// @return 'factory' for 'type' was added successfully
  Result emplaceFactory(
    std::string const& type,
    IndexTypeFactory const& factory
  );

  /// @return 'normalizer' for 'type' was added successfully
  Result emplaceNormalizer(
    std::string const& type,
    IndexNormalizer const& normalizer
  );

  Result enhanceIndexDefinition(
    velocypack::Slice const definition,
    velocypack::Builder& normalized,
    bool isCreation,
    bool isCoordinator
  ) const;

  std::shared_ptr<Index> prepareIndexFromSlice(
    velocypack::Slice definition,
    bool generateKey,
    LogicalCollection* collection,
    bool isClusterConstructor
  ) const;

  /// @brief used to display storage engine capabilities
  virtual std::vector<std::string> supportedIndexes() const;

  /// @brief create system indexes primary / edge
  virtual void fillSystemIndexes(arangodb::LogicalCollection*,
                                 std::vector<std::shared_ptr<arangodb::Index>>&) const = 0;

  /// @brief create indexes from a list of index definitions
  virtual void prepareIndexes(LogicalCollection* col, velocypack::Slice const&,
                              std::vector<std::shared_ptr<arangodb::Index>>&) const = 0;

 protected:
  /// @brief clear internal factory/normalizer maps
  void clear();

  static TRI_idx_iid_t validateSlice(arangodb::velocypack::Slice info, 
                                     bool generateKey, 
                                     bool isClusterConstructor);

 private:
  std::unordered_map<std::string, IndexTypeFactory> _factories;
  std::unordered_map<std::string, IndexNormalizer> _normalizers;
};

}  // namespace arangodb

#endif