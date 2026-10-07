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
#include "Indexes/IIndexFactory.h"
#include "Indexes/Index.h"
#include "Indexes/IndexDefinition.h"
#include "VocBase/Identifiers/IndexId.h"

#include <functional>
#include <utility>

namespace arangodb {

struct Database;
class Index;
class LogicalCollection;

namespace application_features {

class ApplicationServer;

}  // namespace application_features

class IndexTypeCatalog;
namespace velocypack {

class Builder;
class Slice;

}  // namespace velocypack
namespace helpers {

IndexId extractId(velocypack::Slice slice) noexcept;
std::string_view extractName(velocypack::Slice slice) noexcept;

}  // namespace helpers

class IndexFactory : public IIndexFactory {
 public:
  IndexFactory(application_features::ApplicationServer&,
               IndexTypeCatalog const& catalog);
  ~IndexFactory() override = default;

  IndexTypeCatalog const& catalog() const noexcept { return _catalog; }

  // the arangosearch link is created by a function IResearchFeature injects
  // at startup, not by the engines directly - see the comment on
  // IIndexFactory::createIResearchLink
  using LinkCreator = std::function<std::shared_ptr<Index>(
      LogicalCollection&, velocypack::Slice, IndexId, bool)>;
  void setLinkCreator(LinkCreator creator);
  std::shared_ptr<Index> createIResearchLink(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool isClusterConstructor) const override;

  virtual Result enhanceIndexDefinition(velocypack::Slice definition,
                                        velocypack::Builder& normalized,
                                        bool isCreation,
                                        Database const& vocbase) const;

  // engine-specific step run after a type's normalize() succeeds and before
  // the normalized definition is closed (e.g. RocksDB adds the objectId that
  // becomes part of the persisted definition); no-op by default
  virtual void finalizeDefinition(velocypack::Builder& normalized,
                                  velocypack::Slice definition,
                                  bool isCreation) const {}

  /// @brief returns the index created from the definition
  /// will throw if an error occurs
  std::shared_ptr<Index> prepareIndexFromSlice(velocypack::Slice definition,
                                               bool generateKey,
                                               LogicalCollection& collection,
                                               bool isClusterConstructor) const;

  /// @brief used to display storage engine capabilities
  virtual std::vector<std::string_view> supportedIndexes(
      uint32_t apiVersion) const;

  /// @brief index name aliases (e.g. "persistent" => "hash", "skiplist" =>
  /// "hash") used to display storage engine capabilities
  virtual std::vector<std::pair<std::string_view, std::string_view>>
  indexAliases(uint32_t apiVersion) const;

  /// @brief create system indexes primary / edge
  virtual void fillSystemIndexes(
      LogicalCollection& col,
      std::vector<std::shared_ptr<Index>>& systemIndexes) const = 0;

  /// @brief create indexes from a list of index definitions
  virtual void prepareIndexes(
      LogicalCollection& col, velocypack::Slice indexesSlice,
      std::vector<std::shared_ptr<Index>>& indexes) const = 0;

  static Result validateFieldsDefinition(velocypack::Slice definition,
                                         std::string const& attributeName,
                                         size_t minFields, size_t maxFields,
                                         bool allowSubAttributes,
                                         bool allowIdAttribute);

  /// @brief process the "fields" list, deduplicate it, and add it to the json
  static Result processIndexFields(velocypack::Slice definition,
                                   velocypack::Builder& builder,
                                   size_t minFields, size_t maxFields,
                                   bool create, bool allowExpansion,
                                   bool allowSubAttributes,
                                   bool allowIdAttribute);

  /// @brief process the "storedValues" list, deduplicate it, and add it to the
  /// json
  static Result processIndexStoredValues(velocypack::Slice definition,
                                         velocypack::Builder& builder,
                                         size_t minFields, size_t maxFields,
                                         bool create, bool allowSubAttributes,
                                         bool allowOverlappingFields);

  /// @brief process the "cacheEnabled" flag and add it to the json
  static void processIndexCacheEnabled(velocypack::Slice definition,
                                       velocypack::Builder& builder);

  /// @brief process the "inBackground" flag and add it to the json
  static void processIndexInBackground(velocypack::Slice definition,
                                       velocypack::Builder& builder);

  /// @brief Default number of threads to use for index creation
  static constexpr size_t kDefaultParallelism = 2;

  // FIXME(gnusi): determine at runtime
  /// @brief Max possible number of threads for index creation.
  static constexpr size_t kMaxParallelism = 16;

  /// @brief process the "parallelism" value and add it to the json
  static void processIndexParallelism(velocypack::Slice definition,
                                      velocypack::Builder& builder);

  /// @brief process the "unique" flag and add it to the json
  static void processIndexUniqueFlag(velocypack::Slice definition,
                                     velocypack::Builder& builder);

  /// @brief process the "sparse" flag and add it to the json
  static void processIndexSparseFlag(velocypack::Slice definition,
                                     velocypack::Builder& builder, bool create);

  /// @brief process the "deduplicate" flag and add it to the json
  static void processIndexDeduplicateFlag(velocypack::Slice definition,
                                          velocypack::Builder& builder);

  /// @brief process the "geojson" flag and add it to the json
  static void processIndexGeoJsonFlag(velocypack::Slice definition,
                                      velocypack::Builder& builder);

  /// @brief process the "legacyPolygons" flag and add it to the json
  static void processIndexLegacyPolygonsFlag(velocypack::Slice definition,
                                             velocypack::Builder& builder);

  /// @brief enhances the json of a hash, skiplist or persistent index
  static Result enhanceJsonIndexGeneric(velocypack::Slice definition,
                                        velocypack::Builder& builder,
                                        bool create);

  /// @brief enhances the json of a ttl index
  static Result enhanceJsonIndexTtl(velocypack::Slice definition,
                                    velocypack::Builder& builder, bool create);

  /// @brief enhances the json of a geo, geo1 or geo2 index
  static Result enhanceJsonIndexGeo(velocypack::Slice definition,
                                    velocypack::Builder& builder, bool create,
                                    int minFields, int maxFields);

  /// @brief enhances the json of a fulltext index
  static Result enhanceJsonIndexFulltext(velocypack::Slice definition,
                                         velocypack::Builder& builder,
                                         bool create);

  /// @brief enhances the json of a mdi
  static Result enhanceJsonIndexMdi(arangodb::velocypack::Slice definition,
                                    arangodb::velocypack::Builder& builder,
                                    bool create);
  static Result enhanceJsonIndexMdiPrefixed(
      arangodb::velocypack::Slice definition,
      arangodb::velocypack::Builder& builder, bool create);

  /// @brief enhances the json of a vector index
  static Result enhanceJsonIndexVector(arangodb::velocypack::Slice definition,
                                       arangodb::velocypack::Builder& builder,
                                       bool create);

 protected:
  static IndexId validateSlice(velocypack::Slice info, bool generateKey,
                               bool isClusterConstructor);

 protected:
  application_features::ApplicationServer& _server;
  IndexTypeCatalog const& _catalog;
  LinkCreator _linkCreator;
};

}  // namespace arangodb
