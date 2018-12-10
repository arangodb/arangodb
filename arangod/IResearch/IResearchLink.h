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

#include "index/directory_reader.hpp"
#include "index/index_writer.hpp"
#include "store/directory.hpp"
#include "utils/utf8_path.hpp"

#include "IResearchLinkMeta.h"
#include "IResearchViewMeta.h"
#include "Indexes/Index.h"
#include "Transaction/Status.h"

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

class AsyncMeta; // forward declaration
class IResearchView; // forward declaration
template<typename T> class TypedResourceMutex; // forward declaration

////////////////////////////////////////////////////////////////////////////////
/// @brief common base class for functionality required to link an ArangoDB
///        LogicalCollection with an IResearchView
////////////////////////////////////////////////////////////////////////////////
class IResearchLink {
 public:
  typedef std::shared_ptr<TypedResourceMutex<IResearchLink>> AsyncLinkPtr;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a snapshot representation of the data-store
  ///        locked to prevent data store deallocation
  //////////////////////////////////////////////////////////////////////////////
  class Snapshot {
   public:
    Snapshot() noexcept {} // on-default implementation required for MacOS
    Snapshot(
      std::unique_lock<irs::async_utils::read_write_mutex::read_mutex>&& lock,
      irs::directory_reader&& reader
    ) noexcept: _lock(std::move(lock)), _reader(std::move(reader)) {
      TRI_ASSERT(_lock.owns_lock());
    }
    operator irs::directory_reader const&() const noexcept { return _reader; }

   private:
    std::unique_lock<irs::async_utils::read_write_mutex::read_mutex> _lock; // lock preventing data store dealocation
    const irs::directory_reader _reader;
  };

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
    arangodb::transaction::Methods& trx,
    std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> const& batch,
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue
  ); // arangodb::Index override

  bool canBeDropped() const; // arangodb::Index override

  //////////////////////////////////////////////////////////////////////////////
  /// @return the associated collection
  //////////////////////////////////////////////////////////////////////////////
  arangodb::LogicalCollection& collection() const noexcept; // arangodb::Index override

  //////////////////////////////////////////////////////////////////////////////
  /// @brief mark the current data store state as te latest valid state
  //////////////////////////////////////////////////////////////////////////////
  arangodb::Result commit();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief invoke internal data store consolidation with the specified policy
  /// @return success
  //////////////////////////////////////////////////////////////////////////////
  arangodb::Result consolidate(
    IResearchViewMeta::ConsolidationPolicy const& policy,
    irs::merge_writer::flush_progress_t const& progress,
    bool runCleanupAfterConsolidation
  );

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
    arangodb::transaction::Methods& trx,
    arangodb::LocalDocumentId const& documentId,
    arangodb::velocypack::Slice const& doc,
    arangodb::Index::OperationMode mode
  ); // arangodb::Index override

  bool isPersistent() const; // arangodb::Index override
  bool isSorted() const; // arangodb::Index override

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
    arangodb::transaction::Methods& trx,
    arangodb::LocalDocumentId const& documentId,
    arangodb::velocypack::Slice const& doc,
    arangodb::Index::OperationMode mode
  ); // arangodb::Index override

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief 'this' for the lifetime of the link data-store
  ///        for use with asynchronous calls, e.g. callbacks, view
  ///////////////////////////////////////////////////////////////////////////////
  AsyncLinkPtr self() const { return _asyncSelf; }

  //////////////////////////////////////////////////////////////////////////////
  /// @return pointer to an index reader containing the data store current
  ///         record snapshot
  ///         (nullptr == no data store snapshot availabe, e.g. error)
  //////////////////////////////////////////////////////////////////////////////
  Snapshot snapshot() const;

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
  IResearchLink(TRI_idx_iid_t iid, arangodb::LogicalCollection& collection);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize from the specified definition used in make(...)
  /// @return success
  ////////////////////////////////////////////////////////////////////////////////
  arangodb::Result init(arangodb::velocypack::Slice const& definition);

  ////////////////////////////////////////////////////////////////////////////////
  /// @return the associated IResearch view or nullptr if not associated
  ////////////////////////////////////////////////////////////////////////////////
  std::shared_ptr<IResearchView> view() const;

 private:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the underlying iresearch data store
  //////////////////////////////////////////////////////////////////////////////
  struct DataStore {
    irs::directory::ptr _directory;
    irs::utf8_path _path;
    irs::directory_reader _reader;
    irs::index_writer::ptr _writer;
    operator bool() const noexcept { return _directory && _writer; }
  };

  AsyncLinkPtr _asyncSelf; // 'this' for the lifetime of the view (for use with asynchronous calls)
  arangodb::LogicalCollection& _collection; // the linked collection
  DataStore _dataStore; // the iresearch data store, protected by _asyncSelf->mutex()
  TRI_idx_iid_t const _id; // the index identifier
  std::atomic<bool> _inRecovery; // the link is currently in the WAL recovery state
  IResearchLinkMeta const _meta; // how this collection should be indexed (read-only, set via init())
  std::mutex _readerMutex; // prevents query cache double invalidation
  std::function<void(arangodb::transaction::Methods& trx, arangodb::transaction::Status status)> _trxCallback; // for insert(...)/remove(...)
  std::string const _viewGuid; // the identifier of the desired view (read-only, set via init())

  arangodb::Result initDataStore(IResearchView const& view);
}; // IResearchLink

NS_END // iresearch
NS_END // arangodb

#endif