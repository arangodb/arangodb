//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__IRESEARCH_LINK_H
#define ARANGOD_IRESEARCH__IRESEARCH_LINK_H 1

#include "IResearchLinkMeta.h"
#include "IResearchView.h"

#include "Indexes/Index.h"

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

////////////////////////////////////////////////////////////////////////////////
/// @brief common base class for functionality required to link an ArangoDB
///        LogicalCollection with an IResearchView
////////////////////////////////////////////////////////////////////////////////
class IResearchLink {
 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief finds first link between specified collection and view
  ////////////////////////////////////////////////////////////////////////////////
  static std::shared_ptr<IResearchLink> find(
    LogicalCollection const& collection,
    LogicalView const& view
  );

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

  void afterTruncate(); // arangodb::Index override

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief insert a set of ArangoDB documents into an iResearch View using
  ///        '_meta' params
  ////////////////////////////////////////////////////////////////////////////////
  virtual void batchInsert(
    transaction::Methods* trx,
    std::vector<std::pair<LocalDocumentId, arangodb::velocypack::Slice>> const& batch,
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue = nullptr
  ); // arangodb::Index override

  bool canBeDropped() const; // arangodb::Index override

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called when the iResearch Link is dropped
  ////////////////////////////////////////////////////////////////////////////////
  arangodb::Result drop(); // arangodb::Index override

  bool hasBatchInsert() const; // arangodb::Index override
  bool hasSelectivityEstimate() const; // arangodb::Index override

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief insert an ArangoDB document into an iResearch View using '_meta' params
  ////////////////////////////////////////////////////////////////////////////////
  arangodb::Result insert(
    transaction::Methods* trx,
    LocalDocumentId const& documentId,
    VPackSlice const& doc,
    Index::OperationMode mode
  ); // arangodb::Index override

  bool isSorted() const; // arangodb::Index override
  
  bool isHidden() const { // arangodb::Index override
    return true; // always hide links
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the identifier for this link
  ////////////////////////////////////////////////////////////////////////////////
  TRI_idx_iid_t id() const noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a jSON description of a IResearchLink object
  ///        elements are appended to an existing object
  /// @return success or set TRI_set_errno(...) and return false
  ////////////////////////////////////////////////////////////////////////////////
  bool json(arangodb::velocypack::Builder& builder) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called when the iResearch Link is loaded into memory
  ////////////////////////////////////////////////////////////////////////////////
  void load(); // arangodb::Index override

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief index comparator, used by the coordinator to detect if the specified
  ///        definition is the same as this link
  ////////////////////////////////////////////////////////////////////////////////
  bool matchesDefinition(
    arangodb::velocypack::Slice const& slice
  ) const; // arangodb::Index override

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief amount of memory in bytes occupied by this iResearch Link
  ////////////////////////////////////////////////////////////////////////////////
  size_t memory() const; // arangodb::Index override

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove an ArangoDB document from an iResearch View
  ////////////////////////////////////////////////////////////////////////////////
  arangodb::Result remove(
    transaction::Methods* trx,
    arangodb::LocalDocumentId const& documentId,
    VPackSlice const& doc,
    Index::OperationMode mode
  ); // arangodb::Index override

  //////////////////////////////////////////////////////////////////////////////
  /// @brief remove an ArangoDB document from an iResearch View without
  /// document data
  //////////////////////////////////////////////////////////////////////////////
  arangodb::Result remove(
    transaction::Methods* trx,
    arangodb::LocalDocumentId const& documentId,
    Index::OperationMode mode
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief iResearch Link index type enum value
  ////////////////////////////////////////////////////////////////////////////////
  arangodb::Index::IndexType type() const; // arangodb::Index override

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief iResearch Link index type string value
  ////////////////////////////////////////////////////////////////////////////////
  char const* typeName() const; // arangodb::Index override

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called when the iResearch Link is unloaded from memory
  ////////////////////////////////////////////////////////////////////////////////
  arangodb::Result unload(); // arangodb::Index override

 protected:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief construct an uninitialized IResearch link, must call init(...) after
  ////////////////////////////////////////////////////////////////////////////////
  IResearchLink(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection& collection
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize from the specified definition used in make(...)
  /// @return success
  ////////////////////////////////////////////////////////////////////////////////
  arangodb::Result init(arangodb::velocypack::Slice const& definition);

  ////////////////////////////////////////////////////////////////////////////////
  /// @return the associated IResearch view or nullptr if not associated
  ////////////////////////////////////////////////////////////////////////////////
  const IResearchView* view() const;

 private:
  // FIXME TODO remove once View::updateProperties(...) will be fixed to write
  // the update delta into the WAL marker instead of the full persisted state
  // FIXME TODO remove #include "IResearchView.h"
  // friend arangodb::Result IResearchView::updateProperties(arangodb::velocypack::Slice const&, bool);
  friend class IResearchView;

  LogicalCollection& _collection; // the linked collection
  std::string _defaultGuid; // the identifier of the desired view (iff _view == nullptr)
  bool _dropCollectionInDestructor; // collection should be dropped from view in the destructor (for the case where init(..) is called folowed by distructor)
  TRI_idx_iid_t const _id; // the index identifier
  IResearchLinkMeta _meta; // how this collection should be indexed
  mutable irs::async_utils::read_write_mutex _mutex; // for use with _view to allow asynchronous disassociation
  IResearchView* _view; // effectively the IResearch datastore itself (nullptr == not associated)
  std::unique_lock<irs::async_utils::read_write_mutex::read_mutex> _viewLock; // prevent view deallocation (lock @ AsyncSelf)
}; // IResearchLink

NS_END // iresearch
NS_END // arangodb

#endif
