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
  virtual bool equal(Index::IndexType type, velocypack::Slice const& lhs, velocypack::Slice const& rhs,
                     bool attributeOrderMatters) const;

  virtual bool equal(velocypack::Slice const& lhs, velocypack::Slice const& rhs,
                     std::string const& dbname) const = 0;

  /// @brief instantiate an Index definition
  virtual std::shared_ptr<Index> instantiate(LogicalCollection& collection,
                                             velocypack::Slice const& definition, IndexId id,
                                             bool isClusterConstructor) const = 0;

  /// @brief normalize an Index definition prior to instantiation/persistence
  virtual Result normalize( // normalize definition
    velocypack::Builder& normalized, // normalized definition (out-param)
    velocypack::Slice definition, // source definition
    bool isCreation, // definition for index creation
    TRI_vocbase_t const& vocbase // index vocbase
  ) const = 0;

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

  virtual Result enhanceIndexDefinition( // normalizze definition
    velocypack::Slice const definition, // source definition
    velocypack::Builder& normalized, // normalized definition (out-param)
    bool isCreation, // definition for index creation
    TRI_vocbase_t const& vocbase // index vocbase
  ) const;

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
  virtual void fillSystemIndexes(arangodb::LogicalCollection& col,
                                 std::vector<std::shared_ptr<arangodb::Index>>& systemIndexes) const = 0;

  /// @brief create indexes from a list of index definitions
  virtual void prepareIndexes(LogicalCollection& col,
                              arangodb::velocypack::Slice const& indexesSlice,
                              std::vector<std::shared_ptr<arangodb::Index>>& indexes) const = 0;

  static Result validateFieldsDefinition(arangodb::velocypack::Slice definition, 
                                         size_t minFields, size_t maxFields,
                                         bool allowSubAttributes = true);

  /// @brief process the fields list, deduplicate it, and add it to the json
  static Result processIndexFields(arangodb::velocypack::Slice definition, 
                                   arangodb::velocypack::Builder& builder,
                                   size_t minFields, size_t maxFields, bool create,
                                   bool allowExpansion, bool allowSubAttributes = true);

  /// @brief process the unique flag and add it to the json
  static void processIndexUniqueFlag(arangodb::velocypack::Slice definition,
                                     arangodb::velocypack::Builder& builder);

  /// @brief process the sparse flag and add it to the json
  static void processIndexSparseFlag(arangodb::velocypack::Slice definition,
                                     arangodb::velocypack::Builder& builder, bool create);

  /// @brief process the deduplicate flag and add it to the json
  static void processIndexDeduplicateFlag(arangodb::velocypack::Slice definition, 
                                          arangodb::velocypack::Builder& builder);

  /// @brief process the geojson flag and add it to the json
  static void processIndexGeoJsonFlag(arangodb::velocypack::Slice definition,
                                      arangodb::velocypack::Builder& builder);

  /// @brief enhances the json of a hash, skiplist or persistent index
  static Result enhanceJsonIndexGeneric(arangodb::velocypack::Slice definition,
                                        arangodb::velocypack::Builder& builder, bool create);

  /// @brief enhances the json of a ttl index
  static Result enhanceJsonIndexTtl(arangodb::velocypack::Slice definition,
                                    arangodb::velocypack::Builder& builder, bool create);

  /// @brief enhances the json of a geo, geo1 or geo2 index
  static Result enhanceJsonIndexGeo(arangodb::velocypack::Slice definition,
                                    arangodb::velocypack::Builder& builder, bool create,
                                    int minFields, int maxFields);
  
  /// @brief enhances the json of a fulltext index
  static Result enhanceJsonIndexFulltext(arangodb::velocypack::Slice definition,
                                         arangodb::velocypack::Builder& builder, bool create);

 protected:
  /// @brief clear internal factory/normalizer maps
  void clear();

  static IndexId validateSlice(arangodb::velocypack::Slice info,
                               bool generateKey, bool isClusterConstructor);

 private:
  application_features::ApplicationServer& _server;
  std::unordered_map<std::string, IndexTypeFactory const*> _factories;
  std::unique_ptr<IndexTypeFactory> _invalid;
};

}  // namespace arangodb

#endif
