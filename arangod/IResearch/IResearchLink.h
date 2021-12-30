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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "index/directory_reader.hpp"
#include "index/index_writer.hpp"
#include "store/directory.hpp"
#include "utils/utf8_path.hpp"

#include "IResearch/IResearchDataStore.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchVPackComparer.h"
#include "IResearch/IResearchViewMeta.h"
#include "Indexes/Index.h"
#include "Metrics/Fwd.h"
#include "RestServer/DatabasePathFeature.h"
#include "Transaction/Status.h"
#include "Utils/OperationOptions.h"
#include "VocBase/Identifiers/IndexId.h"

namespace arangodb {
namespace iresearch {
////////////////////////////////////////////////////////////////////////////////
/// @brief common base class for functionality required to link an ArangoDB
///        LogicalCollection with an IResearchView
////////////////////////////////////////////////////////////////////////////////
class IResearchLink : public IResearchDataStore {
 public:
  IResearchLink(IResearchLink const&) = delete;
  IResearchLink(IResearchLink&&) = delete;
  IResearchLink& operator=(IResearchLink const&) = delete;
  IResearchLink& operator=(IResearchLink&&) = delete;

  virtual ~IResearchLink();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief does this IResearch Link reference the supplied view
  ////////////////////////////////////////////////////////////////////////////////
  bool operator==(LogicalView const& view) const noexcept;
  bool operator!=(LogicalView const& view) const noexcept {
    return !(*this == view);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief does this iResearch Link match the meta definition
  ////////////////////////////////////////////////////////////////////////////////
  bool operator==(IResearchLinkMeta const& meta) const noexcept;
  bool operator!=(IResearchLinkMeta const& meta) const noexcept {
    return !(*this == meta);
  }

  static bool canBeDropped() {
    // valid for a link to be dropped from an ArangoSearch view
    return true;
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called when the iResearch Link is dropped
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  Result drop();

  static bool isHidden();  // arangodb::Index override
  static bool isSorted();  // arangodb::Index override

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called when the iResearch Link is loaded into memory
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  void load();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief index comparator, used by the coordinator to detect if the
  /// specified
  ///        definition is the same as this link
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  bool matchesDefinition(velocypack::Slice slice) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a jSON description of a IResearchLink object
  ///        elements are appended to an existing object
  //////////////////////////////////////////////////////////////////////////////
  Result properties(velocypack::Builder& builder, bool forPersistence) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief update runtime data processing properties (not persisted)
  /// @return success
  //////////////////////////////////////////////////////////////////////////////
  Result properties(IResearchViewMeta const& meta);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief ArangoSearch Link index type enum value
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  static Index::IndexType type();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief ArangoSearch Link index type string value
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  static char const* typeName();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called when the iResearch Link is unloaded from memory
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  Result unload();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief lookup referenced analyzer
  ////////////////////////////////////////////////////////////////////////////////
  AnalyzerPool::ptr findAnalyzer(AnalyzerPool const& analyzer) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize from the specified definition used in make(...)
  /// @return success
  ////////////////////////////////////////////////////////////////////////////////
  Result init(velocypack::Slice const& definition,
              InitCallback const& initCallback = {});

  ////////////////////////////////////////////////////////////////////////////////
  /// @return arangosearch internal format identifier
  ////////////////////////////////////////////////////////////////////////////////
  std::string_view format() const noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get stored values
  ////////////////////////////////////////////////////////////////////////////////
  IResearchViewStoredValues const& storedValues() const noexcept;

  /// @brief sets the _collectionName in Link meta. Used in cluster only to
  /// store linked collection name (as shard name differs from the cluster-wide
  /// collection name)
  /// @param name collection to set. Should match existing value of the
  /// _collectionName if it is not empty.
  /// @return true if name not existed in link before and was actually set by
  /// this call, false otherwise
  bool setCollectionName(irs::string_ref name) noexcept;

  /// @brief insert an ArangoDB document into an iResearch View using '_meta'
  /// params
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  Result insert(transaction::Methods& trx, LocalDocumentId const documentId,
                velocypack::Slice const doc);

  std::string const& getViewId() const noexcept;
  std::string const& getDbName() const;
  std::string const& getShardName() const noexcept;
  std::string getCollectionName() const;

  // TODO: Generalize for Link/Index
  struct LinkStats : Stats {
    LinkStats() = default;
    explicit LinkStats(Stats const& storeStats) : Stats(storeStats) {}

    void needName() const;
    void toPrometheus(std::string& result, std::string_view globals,
                      std::string_view labels) const;

   private:
    mutable bool _needName{false};
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get index stats for current snapshot
  ////////////////////////////////////////////////////////////////////////////////
  LinkStats stats() const;

 protected:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief construct an uninitialized IResearch link, must call init(...)
  /// after
  ////////////////////////////////////////////////////////////////////////////////
  IResearchLink(IndexId iid, LogicalCollection& collection);

  void updateStats(Stats const& stats) override;

  void insertStats() override;
  void removeStats() override;
  void invalidateQueryCache(TRI_vocbase_t* vocbase) override;

 private:
  metrics::Batch<LinkStats>* _linkStats;
  IResearchLinkMeta const _meta;
  std::string const _viewGuid;  // the identifier of the desired view
                                // (read-only, set via init())
};

}  // namespace iresearch
}  // namespace arangodb
