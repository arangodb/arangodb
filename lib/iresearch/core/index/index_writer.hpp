////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>
#include <string_view>

#include "formats/formats.hpp"
#include "index/column_info.hpp"
#include "index/directory_reader.hpp"
#include "index/field_meta.hpp"
#include "index/index_features.hpp"
#include "index/index_meta.hpp"
#include "index/index_reader_options.hpp"
#include "index/merge_writer.hpp"
#include "index/segment_reader.hpp"
#include "index/segment_writer.hpp"
#include "search/filter.hpp"
#include "utils/async_utils.hpp"
#include "utils/noncopyable.hpp"
#include "utils/object_pool.hpp"
#include "utils/string.hpp"
#include "utils/thread_utils.hpp"
#include "utils/wait_group.hpp"

#include <absl/container/flat_hash_map.h>

namespace irs {

class Comparer;
struct directory;

// Defines how index writer should be opened
enum OpenMode {
  // Creates new index repository. In case if repository already
  // exists, all contents will be cleared.
  OM_CREATE = 1,

  // Opens existing index repository. In case if repository does not
  // exists, error will be generated.
  OM_APPEND = 2,
};

ENABLE_BITMASK_ENUM(OpenMode);

// A set of candidates denoting an instance of consolidation
using Consolidation = std::vector<const SubReader*>;
using ConsolidationView = std::span<const SubReader* const>;

// segments that are under consolidation
using ConsolidatingSegments = absl::flat_hash_set<std::string_view>;

// Mark consolidation candidate segments matching the current policy
// candidates the segments that should be consolidated
// in: segment candidates that may be considered by this policy
// out: actual segments selected by the current policy
// dir the segment directory
// meta the index meta containing segments to be considered
// Consolidating_segments segments that are currently in progress
// of consolidation
// Final candidates are all segments selected by at least some policy.
// favorCleanupOverMerge indicates whether to consolidate segments
// that are suitable candidates to perform cleanup or to consolidate those
// that are a better candidate to merge.
// Cleanup reclaims disk space of deleted documents whereas the merge
// reduces the total no. of segment files on disk.
using ConsolidationPolicy =
  std::function<void(
                  Consolidation& candidates,
                  const IndexReader& index,
                  const ConsolidatingSegments& consolidating_segments,
                  bool favorCleanupOverMerge)>;

enum class ConsolidationError : uint32_t {
  // Consolidation failed
  FAIL = 0,

  // Consolidation successfully finished
  OK,

  // Consolidation was scheduled for the upcoming commit
  PENDING
};

// Represents result of a consolidation
struct ConsolidationResult {
  // Number of candidates
  size_t size{0};

  // Error code
  ConsolidationError error{ConsolidationError::FAIL};

  // intentionally implicit
  operator bool() const noexcept { return error != ConsolidationError::FAIL; }
};

// Options the the writer should use for segments
struct SegmentOptions {
  // Segment acquisition requests will block and wait for free segments
  // after this many segments have been acquired e.g. via GetBatch()
  // 0 == unlimited
  size_t segment_count_max{0};

  // Flush the segment to the repository after its in-memory size
  // grows beyond this byte limit, in-flight documents will still be
  // written to the segment before flush
  // 0 == unlimited
  size_t segment_memory_max{0};

  // Flush the segment to the repository after its total document
  // count (live + masked) grows beyond this byte limit, in-flight
  // documents will still be written to the segment before flush
  // 0 == unlimited
  uint32_t segment_docs_max{0};
};

// Progress report callback types for commits.
using ProgressReportCallback =
  std::function<void(std::string_view phase, size_t current, size_t total)>;

// Functor for creating payload. Operation tick is provided for
// payload generation.
using PayloadProvider = std::function<bool(uint64_t, bstring&)>;

// Options the the writer should use after creation
struct IndexWriterOptions : public SegmentOptions {
  // Options for snapshot management
  IndexReaderOptions reader_options;

  // Returns column info for a feature the writer should use for
  // columnstore
  FeatureInfoProvider features;

  // Returns column info the writer should use for columnstore
  ColumnInfoProvider column_info;

  // Provides payload for index_meta created by writer
  PayloadProvider meta_payload_provider;

  // Comparator defines physical order of documents in each segment
  // produced by an index_writer.
  // empty == use default system sorting order
  const Comparer* comparator{nullptr};

  // Number of free segments cached in the segment pool for reuse
  // 0 == do not cache any segments, i.e. always create new segments
  size_t segment_pool_size{128};  // arbitrary size

  // Acquire an exclusive lock on the repository to guard against index
  // corruption from multiple index_writers
  bool lock_repository{true};

  IndexWriterOptions() {}  // compiler requires non-default definition
};

struct CommitInfo {
  uint64_t tick = writer_limits::kMaxTick;
  ProgressReportCallback progress;
  bool reopen_columnstore = false;
};

// The object is using for indexing data. Only one writer can write to
// the same directory simultaneously.
// Thread safe.
class IndexWriter : private util::noncopyable {
 public:
  struct SegmentContext;

 private:
  struct FlushContext;

  using FlushContextPtr =
    std::unique_ptr<FlushContext, void (*)(FlushContext*)>;

  // Disallow using public constructor
  struct ConstructToken {
    explicit ConstructToken() = default;
  };

  class ActiveSegmentContext {
   public:
    ActiveSegmentContext() = default;
    ActiveSegmentContext(
      std::shared_ptr<SegmentContext> segment,
      std::atomic_size_t& segments_active,
      // the FlushContext the SegmentContext is currently registered with
      FlushContext* flush = nullptr,
      // the segment offset in flush->pending_segments_
      size_t pending_segment_offset = writer_limits::kInvalidOffset) noexcept;
    ActiveSegmentContext(ActiveSegmentContext&& other) noexcept;
    ActiveSegmentContext& operator=(ActiveSegmentContext&& other) noexcept;

    ~ActiveSegmentContext();

    auto* Segment() const noexcept { return segment_.get(); }
    auto* Segment() noexcept { return segment_.get(); }
    auto* Flush() noexcept { return flush_; }

   private:
    friend struct FlushContext;  // for FlushContext::AddToPending(...)

    std::shared_ptr<SegmentContext> segment_;
    // reference to IndexWriter::segments_active_
    std::atomic_size_t* segments_active_{nullptr};
    // nullptr will not match any FlushContext
    FlushContext* flush_{nullptr};
    // segment offset in flush_->pending_segments_
    size_t pending_segment_offset_{writer_limits::kInvalidOffset};
  };

  static_assert(std::is_nothrow_move_constructible_v<ActiveSegmentContext>);
  static_assert(std::is_nothrow_move_assignable_v<ActiveSegmentContext>);

 public:
  // Additional information required for remove/replace requests
  struct QueryContext {
    using FilterPtr = std::shared_ptr<const irs::filter>;

    QueryContext() = default;

    static constexpr uintptr_t kDone = 0;
    static constexpr uintptr_t kReplace = std::numeric_limits<uintptr_t>::max();

    QueryContext(FilterPtr filter, uint64_t tick, uintptr_t data)
      : filter{std::move(filter)}, tick{tick}, data{data} {
      IRS_ASSERT(this->filter != nullptr);
    }
    QueryContext(const irs::filter& filter, uint64_t tick, size_t data)
      : QueryContext{{FilterPtr{}, &filter}, tick, data} {}
    QueryContext(irs::filter::ptr&& filter, uint64_t tick, size_t data)
      : QueryContext{FilterPtr{std::move(filter)}, tick, data} {}

    // keep a handle to the filter for the case when this object has ownership
    FilterPtr filter;
    uint64_t tick;

    bool IsDone() const noexcept { return data == kDone; }
    void ForceDone() noexcept { data = kDone; }
    void Done() noexcept {
      IRS_ASSERT(!IsDone());
      Done(this);
    }
    void DependsOn(QueryContext& query) noexcept {
      IRS_ASSERT(!IsDone());
      if (query.data == kDone) {
        Done(this);
      } else {
        IRS_ASSERT(query.data == kReplace);
        query.data = reinterpret_cast<uintptr_t>(this);
      }
    }

   private:
    uintptr_t data{kDone};

    static void Done(QueryContext* query) noexcept {
      while (true) {
        auto next = std::exchange(query->data, kDone);
        IRS_ASSERT(next != kDone);
        if (next == kReplace) {
          return;
        }
        query = reinterpret_cast<QueryContext*>(next);
        IRS_ASSERT(query != nullptr);
      }
    }
  };
  static_assert(std::is_nothrow_move_constructible_v<QueryContext>);

  // A context allowing index modification operations.
  // The object is non-thread-safe, each thread should use its own
  // separate instance.
  class Document : private util::noncopyable {
   public:
    Document(SegmentContext& segment, segment_writer::DocContext doc,
             QueryContext* query = nullptr);

    Document(Document&&) = default;
    Document& operator=(Document&&) = delete;

    ~Document() noexcept;

    // Return current state of the object
    // Note that if the object is in an invalid state all further operations
    // will not take any effect
    explicit operator bool() const noexcept { return writer_.valid(); }

    // Inserts the specified field into the document according to the
    // specified ACTION
    // Note that 'Field' type type must satisfy the Field concept
    // field attribute to be inserted
    // Return true, if field was successfully inserted
    template<Action action, typename Field>
    bool Insert(Field&& field) const {
      return writer_.insert<action>(std::forward<Field>(field));
    }

    // Inserts the specified field (denoted by the pointer) into the
    //        document according to the specified ACTION
    // Note that 'Field' type type must satisfy the Field concept
    // Note that pointer must not be nullptr
    // field attribute to be inserted
    // Return true, if field was successfully inserted
    template<Action action, typename Field>
    bool Insert(Field* field) const {
      return writer_.insert<action>(*field);
    }

    // Inserts the specified range of fields, denoted by the [begin;end)
    // into the document according to the specified ACTION
    // Note that 'Iterator' underline value type must satisfy the Field concept
    // begin the beginning of the fields range
    // end the end of the fields range
    // Return true, if the range was successfully inserted
    template<Action action, typename Iterator>
    bool Insert(Iterator begin, Iterator end) const {
      for (; writer_.valid() && begin != end; ++begin) {
        Insert<action>(*begin);
      }

      return writer_.valid();
    }

   private:
    segment_writer& writer_;
    QueryContext* query_;
  };
  static_assert(std::is_nothrow_move_constructible_v<Document>);

  class Transaction : private util::noncopyable {
   public:
    Transaction() = default;
    explicit Transaction(IndexWriter& writer) noexcept : writer_{&writer} {}

    Transaction(Transaction&& other) = default;
    Transaction& operator=(Transaction&& other) = default;

    ~Transaction() {
      // FIXME(gnusi): consider calling Abort in future
      // Commit can throw in such case -> better error handling
      Commit();
    }

    // Create a document to filled by the caller
    // for insertion into the index index
    // applied upon return value deallocation
    // `disable_flush` don't trigger segment flush
    //
    // The changes are not visible until commit()
    // Transaction should be valid
    Document Insert(bool disable_flush = false) {
      UpdateSegment(disable_flush);
      return {*active_.Segment(), segment_writer::DocContext{queries_}};
    }

    // Marks all documents matching the filter for removal.
    // TickBound - Remove filter usage is restricted by document creation tick.
    // Filter the filter selecting which documents should be removed.
    // Note that changes are not visible until commit().
    // Note that filter must be valid until commit().
    // Remove</*TickBound=*/false> is applied even for documents created after
    // the Remove call and until next TickBound Remove or Replace.
    // Transaction should be valid
    template<bool TickBound = true, typename Filter>
    void Remove(Filter&& filter) {
      UpdateSegment(/*disable_flush=*/true);
      active_.Segment()->queries_.emplace_back(std::forward<Filter>(filter),
                                               queries_, QueryContext::kDone);
      if constexpr (TickBound) {
        ++queries_;
      }
    }

    // Create a document to filled by the caller
    // for replacement of existing documents already in the index
    // matching filter with the filled document
    // applied upon return value deallocation
    // filter the filter selecting which documents should be replaced
    // Note the changes are not visible until commit()
    // Note that filter must be valid until commit()
    // Transaction should be valid
    template<typename Filter>
    Document Replace(Filter&& filter, bool disable_flush = false) {
      UpdateSegment(disable_flush);
      auto& segment = *active_.Segment();
      auto& query = segment.queries_.emplace_back(
        std::forward<Filter>(filter), queries_, QueryContext::kReplace);
      segment.has_replace_ = true;
      return {
        segment,
        segment_writer::DocContext{++queries_, segment.queries_.size() - 1},
        &query};
    }

    // Revert all pending document modifications and release resources
    // noexcept because all insertions reserve enough space for rollback
    void Reset() noexcept;

    // Register underlying segment to be flushed with the upcoming index commit
    void RegisterFlush();

    // Commit all accumulated modifications and release resources
    // return successful or not, if not call Abort
    bool Commit() noexcept {
      auto* segment = active_.Segment();
      if (segment == nullptr) {
        return true;
      }
      const auto first_tick =
        writer_->tick_.fetch_add(queries_, std::memory_order_relaxed);
      return CommitImpl(first_tick + queries_);
    }

    bool Commit(uint64_t last_tick) noexcept {
      auto* segment = active_.Segment();
      if (segment == nullptr) {
        return true;
      }
      return CommitImpl(last_tick);
    }

    // Reset all accumulated modifications and release resources
    void Abort() noexcept;

    bool FlushRequired() const noexcept {
      auto* segment = active_.Segment();
      if (segment == nullptr) {
        return false;
      }
      return writer_->FlushRequired(*segment->writer_);
    }

    bool Valid() const noexcept { return writer_ != nullptr; }

   private:
    bool CommitImpl(uint64_t last_tick) noexcept;
    // refresh segment if required (guarded by FlushContext::context_mutex_)
    // is is thread-safe to use ctx_/segment_ while holding 'flush_context_ptr'
    // since active 'flush_context' will not change and hence no reload required
    void UpdateSegment(bool disable_flush);

    IndexWriter* writer_{nullptr};
    // the segment_context used for storing changes (lazy-initialized)
    ActiveSegmentContext active_;
    // We can use active_.Segment()->queries_.size() for same purpose
    uint64_t queries_{0};
  };
  static_assert(std::is_nothrow_move_constructible_v<Transaction>);
  static_assert(std::is_nothrow_move_assignable_v<Transaction>);

  // Returns a context allowing index modification operations
  // All document insertions will be applied to the same segment on a
  // best effort basis, e.g. a flush_all() will cause a segment switch
  Transaction GetBatch() noexcept { return Transaction{*this}; }

  using ptr = std::shared_ptr<IndexWriter>;

  // Name of the lock for index repository
  static constexpr std::string_view kWriteLockName = "write.lock";

  ~IndexWriter() noexcept;

  // Returns current index snapshot
  DirectoryReader GetSnapshot() const noexcept {
    return DirectoryReader{
      std::atomic_load_explicit(&committed_reader_, std::memory_order_acquire)};
  }

  // Returns overall number of buffered documents in a writer
  uint64_t BufferedDocs() const;

  // Clears the existing index repository by staring an empty index.
  // Previously opened readers still remain valid.
  // truncate transaction tick
  // Call will rollback any opened transaction.
  void Clear(uint64_t tick = writer_limits::kMinTick);

  // Merges segments accepted by the specified defragment policy into
  // a new segment. For all accepted segments frees the space occupied
  // by the documents marked as deleted and deduplicate terms.
  // Policy the specified defragmentation policy
  // Codec desired format that will be used for segment creation,
  // nullptr == use index_writer's codec
  // Progress callback triggered for consolidation steps, if the
  // callback returns false then consolidation is aborted
  // For deferred policies during the commit stage each policy will be
  // given the exact same index_meta containing all segments in the
  // commit, however, the resulting acceptor will only be segments not
  // yet marked for consolidation by other policies in the same commit
  ConsolidationResult Consolidate(
    const ConsolidationPolicy& policy, format::ptr codec = nullptr,
    const MergeWriter::FlushProgress& progress = {});

  // Imports index from the specified index reader into new segment
  // Reader the index reader to import.
  // Desired format that will be used for segment creation,
  // nullptr == use index_writer's codec.
  // Progress callback triggered for consolidation steps, if the
  // callback returns false then consolidation is aborted.
  // Returns true on success.
  bool Import(const IndexReader& reader, format::ptr codec = nullptr,
              const MergeWriter::FlushProgress& progress = {});

  // Opens new index writer.
  // dir directory where index will be should reside
  // codec format that will be used for creating new index segments
  // mode specifies how to open a writer
  // options the configuration parameters for the writer
  static IndexWriter::ptr Make(directory& dir, format::ptr codec, OpenMode mode,
                               const IndexWriterOptions& opts = {});

  // Modify the runtime segment options as per the specified values
  // options will apply no later than after the next commit()
  void Options(const SegmentOptions& opts) noexcept { segment_limits_ = opts; }

  // Returns comparator using for sorting documents by a primary key
  // nullptr == default sort order
  const Comparer* Comparator() const noexcept { return comparator_; }

  // Begins the two-phase transaction.
  // payload arbitrary user supplied data to store in the index
  // Returns true if transaction has been successfully started.

  bool Begin(const CommitInfo& info = {}) {
    std::lock_guard lock{commit_lock_};
    return Start(info);
  }

  // Rollbacks the two-phase transaction
  void Rollback() {
    std::lock_guard lock{commit_lock_};
    Abort();
  }

  // Make all buffered changes visible for readers.
  // payload arbitrary user supplied data to store in the index
  // Return whether any changes were committed.
  //
  // Note that if begin() has been already called commit() is
  // relatively lightweight operation.
  // FIXME(gnusi): Commit() should return committed index snapshot
  bool Commit(const CommitInfo& info = {}) {
    std::lock_guard lock{commit_lock_};
    const bool modified = Start(info);
    Finish();
    return modified;
  }

  // Returns field features.
  const FeatureInfoProvider& FeatureInfo() const noexcept {
    return feature_info_;
  }

  bool FlushRequired(const segment_writer& writer) const noexcept;

  // public because we want to use std::make_shared
  IndexWriter(ConstructToken, index_lock::ptr&& lock,
              index_file_refs::ref_t&& lock_file_ref, directory& dir,
              format::ptr codec, size_t segment_pool_size,
              const SegmentOptions& segment_limits, const Comparer* comparator,
              const ColumnInfoProvider& column_info,
              const FeatureInfoProvider& feature_info,
              const PayloadProvider& meta_payload_provider,
              std::shared_ptr<const DirectoryReaderImpl>&& committed_reader,
              const ResourceManagementOptions& rm);

 private:
  struct ConsolidationContext : util::noncopyable {
    std::shared_ptr<const DirectoryReaderImpl> consolidation_reader;
    Consolidation candidates;
    MergeWriter merger;
  };

  static_assert(std::is_nothrow_move_constructible_v<ConsolidationContext>);

  struct ImportContext {
    ImportContext(
      IndexSegment&& segment, uint64_t tick, FileRefs&& refs,
      Consolidation&& consolidation_candidates,
      std::shared_ptr<const SegmentReaderImpl>&& reader,
      std::shared_ptr<const DirectoryReaderImpl>&& consolidation_reader,
      MergeWriter&& merger) noexcept
      : tick{tick},
        segment{std::move(segment)},
        refs{std::move(refs)},
        reader{std::move(reader)},
        consolidation_ctx{
          .consolidation_reader = std::move(consolidation_reader),
          .candidates = std::move(consolidation_candidates),
          .merger = std::move(merger)} {}

    ImportContext(IndexSegment&& segment, uint64_t tick, FileRefs&& refs,
                  std::shared_ptr<const SegmentReaderImpl>&& reader,
                  const ResourceManagementOptions& rm) noexcept
      : tick{tick},
        segment{std::move(segment)},
        refs{std::move(refs)},
        reader{std::move(reader)},
        consolidation_ctx{.merger{*rm.consolidations}} {}

    ImportContext(ImportContext&&) = default;

    ImportContext& operator=(const ImportContext&) = delete;
    ImportContext& operator=(ImportContext&&) = delete;

    uint64_t tick;
    IndexSegment segment;
    FileRefs refs;
    std::shared_ptr<const SegmentReaderImpl> reader;
    ConsolidationContext consolidation_ctx;
  };

  static_assert(std::is_nothrow_move_constructible_v<ImportContext>);

 public:
  struct FlushedSegment : public IndexSegment {
    FlushedSegment() = default;
    explicit FlushedSegment(IndexSegment&& segment, DocMap&& old2new,
                            DocsMask&& docs_mask, size_t docs_begin) noexcept
      : IndexSegment{std::move(segment)},
        old2new{std::move(old2new)},
        docs_mask{std::move(docs_mask)},
        document_mask{{this->docs_mask.set.get_allocator()}},
        docs_begin_{docs_begin},
        docs_end_{docs_begin_ + meta.docs_count} {}

    size_t GetDocsBegin() const noexcept { return docs_begin_; }
    size_t GetDocsEnd() const noexcept { return docs_end_; }

    bool SetCommitted(size_t committed) noexcept {
      IRS_ASSERT(GetDocsBegin() <= committed);
      IRS_ASSERT(committed < GetDocsEnd());
      docs_end_ = committed;
      return docs_begin_ != committed;
    }

    DocMap old2new;
    DocMap new2old;
    // Flushed segment removals
    DocsMask docs_mask;
    DocumentMask document_mask;
    bool was_flush = false;

   private:
    // starting doc_id that should be added to docs_mask
    // TODO(MBkkt) Better to remove, but only after parallel Commit
    size_t docs_begin_;
    size_t docs_end_;
  };

  // The segment writer and its associated ref tracking directory
  // for use with an unbounded_object_pool
  struct SegmentContext {
    using segment_meta_generator_t = std::function<SegmentMeta()>;
    using ptr = std::unique_ptr<SegmentContext>;

    // for use with index_writer::buffered_docs(), asynchronous call
    std::atomic_size_t buffered_docs_{0};
    // the codec to used for flushing a segment writer
    format::ptr codec_;
    // ref tracking for segment_writer to allow for easy ref removal on
    // segment_writer reset
    RefTrackingDirectory dir_;

    // sequential list of pending modification
    ManagedVector<QueryContext> queries_;
    // all of the previously flushed versions of this segment
    ManagedVector<FlushedSegment> flushed_;
    // update_contexts to use with 'flushed_'
    // sequentially increasing through all offsets
    // (sequential doc_id in 'flushed_' == offset + doc_limits::min(), size()
    // == sum of all 'flushed_'.'docs_count')
    ManagedVector<segment_writer::DocContext> flushed_docs_;

    // function to get new SegmentMeta from
    segment_meta_generator_t meta_generator_;

    size_t flushed_queries_{0};
    // Transaction::Commit was not called for these:
    size_t committed_queries_{0};
    size_t committed_buffered_docs_{0};
    size_t committed_flushed_docs_{0};

    uint64_t first_tick_{writer_limits::kMaxTick};
    uint64_t last_tick_{writer_limits::kMinTick};

    std::unique_ptr<segment_writer> writer_;
    // the SegmentMeta this writer was initialized with
    IndexSegment writer_meta_;
    // TODO(MBkkt) Better to be per FlushedSegment
    bool has_replace_{false};

    static std::unique_ptr<SegmentContext> make(
      directory& dir, segment_meta_generator_t&& meta_generator,
      const SegmentWriterOptions& options);

    SegmentContext(directory& dir, segment_meta_generator_t&& meta_generator,
                   const SegmentWriterOptions& options);

    void Rollback() noexcept;

    void Commit(uint64_t queries, uint64_t last_tick);

    // Flush current writer state into a materialized segment.
    // Return tick of last committed transaction.
    void Flush();

    // Ensure writer is ready to receive documents
    void Prepare();

    // Reset segment state to the initial state
    // store_flushed should store info about flushed segments?
    // Note should be true if something went wrong during segment flush
    void Reset(bool store_flushed = false) noexcept;
  };

 private:
  struct SegmentLimits {
   private:
    // TODO(MBkkt) Change zero meaning
    static constexpr auto kSizeMax = std::numeric_limits<size_t>::max();
    static constexpr auto kDocsMax = std::numeric_limits<uint32_t>::max() - 2;
    static auto ZeroMax(auto value, auto max) noexcept {
      return std::min(value - 1, max - 1) + 1;
    }

   public:
    // see segment_options::max_segment_count
    std::atomic_size_t segment_count_max;
    // see segment_options::max_segment_memory
    std::atomic_size_t segment_memory_max;
    // see segment_options::max_segment_docs
    std::atomic_uint32_t segment_docs_max;

    explicit SegmentLimits(const SegmentOptions& opts) noexcept
      : segment_count_max{ZeroMax(opts.segment_count_max, kSizeMax)},
        segment_memory_max{ZeroMax(opts.segment_memory_max, kSizeMax)},
        segment_docs_max{ZeroMax(opts.segment_docs_max, kDocsMax)} {}

    SegmentLimits& operator=(const SegmentOptions& opts) noexcept {
      segment_count_max.store(ZeroMax(opts.segment_count_max, kSizeMax));
      segment_memory_max.store(ZeroMax(opts.segment_memory_max, kSizeMax));
      segment_docs_max.store(ZeroMax(opts.segment_docs_max, kDocsMax));
      return *this;
    }
  };

  using SegmentPool = unbounded_object_pool<SegmentContext>;
  // 'value' == node offset into 'pending_segment_context_'
  using Freelist = concurrent_stack<size_t>;

  struct PendingSegmentContext : public Freelist::node_type {
    std::shared_ptr<SegmentContext> segment_;

    PendingSegmentContext(std::shared_ptr<SegmentContext> segment,
                          size_t pending_segment_context_offset)
      : Freelist::node_type{.value = pending_segment_context_offset},
        segment_{std::move(segment)} {
      IRS_ASSERT(segment_ != nullptr);
    }
  };

  using CachedReaders =
    absl::flat_hash_map<FlushedSegment*,
                        std::shared_ptr<const SegmentReaderImpl>>;

  // The context containing data collected for the next commit() call
  // Note a 'segment_context' is tracked by at most 1 'flush_context', it is
  // the job of the 'documents_context' to guarantee that the
  // 'segment_context' is not used once the tracker 'flush_context' is no
  // longer active.
  struct FlushContext {
    // ref tracking directory used by this context
    // (tracks all/only refs for this context)
    RefTrackingDirectory::ptr dir_;
    // guard for the current context during flush
    // (write) operations vs update (read)
    std::shared_mutex context_mutex_;
    // the next context to switch to
    FlushContext* next_{nullptr};

    std::vector<std::shared_ptr<SegmentContext>> segments_;
    CachedReaders cached_;

    // complete segments to be added during next commit (import)
    std::vector<ImportContext> imports_;

    void ClearPending() noexcept {
      while (pending_freelist_.pop() != nullptr) {
      }
      pending_segments_.clear();
    }

    // segment writers with data pending for next commit
    // (all segments that have been used by this flush_context)
    // must be std::deque to guarantee that element memory location does
    // not change for use with 'pending_segment_contexts_freelist_'
    std::deque<PendingSegmentContext> pending_segments_;
    // entries from 'pending_segments_' that are available for reuse
    Freelist pending_freelist_;
    WaitGroup pending_;

    // set of segments to be removed from the index upon commit
    ConsolidatingSegments segment_mask_;

    FlushContext() = default;

    ~FlushContext() noexcept { Reset(); }

    // release segment to this FlushContext
    void Emplace(ActiveSegmentContext&& active);

    // add the segment to this flush_context pending segments
    // but not to freelist. So this segment would be waited upon flushing
    void AddToPending(ActiveSegmentContext& active);

    uint64_t FlushPending(uint64_t committed_tick, uint64_t tick);

    void Reset() noexcept;
  };

  void Cleanup(FlushContext& curr, FlushContext* next = nullptr) noexcept;

  struct PendingBase {
    // Reference to flush context held until end of commit
    FlushContextPtr ctx{nullptr, nullptr};
    uint64_t tick{writer_limits::kMinTick};

    [[nodiscard]] auto StartReset(IndexWriter& writer,
                                  bool keep_next = false) noexcept {
      auto* curr = ctx.get();
      std::unique_lock lock{writer.consolidation_lock_, std::defer_lock};
      if (curr != nullptr) {
        lock.lock();
        writer.Cleanup(*curr, keep_next ? nullptr : curr->next_);
      }
      return lock;
    }
  };

  struct PendingContext : PendingBase {
    // Index meta of the next commit
    IndexMeta meta;
    // Segment readers of the next commit
    std::vector<SegmentReader> readers;
    // Files to sync
    std::vector<std::string_view> files_to_sync;

    bool Empty() const noexcept { return !ctx; }
  };

  static_assert(std::is_nothrow_move_constructible_v<PendingContext>);
  static_assert(std::is_nothrow_move_assignable_v<PendingContext>);

  struct PendingState : PendingBase {
    // meta + references of next commit
    std::shared_ptr<const DirectoryReaderImpl> commit;

    bool Valid() const noexcept { return ctx && commit; }

    void FinishReset() noexcept {
      ctx.reset();
      commit.reset();
    }

    void Reset(IndexWriter& writer) noexcept {
      std::ignore = StartReset(writer);
      FinishReset();
    }
  };

  static_assert(std::is_nothrow_move_constructible_v<PendingState>);
  static_assert(std::is_nothrow_move_assignable_v<PendingState>);

  PendingContext PrepareFlush(const CommitInfo& info);
  void ApplyFlush(PendingContext&& context);

  FlushContextPtr GetFlushContext() const noexcept;
  FlushContextPtr SwitchFlushContext() noexcept;

  // Return a usable segment or a nullptr segment if retry is required
  // (e.g. no free segments available)
  ActiveSegmentContext GetSegmentContext();

  // Return options for segment_writer
  SegmentWriterOptions GetSegmentWriterOptions(
    bool consolidation) const noexcept;

  // Return next segment identifier
  uint64_t NextSegmentId() noexcept;
  // Return current segment identifier
  uint64_t CurrentSegmentId() const noexcept;
  // Initialize new index meta
  void InitMeta(IndexMeta& meta, uint64_t tick) const;

  // Start transaction
  bool Start(const CommitInfo& info);
  // Finish transaction
  void Finish();
  // Abort transaction
  void Abort() noexcept;

  feature_set_t wand_features_;  // Set of features required for wand
  ScorersView wand_scorers_;
  FeatureInfoProvider feature_info_;
  ColumnInfoProvider column_info_;
  PayloadProvider meta_payload_provider_;  // provides payload for new segments
  const Comparer* comparator_;
  format::ptr codec_;
  // guard for cached_segment_readers_, commit_pool_, meta_
  // (modification during commit()/defragment()), payload_buf_
  std::mutex commit_lock_;
  std::recursive_mutex consolidation_lock_;
  // During consolidation, we can either perform segments merge or segments cleanup.
  // When consolidation_merge_or_cleanup_ is true, we attempt merge operation first
  // during consolidation.
  // When false, we attempt cleanup first.
  // We use this to alternate between merge and cleanup for fair execution
  bool consolidation_merge_or_cleanup_ { false };
  // segments that are under consolidation
  ConsolidatingSegments consolidating_segments_;
  // directory used for initialization of readers
  directory& dir_;
  // collection of contexts that collect data to be
  // flushed, 2 because just swap them
  std::vector<FlushContext> flush_context_pool_{2};
  // currently active context accumulating data to be
  // processed during the next flush
  std::atomic<FlushContext*> flush_context_;
  // latest/active index snapshot
  std::shared_ptr<const DirectoryReaderImpl> committed_reader_;
  // current state awaiting commit completion
  PendingState pending_state_;
  // limits for use with respect to segments
  SegmentLimits segment_limits_;
  // a cache of segments available for reuse
  SegmentPool segment_writer_pool_;
  // number of segments currently in use by the writer
  std::atomic_size_t segments_active_{0};
  std::atomic_uint64_t seg_counter_;  // segment counter
  // current modification/update tick
  std::atomic_uint64_t tick_{writer_limits::kMinTick + 1};
  uint64_t committed_tick_{writer_limits::kMinTick};
  // last committed index meta generation. Not related to ticks!
  uint64_t last_gen_;
  index_meta_writer::ptr writer_;
  index_lock::ptr write_lock_;  // exclusive write lock for directory
  index_file_refs::ref_t write_lock_file_ref_;  // file ref for lock file
  ResourceManagementOptions resource_manager_;
};

}  // namespace irs
