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
#include "IResearch/IResearchViewMeta.h"
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

  bool canBeDropped() const {
    // valid for a link to be dropped from an ArangoSearch view
    return true;
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called when the iResearch Link is dropped
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  Result drop();

  bool hasSelectivityEstimate() const; // arangodb::Index override

  void afterCommit() override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief insert an ArangoDB document into an iResearch View using '_meta' params
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  Result insert(transaction::Methods& trx,
                LocalDocumentId const& documentId,
                velocypack::Slice const doc);

  bool isHidden() const;  // arangodb::Index override
  bool isSorted() const; // arangodb::Index override

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called when the iResearch Link is loaded into memory
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  void load();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief index comparator, used by the coordinator to detect if the specified
  ///        definition is the same as this link
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  bool matchesDefinition(velocypack::Slice const& slice) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a jSON description of a IResearchLink object
  ///        elements are appended to an existing object
  //////////////////////////////////////////////////////////////////////////////
  Result properties(velocypack::Builder& builder, bool forPersistence) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief update runtine data processing properties (not persisted)
  /// @return success
  //////////////////////////////////////////////////////////////////////////////
  Result properties(IResearchViewMeta const& meta);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove an ArangoDB document from an iResearch View
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  Result remove(transaction::Methods& trx,
                LocalDocumentId const& documentId,
                velocypack::Slice const doc);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief ArangoSearch Link index type enum value
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  Index::IndexType type() const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief ArangoSearch Link index type string value
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  char const* typeName() const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called when the iResearch Link is unloaded from memory
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  Result unload();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief lookup referenced analyzer
  ////////////////////////////////////////////////////////////////////////////////
  AnalyzerPool::ptr findAnalyzer(AnalyzerPool const& analyzer) const override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize from the specified definition used in make(...)
  /// @return success
  ////////////////////////////////////////////////////////////////////////////////
  Result init(velocypack::Slice const& definition,
              InitCallback const& initCallback = {});
              
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get stored values
  ////////////////////////////////////////////////////////////////////////////////
  IResearchViewStoredValues const& storedValues() const noexcept;

  /// @brief sets the _collectionName in Link meta. Used in cluster only to store
  /// linked collection name (as shard name differs from the cluster-wide collection name)
  /// @param name  collectioName to set. Should match existing value of  the _collectionName
  /// if it is not empty. 
  /// @return true if name not existed in link before and was actually set by this call,
  /// false otherwise
  bool setCollectionName(irs::string_ref name) noexcept;

 protected:

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief construct an uninitialized IResearch link, must call init(...)
  /// after
  ////////////////////////////////////////////////////////////////////////////////
  IResearchLink(IndexId iid, LogicalCollection& collection);

 private:
  IResearchLinkMeta const _meta; // how this collection should be indexed (read-only, set via init())
  std::string const _viewGuid; // the identifier of the desired view (read-only, set via init())
};  // IResearchLink

}  // namespace iresearch
}  // namespace arangodb

