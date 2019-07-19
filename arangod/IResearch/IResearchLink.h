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
#include "IResearchVPackComparer.h"
#include "Indexes/Index.h"
#include "Transaction/Status.h"

namespace arangodb {
namespace iresearch {

class AsyncMeta; // forward declaration
class IResearchFeature; // forward declaration
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
    Snapshot() noexcept {}  // non-default implementation required for MacOS
    Snapshot(std::unique_lock<irs::async_utils::read_write_mutex::read_mutex>&& lock,
             irs::directory_reader&& reader) noexcept
        : _lock(std::move(lock)), _reader(std::move(reader)) {
      TRI_ASSERT(_lock.owns_lock());
    }
    operator irs::directory_reader const&() const noexcept { return _reader; }

   private:
    std::unique_lock<irs::async_utils::read_write_mutex::read_mutex> _lock;  // lock preventing data store dealocation
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

  void afterTruncate();  // arangodb::Index override

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief insert a set of ArangoDB documents into an iResearch View using
  ///        '_meta' params
  ////////////////////////////////////////////////////////////////////////////////
  virtual void batchInsert(
      arangodb::transaction::Methods& trx,
      std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> const& batch,
      std::shared_ptr<arangodb::basics::LocalTaskQueue> queue);  // arangodb::Index override

  bool canBeDropped() const;  // arangodb::Index override

  //////////////////////////////////////////////////////////////////////////////
  /// @return the associated collection
  //////////////////////////////////////////////////////////////////////////////
  arangodb::LogicalCollection& collection() const noexcept; // arangodb::Index override

  //////////////////////////////////////////////////////////////////////////////
  /// @brief mark the current data store state as the latest valid state
  //////////////////////////////////////////////////////////////////////////////
  arangodb::Result commit();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called when the iResearch Link is dropped
  ////////////////////////////////////////////////////////////////////////////////
  arangodb::Result drop(); // arangodb::Index override

  bool hasSelectivityEstimate() const; // arangodb::Index override

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the identifier for this link
  //////////////////////////////////////////////////////////////////////////////
  TRI_idx_iid_t id() const noexcept { return _id; }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief insert an ArangoDB document into an iResearch View using '_meta' params
  ////////////////////////////////////////////////////////////////////////////////
  arangodb::Result insert( // insert document
    arangodb::transaction::Methods& trx, // transaction
    arangodb::LocalDocumentId const& documentId, // document identifier
    arangodb::velocypack::Slice const& doc, // document
    arangodb::Index::OperationMode mode // insert mode
  ); // arangodb::Index override

  bool isHidden() const;  // arangodb::Index override
  bool isSorted() const; // arangodb::Index override

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called when the iResearch Link is loaded into memory
  ////////////////////////////////////////////////////////////////////////////////
  void load(); // arangodb::Index override

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief index comparator, used by the coordinator to detect if the specified
  ///        definition is the same as this link
  ////////////////////////////////////////////////////////////////////////////////
  bool matchesDefinition( // matches
    arangodb::velocypack::Slice const& slice // other definition
  ) const; // arangodb::Index override

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief amount of memory in bytes occupied by this iResearch Link
  ////////////////////////////////////////////////////////////////////////////////
  size_t memory() const; // arangodb::Index override

  //////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a jSON description of a IResearchLink object
  ///        elements are appended to an existing object
  //////////////////////////////////////////////////////////////////////////////
  arangodb::Result properties( // get link properties
    arangodb::velocypack::Builder& builder, // output buffer
    bool forPersistence // properties for persistance
  ) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief update runtine data processing properties (not persisted)
  /// @return success
  //////////////////////////////////////////////////////////////////////////////
  arangodb::Result properties(IResearchViewMeta const& meta);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove an ArangoDB document from an iResearch View
  ////////////////////////////////////////////////////////////////////////////////
  arangodb::Result remove( // remove document
    arangodb::transaction::Methods& trx, // transaction
    arangodb::LocalDocumentId const& documentId, // document id
    arangodb::velocypack::Slice const& doc, //document body
    arangodb::Index::OperationMode mode // operation mode
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

  //////////////////////////////////////////////////////////////////////////////
  /// @brief called with the contents of the WAL 'Flush' marker
  /// @note used by IResearchFeature when processing WAL Flush markers
  //////////////////////////////////////////////////////////////////////////////
  arangodb::Result walFlushMarker(arangodb::velocypack::Slice const& value);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called when the iResearch Link is unloaded from memory
  ////////////////////////////////////////////////////////////////////////////////
  arangodb::Result unload(); // arangodb::Index override

 protected:
  typedef std::function<void(irs::directory&)> InitCallback;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief construct an uninitialized IResearch link, must call init(...)
  /// after
  ////////////////////////////////////////////////////////////////////////////////
  IResearchLink(TRI_idx_iid_t iid, arangodb::LogicalCollection& collection);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize from the specified definition used in make(...)
  /// @return success
  ////////////////////////////////////////////////////////////////////////////////
  arangodb::Result init(
    arangodb::velocypack::Slice const& definition,
    InitCallback const& initCallback = {}
  );

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the current data-store recovery state of the link
  //////////////////////////////////////////////////////////////////////////////
  enum class RecoveryState {
    BEFORE_CHECKPOINT, // in recovery but before the FS checkpoint was seen
    DURING_CHECKPOINT, // in recovery, FS checkpoint was seen but before the next WAL checkpoint was seen
    AFTER_CHECKPOINT, // in recovery, FS checkpoint was seen and the next WAL checkpoint was seen
    DONE, // not in recovery
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the underlying iresearch data store
  //////////////////////////////////////////////////////////////////////////////
  struct DataStore {
    irs::directory::ptr _directory;
    IResearchViewMeta _meta; // runtime meta for a data store (not persisted)
    irs::async_utils::read_write_mutex _mutex; // for use with member '_meta'
    irs::utf8_path _path;
    irs::directory_reader _reader;
    std::atomic<RecoveryState> _recovery;
    std::string _recovery_range_start; // previous to last successful WAL 'Flush'
    irs::directory_reader _recovery_reader; // last successful WAL 'Flush'
    irs::index_file_refs::ref_t _recovery_ref; // ref at the checkpoint file
    irs::index_writer::ptr _writer;
    operator bool() const noexcept { return _directory && _writer; }

    void resetDataStore() noexcept { // reset all underlying readers to release file handles 
      _reader.reset(); 
      _writer.reset();
      _recovery_reader.reset();
      _directory.reset();
    } 
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief run filesystem cleanup on the data store
  /// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
  //////////////////////////////////////////////////////////////////////////////
  arangodb::Result cleanupUnsafe();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief mark the current data store state as the latest valid state
  /// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
  //////////////////////////////////////////////////////////////////////////////
  arangodb::Result commitUnsafe();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief run segment consolidation on the data store
  /// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
  //////////////////////////////////////////////////////////////////////////////
  arangodb::Result consolidateUnsafe( // consolidate segments
    IResearchViewMeta::ConsolidationPolicy const& policy, // policy to apply
    irs::merge_writer::flush_progress_t const& progress // policy progress to use
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief initialize the data store with a new or from an existing directory
  //////////////////////////////////////////////////////////////////////////////
  arangodb::Result initDataStore(InitCallback const& initCallback, bool sorted);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief set up asynchronous maintenance tasks
  //////////////////////////////////////////////////////////////////////////////
  void setupLinkMaintenance();

  VPackComparer _comparer;
  IResearchFeature* _asyncFeature; // the feature where async jobs were registered (nullptr == no jobs registered)
  AsyncLinkPtr _asyncSelf; // 'this' for the lifetime of the link (for use with asynchronous calls)
  std::atomic<bool> _asyncTerminate; // trigger termination of long-running async jobs
  arangodb::LogicalCollection& _collection; // the linked collection
  DataStore _dataStore; // the iresearch data store, protected by _asyncSelf->mutex()
  std::function<arangodb::Result(arangodb::velocypack::Slice const&, TRI_voc_tick_t)> _flushCallback; // for writing 'Flush' marker during commit (guaranteed valid by init)
  TRI_idx_iid_t const _id; // the index identifier
  IResearchLinkMeta const _meta; // how this collection should be indexed (read-only, set via init())
  std::mutex _readerMutex; // prevents query cache double invalidation
  std::function<void(arangodb::transaction::Methods& trx, arangodb::transaction::Status status)> _trxCallback; // for insert(...)/remove(...)
  std::string const _viewGuid; // the identifier of the desired view (read-only, set via init())
};  // IResearchLink

}  // namespace iresearch
}  // namespace arangodb

#endif
