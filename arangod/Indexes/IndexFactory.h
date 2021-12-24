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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Result.h"
#include "Indexes/Index.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/voc-types.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
class Index;
class LogicalCollection;

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

/// @brief factory for comparing/instantiating/normalizing a definition for a
///        specific Index type
struct IndexTypeFactory {
  explicit IndexTypeFactory(application_features::ApplicationServer& server);
  virtual ~IndexTypeFactory() = default;  // define to silence warning

  /// @brief determine if the two Index definitions will result in the same
  ///        index once instantiated
  virtual bool equal(Index::IndexType type,
                     velocypack::Slice lhs,
                     velocypack::Slice rhs,
                     bool attributeOrderMatters) const;

  virtual bool equal(velocypack::Slice lhs,
                     velocypack::Slice rhs,
                     std::string const& dbname) const = 0;

  /// @brief instantiate an Index definition
  virtual std::shared_ptr<Index> instantiate(LogicalCollection& collection,
                                             velocypack::Slice definition,
                                             IndexId id,
                                             bool isClusterConstructor) const = 0;

  /// @brief normalize an Index definition prior to instantiation/persistence
  virtual Result normalize(
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation,
    TRI_vocbase_t const& vocbase) const = 0;

  /// @brief the order of attributes matters by default  
  virtual bool attributeOrderMatters() const {
    // can be overridden by specific indexes
    return true;
  }

 protected:
  application_features::ApplicationServer& _server;
};

class IndexFactory {
 public:
  IndexFactory(application_features::ApplicationServer&);
  virtual ~IndexFactory() = default;

  /// @brief returns if 'factory' for 'type' was added successfully
  Result emplace(std::string const& type, IndexTypeFactory const& factory);

  virtual Result enhanceIndexDefinition(
    velocypack::Slice definition,
    velocypack::Builder& normalized,
    bool isCreation,
    TRI_vocbase_t const& vocbase) const;

  /// @brief returns factory for the specified type or a failing placeholder if no such
  /// type
  IndexTypeFactory const& factory(std::string const& type) const noexcept;

  /// @brief returns the index created from the definition
  /// will throw if an error occurs
  std::shared_ptr<Index> prepareIndexFromSlice(velocypack::Slice definition, bool generateKey,
                                               LogicalCollection& collection,
                                               bool isClusterConstructor) const;

  /// @brief used to display storage engine capabilities
  virtual std::vector<std::string> supportedIndexes() const;

  /// @brief index name aliases (e.g. "persistent" => "hash", "skiplist" => "hash")
  /// used to display storage engine capabilities
  virtual std::unordered_map<std::string, std::string> indexAliases() const;

  /// @brief create system indexes primary / edge
  virtual void fillSystemIndexes(LogicalCollection& col,
                                 std::vector<std::shared_ptr<Index>>& systemIndexes) const = 0;

  /// @brief create indexes from a list of index definitions
  virtual void prepareIndexes(LogicalCollection& col,
                              velocypack::Slice indexesSlice,
                              std::vector<std::shared_ptr<Index>>& indexes) const = 0;

  static Result validateFieldsDefinition(velocypack::Slice definition,
                                         std::string const& attributeName,
                                         size_t minFields, size_t maxFields,
                                         bool allowSubAttributes = true);

  /// @brief process the fields list, deduplicate it, and add it to the json
  static Result processIndexFields(velocypack::Slice definition,
                                   velocypack::Builder& builder,
                                   size_t minFields, size_t maxFields, bool create,
                                   bool allowExpansion, bool allowSubAttributes);
  
  /// @brief process the extra fields list, deduplicate it, and add it to the json
  static Result processIndexExtraFields(velocypack::Slice definition,
                                        velocypack::Builder& builder,
                                        size_t minFields, size_t maxFields, bool create,
                                        bool allowSubAttributes);

  static void processIndexInBackground(velocypack::Slice definition, 
                                       velocypack::Builder& builder);

  /// @brief process the unique flag and add it to the json
  static void processIndexUniqueFlag(velocypack::Slice definition,
                                     velocypack::Builder& builder);

  /// @brief process the sparse flag and add it to the json
  static void processIndexSparseFlag(velocypack::Slice definition,
                                     velocypack::Builder& builder, bool create);

  /// @brief process the deduplicate flag and add it to the json
  static void processIndexDeduplicateFlag(velocypack::Slice definition,
                                          velocypack::Builder& builder);

  /// @brief process the geojson flag and add it to the json
  static void processIndexGeoJsonFlag(velocypack::Slice definition,
                                      velocypack::Builder& builder);

  /// @brief enhances the json of a hash, skiplist or persistent index
  static Result enhanceJsonIndexGeneric(velocypack::Slice definition,
                                        velocypack::Builder& builder, bool create);

  /// @brief enhances the json of a ttl index
  static Result enhanceJsonIndexTtl(velocypack::Slice definition,
                                    velocypack::Builder& builder, bool create);

  /// @brief enhances the json of a geo, geo1 or geo2 index
  static Result enhanceJsonIndexGeo(velocypack::Slice definition,
                                    velocypack::Builder& builder, bool create,
                                    int minFields, int maxFields);

  /// @brief enhances the json of a fulltext index
  static Result enhanceJsonIndexFulltext(velocypack::Slice definition,
                                         velocypack::Builder& builder, bool create);

  /// @brief enhances the json of a zkd index
  static Result enhanceJsonIndexZkd(arangodb::velocypack::Slice definition,
                                         arangodb::velocypack::Builder& builder, bool create);

 protected:
  /// @brief clear internal factory/normalizer maps
  void clear();

  static IndexId validateSlice(velocypack::Slice info,
                               bool generateKey, bool isClusterConstructor);

 protected:
  application_features::ApplicationServer& _server;
  std::unordered_map<std::string, IndexTypeFactory const*> _factories;
  std::unique_ptr<IndexTypeFactory> _invalid;
};

}  // namespace arangodb

