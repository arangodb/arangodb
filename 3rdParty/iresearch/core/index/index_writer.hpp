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

#ifndef IRESEARCH_INDEX_WRITER_H
#define IRESEARCH_INDEX_WRITER_H

#include <atomic>

#include <absl/container/flat_hash_map.h>

#include "field_meta.hpp"
#include "column_info.hpp"
#include "index_meta.hpp"
#include "merge_writer.hpp"
#include "segment_reader.hpp"
#include "segment_writer.hpp"

#include "formats/formats.hpp"
#include "search/filter.hpp"

#include "utils/async_utils.hpp"
#include "utils/bitvector.hpp"
#include "utils/thread_utils.hpp"
#include "utils/object_pool.hpp"
#include "utils/string.hpp"
#include "utils/noncopyable.hpp"

namespace iresearch {

class comparer;
class bitvector;
struct directory;
class directory_reader;

class readers_cache final : util::noncopyable {
 public:
  struct key_t {
    key_t(const segment_meta& meta); // implicit constructor

    bool operator==(const key_t& other) const noexcept {
      return name == other.name && version == other.version;
    }

    std::string name;
    uint64_t version;
  };

  struct key_hash_t {
    size_t operator()(const key_t& key) const noexcept {
      return hash_utils::hash(key.name);
    }
  };

  explicit readers_cache(directory& dir) noexcept
    : dir_(dir) {}

  void clear() noexcept;
  segment_reader emplace(const segment_meta& meta);
  size_t purge(const absl::flat_hash_set<key_t, key_hash_t>& segments) noexcept;

 private:
  std::mutex lock_;
  absl::flat_hash_map<key_t, segment_reader, key_hash_t> cache_;
  directory& dir_;
}; // readers_cache

//////////////////////////////////////////////////////////////////////////////
/// @enum OpenMode
/// @brief defines how index writer should be opened
//////////////////////////////////////////////////////////////////////////////
enum OpenMode {
  ////////////////////////////////////////////////////////////////////////////
  /// @brief Creates new index repository. In case if repository already
  ///        exists, all contents will be cleared.
  ////////////////////////////////////////////////////////////////////////////
  OM_CREATE = 1,

  ////////////////////////////////////////////////////////////////////////////
  /// @brief Opens existsing index repository. In case if repository does not
  ///        exists, error will be generated.
  ////////////////////////////////////////////////////////////////////////////
  OM_APPEND = 2,
}; // OpenMode

ENABLE_BITMASK_ENUM(OpenMode);

////////////////////////////////////////////////////////////////////////////////
/// @class index_writer 
/// @brief The object is using for indexing data. Only one writer can write to
///        the same directory simultaneously.
///        Thread safe.
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API index_writer
    : private atomic_shared_ptr_helper<
        std::pair<
          std::shared_ptr<index_meta>, std::vector<index_file_refs::ref_t>
      >>,
      private util::noncopyable {
 private:
  struct flush_context; // forward declaration
  struct segment_context; // forward declaration

  typedef std::unique_ptr<
    flush_context,
    void(*)(flush_context*) // sizeof(std::function<void(flush_context*)>) > sizeof(void(*)(flush_context*))
  > flush_context_ptr; // unique pointer required since need ponter declaration before class declaration e.g. for 'documents_context'

  typedef std::shared_ptr<segment_context> segment_context_ptr; // declaration from segment_context::ptr below

  //////////////////////////////////////////////////////////////////////////////
  /// @brief segment references given out by flush_context to allow tracking
  ///        and updating flush_context::pending_segment_context
  //////////////////////////////////////////////////////////////////////////////
  class IRESEARCH_API active_segment_context: private util::noncopyable { // non-copyable to ensure only one copy for get/put
   public:
    active_segment_context() = default;
    active_segment_context(
        segment_context_ptr ctx,
        std::atomic<size_t>& segments_active,
        flush_context* flush_ctx = nullptr, // the flush_context the segment_context is currently registered with
        size_t pending_segment_context_offset = std::numeric_limits<size_t>::max() // the segment offset in flush_ctx_->pending_segments_
    ) noexcept;
    active_segment_context(active_segment_context&&)  = default;
    ~active_segment_context();
    active_segment_context& operator=(active_segment_context&& other) noexcept;

    const segment_context_ptr& ctx() const noexcept { return ctx_; }

   private:
    IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
    friend struct flush_context; // for flush_context::emplace(...)
    segment_context_ptr ctx_{nullptr};
    flush_context* flush_ctx_{nullptr}; // nullptr will not match any flush_context
    size_t pending_segment_context_offset_; // segment offset in flush_ctx_->pending_segment_contexts_
    std::atomic<size_t>* segments_active_; // reference to index_writer::segments_active_
    IRESEARCH_API_PRIVATE_VARIABLES_END
  };

  static_assert(std::is_nothrow_move_constructible_v<active_segment_context>);

 public:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a context allowing index modification operations
  /// @note the object is non-thread-safe, each thread should use its own
  ///       separate instance
  //////////////////////////////////////////////////////////////////////////////
  class IRESEARCH_API documents_context: private util::noncopyable { // noncopyable because of segments_
   public:
    ////////////////////////////////////////////////////////////////////////////
    /// @brief a wrapper around a segment_writer::document with commit/rollback
    ////////////////////////////////////////////////////////////////////////////
    class IRESEARCH_API document : public segment_writer::document {
     public:
      document(
        flush_context_ptr&& ctx,
        const segment_context_ptr& segment,
        const segment_writer::update_context& update
      );
      document(document&& other) noexcept;
      ~document() noexcept;

     private:
      flush_context& ctx_; // reference to flush_context for rollback operations
      segment_context_ptr segment_; // hold reference to segment to prevent if from going back into the pool
      size_t update_id_;
    };

    explicit documents_context(index_writer& writer) noexcept
      : writer_(writer) {
    }

    documents_context(documents_context&& other) noexcept
      : segment_(std::move(other.segment_)),
        segment_use_count_(std::move(other.segment_use_count_)),
        tick_(other.tick_),
        writer_(other.writer_) {
      other.tick_ = 0;
      other.segment_use_count_ = 0;
    }

    ~documents_context() noexcept;

    ////////////////////////////////////////////////////////////////////////////
    /// @brief create a document to filled by the caller
    ///        for insertion into the index index
    ///        applied upon return value deallocation
    /// @note the changes are not visible until commit()
    ////////////////////////////////////////////////////////////////////////////
    document insert() {
      // thread-safe to use ctx_/segment_ while have lock since active flush_context will not change
      auto ctx = update_segment(); // updates 'segment_' and 'ctx_'
      assert(segment_.ctx());

      return document(
        std::move(ctx),
        segment_.ctx(),
        segment_.ctx()->make_update_context()
      );
    }

    ////////////////////////////////////////////////////////////////////////////
    /// @brief marks all documents matching the filter for removal
    /// @param filter the filter selecting which documents should be removed
    /// @note that changes are not visible until commit()
    /// @note that filter must be valid until commit()
    ////////////////////////////////////////////////////////////////////////////
    template<typename Filter>
    void remove(Filter&& filter) {
      // thread-safe to use ctx_/segment_ while have lock since active flush_context will not change
      auto ctx = update_segment(); // updates 'segment_' and 'ctx_'
      assert(segment_.ctx());

      segment_.ctx()->remove(std::forward<Filter>(filter)); // guarded by flush_context::flush_mutex_
    }

    ////////////////////////////////////////////////////////////////////////////
    /// @brief create a document to filled by the caller
    ///        for replacement of existing documents already in the index
    ///        matching filter with the filled document
    ///        applied upon return value deallocation
    /// @param filter the filter selecting which documents should be replaced
    /// @note the changes are not visible until commit()
    /// @note that filter must be valid until commit()
    ////////////////////////////////////////////////////////////////////////////
    template<typename Filter>
    document replace(Filter&& filter) {
      // thread-safe to use ctx_/segment_ while have lock since active flush_context will not change
      auto ctx = update_segment(); // updates 'segment_' and 'ctx_'
      assert(segment_.ctx());

      return document(
        std::move(ctx),
        segment_.ctx(),
        segment_.ctx()->make_update_context(std::forward<Filter>(filter))
      );
    }

    ////////////////////////////////////////////////////////////////////////////
    /// @brief replace existing documents already in the index matching filter
    ///        with the documents filled by the specified functor
    /// @param filter the filter selecting which documents should be replaced
    /// @param func the insertion logic, similar in signature to e.g.:
    ///        std::function<bool(segment_writer::document&)>
    /// @note the changes are not visible until commit()
    /// @note that filter must be valid until commit()
    /// @return all fields/attributes successfully insterted
    ///         if false && valid() then it is safe to retry the operation
    ///         e.g. if the segment is full and a new one must be started
    ////////////////////////////////////////////////////////////////////////////
    template<typename Filter, typename Func>
    bool replace(Filter&& filter, Func func) {
      flush_context* ctx;
      segment_context_ptr segment;

      {
        // thread-safe to use ctx_/segment_ while have lock since active flush_context will not change
        auto ctx_ptr = update_segment(); // updates 'segment_' and 'ctx_'

        assert(ctx_ptr);
        assert(segment_.ctx());
        assert(segment_.ctx()->writer_);
        ctx = ctx_ptr.get(); // make copies in case 'func' causes their reload
        segment = segment_.ctx(); // make copies in case 'func' causes their reload
        ++segment->active_count_;
      }

      auto clear_busy = make_finally([ctx, segment]()->void {
        if (!--segment->active_count_) {
          auto lock = make_lock_guard(ctx->mutex_); // lock due to context modification and notification
          ctx->pending_segment_context_cond_.notify_all(); // in case ctx is in flush_all()
        }
      });
      auto& writer = *(segment->writer_);
      segment_writer::document doc(writer);
      std::exception_ptr exception;
      bitvector rollback; // 0-based offsets to roll back on failure for this specific replace(..) operation
      auto uncomitted_doc_id_begin =
        segment->uncomitted_doc_id_begin_ > segment->flushed_update_contexts_.size()
        ? (segment->uncomitted_doc_id_begin_ - segment->flushed_update_contexts_.size()) // uncomitted start in 'writer_'
        : doc_limits::min() // uncommited start in 'flushed_'
        ;
      auto update = segment->make_update_context(std::forward<Filter>(filter));

      try {
        for(;;) {
          assert(uncomitted_doc_id_begin <= writer.docs_cached() + doc_limits::min());
          auto rollback_extra = 
            writer.docs_cached() + doc_limits::min() - uncomitted_doc_id_begin; // ensure reset() will be noexcept

          rollback.reserve(writer.docs_cached() + 1); // reserve space for rollback

          if (std::numeric_limits<doc_id_t>::max() <= writer.docs_cached() + doc_limits::min()
              || doc_limits::eof(writer.begin(update, rollback_extra))) {
            break; // the segment cannot fit any more docs, must roll back
          }

          assert(writer.docs_cached());
          rollback.set(writer.docs_cached() - 1); // 0-based
          segment->buffered_docs_.store(writer.docs_cached());

          auto done = !func(doc);

          if (writer.valid()) {
            writer.commit();

            if (done) {
              return true;
            }
          }
        }
      } catch (...) {
        exception = std::current_exception(); // track exception
      }

      // .......................................................................
      // perform rollback
      // implicitly noexcept since memory reserved in the call to begin(...)
      // .......................................................................

      writer.rollback(); // mark as failed

      for (auto i = rollback.size(); i && rollback.any();) {
        if (rollback.test(--i)) {
          rollback.unset(i); // if new doc_ids at end this allows to terminate 'for' earlier
          assert(std::numeric_limits<doc_id_t>::max() >= i + doc_limits::min());
          writer.remove(doc_id_t(i + doc_limits::min())); // convert to doc_id
        }
      }

      segment->modification_queries_[update.update_id].filter = nullptr; // mark invalid

      if (exception) {
        std::rethrow_exception(exception);
      }

      return false;
    }

    ////////////////////////////////////////////////////////////////////////////
    /// @brief revert all pending document modifications and release resources
    /// @note noexcept because all insertions reserve enough space for rollback
    ////////////////////////////////////////////////////////////////////////////
    void reset() noexcept;

    void tick(uint64_t tick) noexcept { tick_ = tick; }
    uint64_t tick() const noexcept { return tick_; }

   private:
    active_segment_context segment_; // the segment_context used for storing changes (lazy-initialized)
    uint64_t segment_use_count_{0}; // segment_.ctx().use_count() at constructor/destructor time must equal
    uint64_t tick_{0}; // transaction tick
    index_writer& writer_;

    // refresh segment if required (guarded by flush_context::flush_mutex_)
    // is is thread-safe to use ctx_/segment_ while holding 'flush_context_ptr'
    // since active 'flush_context' will not change and hence no reload required
    flush_context_ptr update_segment();
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief additional information required for removal/update requests
  //////////////////////////////////////////////////////////////////////////////
  struct modification_context {
    typedef std::shared_ptr<const irs::filter> filter_ptr;

    modification_context(const irs::filter& match_filter, size_t gen, bool isUpdate)
      : filter(filter_ptr(), &match_filter), generation(gen), update(isUpdate), seen(false) {}
    modification_context(const filter_ptr& match_filter, size_t gen, bool isUpdate)
      : filter(match_filter), generation(gen), update(isUpdate), seen(false) {}
    modification_context(irs::filter::ptr&& match_filter, size_t gen, bool isUpdate)
      : filter(std::move(match_filter)), generation(gen), update(isUpdate), seen(false) {}
    modification_context(modification_context&&) = default;
    modification_context& operator=(const modification_context&) = delete;
    modification_context& operator=(modification_context&&) = delete;

    filter_ptr filter; // keep a handle to the filter for the case when this object has ownership
    const size_t generation;
    const bool update; // this is an update modification (as opposed to remove)
    bool seen;
  };

  static_assert(std::is_nothrow_move_constructible_v<modification_context>);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief options the the writer should use for segments
  //////////////////////////////////////////////////////////////////////////////
  struct segment_options {
    ////////////////////////////////////////////////////////////////////////////
    /// @brief segment aquisition requests will block and wait for free segments
    ///        after this many segments have been aquired e.g. via documents()
    ///        0 == unlimited
    ////////////////////////////////////////////////////////////////////////////
    size_t segment_count_max{0};

    ////////////////////////////////////////////////////////////////////////////
    /// @brief flush the segment to the repository after its total document
    ///        count (live + masked) grows beyond this byte limit, in-flight
    ///        documents will still be written to the segment before flush
    ///        0 == unlimited
    ////////////////////////////////////////////////////////////////////////////
    size_t segment_docs_max{0};

    ////////////////////////////////////////////////////////////////////////////
    /// @brief flush the segment to the repository after its in-memory size
    ///        grows beyond this byte limit, in-flight documents will still be
    ///        written to the segment before flush
    ///        0 == unlimited
    ////////////////////////////////////////////////////////////////////////////
    size_t segment_memory_max{0};
  };

  ////////////////////////////////////////////////////////////////////////////
  /// @brief functor for creating payload. Operation tick is provided for 
  /// payload generation.
  ////////////////////////////////////////////////////////////////////////////
  using payload_provider_t = std::function<bool(uint64_t, bstring&)>;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief options the the writer should use after creation
  //////////////////////////////////////////////////////////////////////////////
  struct init_options : public segment_options {
    ////////////////////////////////////////////////////////////////////////////
    /// @brief returns column info the writer should use for columnstore
    ////////////////////////////////////////////////////////////////////////////
    column_info_provider_t column_info;

    ////////////////////////////////////////////////////////////////////////////
    /// @brief Provides payload for index_meta created by writer
    ////////////////////////////////////////////////////////////////////////////
    payload_provider_t meta_payload_provider;

    ////////////////////////////////////////////////////////////////////////////
    /// @brief comparator defines physical order of documents in each segment
    ///        produced by an index_writer.
    ///        empty == use default system sorting order
    ////////////////////////////////////////////////////////////////////////////
    const comparer* comparator{nullptr};

    ////////////////////////////////////////////////////////////////////////////
    /// @brief number of memory blocks to cache by the internal memory pool
    ///        0 == use default from memory_allocator::global()
    ////////////////////////////////////////////////////////////////////////////
    size_t memory_pool_size{0};

    ////////////////////////////////////////////////////////////////////////////
    /// @brief number of free segments cached in the segment pool for reuse
    ///        0 == do not cache any segments, i.e. always create new segments
    ////////////////////////////////////////////////////////////////////////////
    size_t segment_pool_size{128}; // arbitrary size

    ////////////////////////////////////////////////////////////////////////////
    /// @brief aquire an exclusive lock on the repository to guard against index
    ///        corruption from multiple index_writers
    ////////////////////////////////////////////////////////////////////////////
    bool lock_repository{true};

    init_options() {} // GCC5 requires non-default definition
  };

  struct segment_hash {
    size_t operator()(const segment_meta* segment) const noexcept {
      return hash_utils::hash(segment->name);
    }
  }; // segment_hash

  struct segment_equal {
    size_t operator()(const segment_meta* lhs,
                      const segment_meta* rhs) const noexcept {
      return lhs->name == rhs->name;
    }
  }; // segment_equal

  // segments that are under consolidation
  using consolidating_segments_t = absl::flat_hash_set<
    const segment_meta*,
    segment_hash,
    segment_equal>;

  //////////////////////////////////////////////////////////////////////////////
  /// @enum ConsolidationError
  //////////////////////////////////////////////////////////////////////////////
  enum class ConsolidationError : uint32_t {
    ////////////////////////////////////////////////////////////////////////////
    /// @brief consolidation failed
    ////////////////////////////////////////////////////////////////////////////
    FAIL = 0,

    ////////////////////////////////////////////////////////////////////////////
    /// @brief consolidation succesfully finished
    ////////////////////////////////////////////////////////////////////////////
    OK,

    ////////////////////////////////////////////////////////////////////////////
    /// @brief consolidation was scheduled for the upcoming commit
    ////////////////////////////////////////////////////////////////////////////
    PENDING
  }; // ConsolidationError

  //////////////////////////////////////////////////////////////////////////////
  /// @brief represents result of a consolidation
  //////////////////////////////////////////////////////////////////////////////
  struct consolidation_result {
    ////////////////////////////////////////////////////////////////////////////
    /// @brief number of candidates
    ////////////////////////////////////////////////////////////////////////////
    size_t size;

    ////////////////////////////////////////////////////////////////////////////
    /// @brief error code
    ////////////////////////////////////////////////////////////////////////////
    ConsolidationError error;

    // intentionally implicit
    operator bool() const noexcept {
      return error != ConsolidationError::FAIL;
    }
  }; // consolidation_result

  using ptr = std::shared_ptr<index_writer>;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a set of candidates denoting an instance of consolidation
  //////////////////////////////////////////////////////////////////////////////
  using consolidation_t = std::vector<const segment_meta*>;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief mark consolidation candidate segments matching the current policy
  /// @param candidates the segments that should be consolidated
  ///        in: segment candidates that may be considered by this policy
  ///        out: actual segments selected by the current policy
  /// @param dir the segment directory
  /// @param meta the index meta containing segments to be considered
  /// @param consolidating_segments segments that are currently in progress
  ///        of consolidation
  /// @note final candidates are all segments selected by at least some policy
  //////////////////////////////////////////////////////////////////////////////
  using consolidation_policy_t = std::function<void(
    consolidation_t& candidates,
    const index_meta& meta,
    const consolidating_segments_t& consolidating_segments)>;

  ////////////////////////////////////////////////////////////////////////////
  /// @brief name of the lock for index repository 
  ////////////////////////////////////////////////////////////////////////////
  static const std::string WRITE_LOCK_NAME;

  ////////////////////////////////////////////////////////////////////////////
  /// @brief destructor 
  ////////////////////////////////////////////////////////////////////////////
  ~index_writer() noexcept;

  ////////////////////////////////////////////////////////////////////////////
  /// @returns overall number of buffered documents in a writer 
  ////////////////////////////////////////////////////////////////////////////
  uint64_t buffered_docs() const;

  ////////////////////////////////////////////////////////////////////////////
  /// @brief Clears the existing index repository by staring an empty index.
  ///        Previously opened readers still remain valid.
  /// @param truncate transaction tick
  /// @note call will rollback any opened transaction
  ////////////////////////////////////////////////////////////////////////////
  void clear(uint64_t tick = 0);

  ////////////////////////////////////////////////////////////////////////////
  /// @brief merges segments accepted by the specified defragment policty into
  ///        a new segment. For all accepted segments frees the space occupied
  ///        by the doucments marked as deleted and deduplicate terms.
  /// @param policy the speicified defragmentation policy
  /// @param codec desired format that will be used for segment creation,
  ///        nullptr == use index_writer's codec
  /// @param progress callback triggered for consolidation steps, if the
  ///                 callback returns false then consolidation is aborted
  /// @note for deffered policies during the commit stage each policy will be
  ///       given the exact same index_meta containing all segments in the
  ///       commit, however, the resulting acceptor will only be segments not
  ///       yet marked for consolidation by other policies in the same commit
  ////////////////////////////////////////////////////////////////////////////
  consolidation_result consolidate(
    const consolidation_policy_t& policy,
    format::ptr codec = nullptr,
    const merge_writer::flush_progress_t& progress = {});

  //////////////////////////////////////////////////////////////////////////////
  /// @return returns a context allowing index modification operations
  /// @note all document insertions will be applied to the same segment on a
  ///       best effort basis, e.g. a flush_all() will cause a segment switch
  //////////////////////////////////////////////////////////////////////////////
  documents_context documents() noexcept {
    return documents_context(*this);
  }

  ////////////////////////////////////////////////////////////////////////////
  /// @brief imports index from the specified index reader into new segment
  /// @param reader the index reader to import 
  /// @param desired format that will be used for segment creation,
  ///        nullptr == use index_writer's codec
  /// @param progress callback triggered for consolidation steps, if the
  ///        callback returns false then consolidation is aborted
  /// @returns true on success
  ////////////////////////////////////////////////////////////////////////////
  bool import(
    const index_reader& reader,
    format::ptr codec = nullptr,
    const merge_writer::flush_progress_t& progress = {}
  );

  ////////////////////////////////////////////////////////////////////////////
  /// @brief opens new index writer
  /// @param dir directory where index will be should reside
  /// @param codec format that will be used for creating new index segments
  /// @param mode specifies how to open a writer
  /// @param options the configuration parameters for the writer
  ////////////////////////////////////////////////////////////////////////////
  static index_writer::ptr make(
    directory& dir,
    format::ptr codec,
    OpenMode mode,
    const init_options& opts = init_options()
  );

  ////////////////////////////////////////////////////////////////////////////
  /// @brief modify the runtime segment options as per the specified values
  ///        options will apply no later than after the next commit()
  ////////////////////////////////////////////////////////////////////////////
  void options(const segment_options& opts) noexcept {
    segment_limits_ = opts;
  }

  ////////////////////////////////////////////////////////////////////////////
  /// @returns comparator using for sorting documents by a primary key
  ///          nullptr == default sort order
  ////////////////////////////////////////////////////////////////////////////
  const comparer* comparator() const noexcept {
    return comparator_;
  }

  ////////////////////////////////////////////////////////////////////////////
  /// @brief begins the two-phase transaction
  /// @param payload arbitrary user supplied data to store in the index
  /// @returns true if transaction has been sucessflully started
  ////////////////////////////////////////////////////////////////////////////
  bool begin() {
    auto lock = make_lock_guard(commit_lock_);

    return start();
  }

  ////////////////////////////////////////////////////////////////////////////
  /// @brief rollbacks the two-phase transaction 
  ////////////////////////////////////////////////////////////////////////////
  void rollback() {
    auto lock = make_lock_guard(commit_lock_);

    abort();
  }

  ////////////////////////////////////////////////////////////////////////////
  /// @brief make all buffered changes visible for readers
  /// @param payload arbitrary user supplied data to store in the index
  /// @return whether any changes were committed
  ///
  /// @note that if begin() has been already called commit() is
  /// relatively lightweight operation 
  ////////////////////////////////////////////////////////////////////////////
  bool commit() {
    auto lock = make_lock_guard(commit_lock_);

    const bool modified = start();
    finish();
    return modified;
  }

  ////////////////////////////////////////////////////////////////////////////
  /// @brief clears index writer's reader cache
  ////////////////////////////////////////////////////////////////////////////
  void purge_cached_readers() noexcept {
    cached_readers_.clear();
  }

 private:
  typedef std::vector<index_file_refs::ref_t> file_refs_t;

  struct consolidation_context_t : util::noncopyable {
    consolidation_context_t() = default;

    consolidation_context_t(consolidation_context_t&&) = default;
    consolidation_context_t& operator=(consolidation_context_t&&) = delete;

    consolidation_context_t(
        std::shared_ptr<index_meta>&& consolidaton_meta,
        consolidation_t&& candidates,
        merge_writer&& merger) noexcept
      : consolidaton_meta(std::move(consolidaton_meta)),
        candidates(std::move(candidates)),
        merger(std::move(merger)) {
    }

    consolidation_context_t(
        std::shared_ptr<index_meta>&& consolidaton_meta,
        consolidation_t&& candidates) noexcept
      : consolidaton_meta(std::move(consolidaton_meta)),
        candidates(std::move(candidates)) {
    }

    std::shared_ptr<index_meta> consolidaton_meta;
    consolidation_t candidates;
    merge_writer merger;
  }; // consolidation_context_t

  static_assert(std::is_nothrow_move_constructible_v<consolidation_context_t>);

  struct import_context {
    import_context(
        index_meta::index_segment_t&& segment,
        size_t generation,
        file_refs_t&& refs,
        consolidation_t&& consolidation_candidates,
        std::shared_ptr<index_meta>&& consolidation_meta,
        merge_writer&& merger) noexcept
      : generation(generation),
        segment(std::move(segment)),
        refs(std::move(refs)),
        consolidation_ctx(std::move(consolidation_meta),
                          std::move(consolidation_candidates),
                          std::move(merger)) {
    }

    import_context(
        index_meta::index_segment_t&& segment,
        size_t generation,
        file_refs_t&& refs,
        consolidation_t&& consolidation_candidates,
        std::shared_ptr<index_meta>&& consolidation_meta) noexcept
      : generation(generation),
        segment(std::move(segment)),
        refs(std::move(refs)),
        consolidation_ctx(std::move(consolidation_meta),
                          std::move(consolidation_candidates)) {
    }

    import_context(
        index_meta::index_segment_t&& segment,
        size_t generation,
        file_refs_t&& refs,
        consolidation_t&& consolidation_candidates) noexcept
      : generation(generation),
        segment(std::move(segment)),
        refs(std::move(refs)),
        consolidation_ctx(nullptr, std::move(consolidation_candidates)) {
    }

    import_context(
        index_meta::index_segment_t&& segment,
        size_t generation,
        file_refs_t&& refs) noexcept
      : generation(generation),
        segment(std::move(segment)),
        refs(std::move(refs)) {
    }

    import_context(
        index_meta::index_segment_t&& segment,
        size_t generation) noexcept
      : generation(generation),
        segment(std::move(segment)) {
    }

    import_context(import_context&&) = default;

    import_context& operator=(const import_context&) = delete;
    import_context& operator=(import_context&&) = delete;

    const size_t generation;
    index_meta::index_segment_t segment;
    file_refs_t refs;
    consolidation_context_t consolidation_ctx;
  }; // import_context

  static_assert(std::is_nothrow_move_constructible_v<import_context>);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the segment writer and its associated ref tracing directory
  ///        for use with an unbounded_object_pool
  /// @note the segment flows through following stages
  ///        1a) taken from pool (!busy_, !dirty) {Thread A}
  ///        2a) requested by documents() (!busy_, !dirty)
  ///        3a) documents() validates that active context is the same && !dirty_
  ///        4a) documents() sets 'busy_', guarded by flush_context::flush_mutex_
  ///        5a) documents() starts operation
  ///        6a) documents() finishes operation
  ///        7a) documents() unsets 'busy_', guarded by flush_context::mutex_ (different mutex for cond notify)
  ///        8a) documents() notifies flush_context::pending_segment_context_cond_
  ///        ... after some time ...
  ///       10a) documents() validates that active context is the same && !dirty_
  ///       11a) documents() sets 'busy_', guarded by flush_context::flush_mutex_
  ///       12a) documents() starts operation
  ///       13b) flush_all() switches active context {Thread B}
  ///       14b) flush_all() sets 'dirty_', guarded by flush_context::mutex_
  ///       15b) flush_all() checks 'busy_' and waits on flush_context::mutex_ (different mutex for cond notify)
  ///       16a) documents() finishes operation {Thread A}
  ///       17a) documents() unsets 'busy_', guarded by flush_context::mutex_ (different mutex for cond notify)
  ///       18a) documents() notifies flush_context::pending_segment_context_cond_
  ///       19b) flush_all() checks 'busy_' and continues flush {Thread B} (different mutex for cond notify)
  ///       {scenario 1} ... after some time reuse of same documents() {Thread A}
  ///       20a) documents() validates that active context is not the same
  ///       21a) documents() re-requests a new segment, i.e. continues to (1a)
  ///       {scenario 2} ... after some time reuse of same documents() {Thread A}
  ///       20a) documents() validates that active context is the same && dirty_
  ///       21a) documents() re-requests a new segment, i.e. continues to (1a)
  /// @note segment_writer::doc_contexts[...uncomitted_document_contexts_): generation == flush_context::generation
  /// @note segment_writer::doc_contexts[uncomitted_document_contexts_...]: generation == local generation (updated when segment_context registered once again with flush_context)
  //////////////////////////////////////////////////////////////////////////////
  struct IRESEARCH_API segment_context { // IRESEARCH_API because of make_update_context(...)/remove(...) used by documents_context::replace(...)/documents_context::remove(...)
    struct flushed_t: public index_meta::index_segment_t {
      doc_id_t docs_mask_tail_doc_id{std::numeric_limits<doc_id_t>::max()}; // starting doc_id that should be added to docs_mask
      flushed_t() = default;
      flushed_t(segment_meta&& meta)
        : index_meta::index_segment_t(std::move(meta)) {}
    };
    using segment_meta_generator_t = std::function<segment_meta()>;
    using ptr = std::shared_ptr<segment_context>;

    std::atomic<size_t> active_count_; // number of active in-progress operations (insert/replace) (e.g. document instances or replace(...))
    std::atomic<size_t> buffered_docs_; // for use with index_writer::buffered_docs() asynchronous call
    format::ptr codec_; // the codec to used for flushing a segment writer
    bool dirty_; // true if flush_all() started processing this segment (this segment should not be used for any new operations), guarded by the flush_context::flush_mutex_
    ref_tracking_directory dir_; // ref tracking for segment_writer to allow for easy ref removal on segment_writer reset
    std::recursive_mutex flush_mutex_; // guard 'flushed_', 'uncomitted_*' and 'writer_' from concurrent flush
    std::vector<flushed_t> flushed_; // all of the previously flushed versions of this segment, guarded by the flush_context::flush_mutex_
    std::vector<segment_writer::update_context> flushed_update_contexts_; // update_contexts to use with 'flushed_' sequentially increasing through all offsets (sequential doc_id in 'flushed_' == offset + type_limits<type_t::doc_id_t>::min(), size() == sum of all 'flushed_'.'docs_count')
    segment_meta_generator_t meta_generator_; // function to get new segment_meta from
    std::vector<modification_context> modification_queries_; // sequential list of pending modification requests (remove/update)
    size_t uncomitted_doc_id_begin_; // starting doc_id that is not part of the current flush_context (doc_id sequentially increasing through all 'flushed_' offsets and into 'segment_writer::doc_contexts' hence value may be greater than doc_id_t::max)
    size_t uncomitted_generation_offset_; // current modification/update generation offset for asignment to uncommited operations (same as modification_queries_.size() - uncomitted_modification_queries_) FIXME TODO consider removing
    size_t uncomitted_modification_queries_; // staring offset in 'modification_queries_' that is not part of the current flush_context
    segment_writer::ptr writer_;
    index_meta::index_segment_t writer_meta_; // the segment_meta this writer was initialized with

    DECLARE_FACTORY(directory& dir, segment_meta_generator_t&& meta_generator, const column_info_provider_t& column_info, const comparer* comparator);
    segment_context(directory& dir, segment_meta_generator_t&& meta_generator, const column_info_provider_t& column_info, const comparer* comparator);

    ////////////////////////////////////////////////////////////////////////////
    /// @brief flush current writer state into a materialized segment
    /// @return tick of last committed transaction
    ////////////////////////////////////////////////////////////////////////////
    uint64_t flush();

    // returns context for "insert" operation
    segment_writer::update_context make_update_context();

    // returns context for "update" operation
    segment_writer::update_context make_update_context(const filter& filter);
    segment_writer::update_context make_update_context(const std::shared_ptr<filter>& filter);
    segment_writer::update_context make_update_context(filter::ptr&& filter);

    ////////////////////////////////////////////////////////////////////////////
    /// @brief ensure writer is ready to recieve documents
    ////////////////////////////////////////////////////////////////////////////
    void prepare();

    // modifies context for "remove" operation
    void remove(const filter& filter);
    void remove(const std::shared_ptr<filter>& filter);
    void remove(filter::ptr&& filter);

    ////////////////////////////////////////////////////////////////////////////
    /// @brief reset segment state to the initial state
    ////////////////////////////////////////////////////////////////////////////
    void reset() noexcept;
  };

  struct segment_limits {
    std::atomic<size_t> segment_count_max; // @see segment_options::max_segment_count
    std::atomic<size_t> segment_docs_max; // @see segment_options::max_segment_docs
    std::atomic<size_t> segment_memory_max; // @see segment_options::max_segment_memory
    segment_limits(const segment_options& opts) noexcept
      : segment_count_max(opts.segment_count_max),
        segment_docs_max(opts.segment_docs_max),
        segment_memory_max(opts.segment_memory_max) {
    }
    segment_limits& operator=(const segment_options& opts) noexcept {
      segment_count_max.store(opts.segment_count_max);
      segment_docs_max.store(opts.segment_docs_max);
      segment_memory_max.store(opts.segment_memory_max);
      return *this;
    }
  };

  typedef std::shared_ptr<
    std::pair<std::shared_ptr<index_meta>,
    file_refs_t
  >> committed_state_t;
  typedef atomic_shared_ptr_helper<
    committed_state_t::element_type
  > committed_state_helper;

  typedef unbounded_object_pool<segment_context> segment_pool_t;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the context containing data collected for the next commit() call
  /// @note a 'segment_context' is tracked by at most 1 'flush_context', it is
  ///       the job of the 'documents_context' to garantee that the
  ///       'segment_context' is not used once the tracker 'flush_context' is no
  ///       longer active
  //////////////////////////////////////////////////////////////////////////////
  struct flush_context {
    typedef concurrent_stack<size_t> freelist_t; // 'value' == node offset into 'pending_segment_context_'
    struct pending_segment_context: public freelist_t::node_type {
      const size_t doc_id_begin_; // starting segment_context::document_contexts_ for this flush_context range [pending_segment_context::doc_id_begin_, std::min(pending_segment_context::doc_id_end_, segment_context::uncomitted_doc_ids_))
      size_t doc_id_end_; // ending segment_context::document_contexts_ for this flush_context range [pending_segment_context::doc_id_begin_, std::min(pending_segment_context::doc_id_end_, segment_context::uncomitted_doc_ids_))
      const size_t modification_offset_begin_; // starting segment_context::modification_queries_ for this flush_context range [pending_segment_context::modification_offset_begin_, std::min(pending_segment_context::::modification_offset_end_, segment_context::uncomitted_modification_queries_))
      size_t modification_offset_end_; // ending segment_context::modification_queries_ for this flush_context range [pending_segment_context::modification_offset_begin_, std::min(pending_segment_context::::modification_offset_end_, segment_context::uncomitted_modification_queries_))
      const segment_context::ptr segment_;

      pending_segment_context(
        const segment_context::ptr& segment,
        size_t pending_segment_context_offset
      ): doc_id_begin_(segment->uncomitted_doc_id_begin_),
         doc_id_end_(std::numeric_limits<size_t>::max()),
         modification_offset_begin_(segment->uncomitted_modification_queries_),
         modification_offset_end_(std::numeric_limits<size_t>::max()),
         segment_(segment) {
        assert(segment);
        value = pending_segment_context_offset;
      }
    };

    std::atomic<size_t> generation_{ 0 }; // current modification/update generation
    ref_tracking_directory::ptr dir_; // ref tracking directory used by this context (tracks all/only refs for this context)
    async_utils::read_write_mutex flush_mutex_; // guard for the current context during flush (write) operations vs update (read)
    std::mutex mutex_; // guard for the current context during struct update operations, e.g. pending_segments_, pending_segment_contexts_
    flush_context* next_context_; // the next context to switch to
    std::vector<import_context> pending_segments_; // complete segments to be added during next commit (import)
    std::condition_variable pending_segment_context_cond_; // notified when a segment has been freed (guarded by mutex_)
    std::deque<pending_segment_context> pending_segment_contexts_; // segment writers with data pending for next commit (all segments that have been used by this flush_context) must be std::deque to garantee that element memory location does not change for use with 'pending_segment_contexts_freelist_'
    freelist_t pending_segment_contexts_freelist_; // entries from 'pending_segment_contexts_' that are available for reuse
    absl::flat_hash_set<readers_cache::key_t, readers_cache::key_hash_t> segment_mask_; // set of segment names to be removed from the index upon commit

    flush_context() = default;

    ~flush_context() noexcept {
      reset();
    }

    void emplace(active_segment_context&& segment); // add the segment to this flush_context
    void reset() noexcept;
  }; // flush_context

  struct sync_context : util::noncopyable {
    sync_context() = default;
    sync_context(sync_context&& rhs) noexcept
      : files(std::move(rhs.files)),
        segments(std::move(rhs.segments)) {
    }
    sync_context& operator=(sync_context&& rhs) noexcept {
      if (this != &rhs) {
        files = std::move(rhs.files);
        segments = std::move(rhs.segments);
      }
      return *this;
    }

    bool empty() const noexcept {
      return segments.empty();
    }

    void register_full_sync(size_t i) {
      segments.emplace_back(i, 0);
    }

    void register_partial_sync(size_t i, const std::string& file) {
      segments.emplace_back(i, 1);
      files.emplace_back(file);
    }

    template<typename Visitor>
    bool visit(const Visitor& visitor, const index_meta& meta) const {
      auto begin = files.begin();

      for (auto& entry : segments) {
        auto& segment = meta[entry.first];

        if (entry.second) {
          // partial update
          assert(begin <= files.end());

          if (std::numeric_limits<size_t>::max() == entry.second) {
            // skip invalid segments
            begin += entry.second;
            continue;
          }

          for (auto end = begin + entry.second; begin != end; ++begin) {
            if (!visitor(begin->get())) {
              return false;
            }
          }
        } else {
          // full sync
          for (auto& file : segment.meta.files) {
            if (!visitor(file)) {
              return false;
            }
          }
        }

        if (!visitor(segment.filename)) {
          return false;
        }
      }

      return true;
    }

    std::vector<std::reference_wrapper<const std::string>> files; // files to sync
    std::vector<std::pair<size_t, size_t>> segments; // segments to sync (index within index meta + number of files to sync)
  }; // sync_context

  struct pending_context_t {
    flush_context_ptr ctx{ nullptr, nullptr }; // reference to flush context held until end of commit
    index_meta::ptr meta; // index meta of next commit
    sync_context to_sync; // file names and segments to be synced during next commit

    operator bool() const noexcept { return ctx && meta; }
  }; // pending_context_t

  static_assert(std::is_nothrow_move_constructible_v<pending_context_t>);
  static_assert(std::is_nothrow_move_assignable_v<pending_context_t>);

  struct pending_state_t {
    flush_context_ptr ctx{ nullptr, nullptr }; // reference to flush context held until end of commit
    committed_state_t commit; // meta + references of next commit

    operator bool() const noexcept { return ctx && commit; }

    void reset() noexcept {
      ctx.reset();
      commit.reset();
    }
  }; // pending_state_t

  static_assert(std::is_nothrow_move_constructible_v<pending_state_t>);
  static_assert(std::is_nothrow_move_assignable_v<pending_state_t>);

  index_writer(
    index_lock::ptr&& lock, 
    index_file_refs::ref_t&& lock_file_ref,
    directory& dir, 
    format::ptr codec,
    size_t segment_pool_size,
    const segment_options& segment_limits,
    const comparer* comparator,
    const column_info_provider_t& column_info,
    const payload_provider_t& meta_payload_provider,
    index_meta&& meta,
    committed_state_t&& committed_state
  );

  pending_context_t flush_all();

  flush_context_ptr get_flush_context(bool shared = true);
  active_segment_context get_segment_context(flush_context& ctx); // return a usable segment or a nullptr segment if retry is required (e.g. no free segments available)

  bool start(); // starts transaction
  void finish(); // finishes transaction
  void abort(); // aborts transaction

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  column_info_provider_t column_info_;
  payload_provider_t meta_payload_provider_; // provides payload for new segments
  const comparer* comparator_;
  readers_cache cached_readers_; // readers by segment name
  format::ptr codec_;
  std::mutex commit_lock_; // guard for cached_segment_readers_, commit_pool_, meta_ (modification during commit()/defragment()), paylaod_buf_
  committed_state_t committed_state_; // last successfully committed state
  std::recursive_mutex consolidation_lock_;
  consolidating_segments_t consolidating_segments_; // segments that are under consolidation
  directory& dir_; // directory used for initialization of readers
  std::vector<flush_context> flush_context_pool_; // collection of contexts that collect data to be flushed, 2 because just swap them
  std::atomic<flush_context*> flush_context_; // currently active context accumulating data to be processed during the next flush
  index_meta meta_; // latest/active state of index metadata
  pending_state_t pending_state_; // current state awaiting commit completion
  segment_limits segment_limits_; // limits for use with respect to segments
  segment_pool_t segment_writer_pool_; // a cache of segments available for reuse
  std::atomic<size_t> segments_active_; // number of segments currently in use by the writer
  index_meta_writer::ptr writer_;
  index_lock::ptr write_lock_; // exclusive write lock for directory
  index_file_refs::ref_t write_lock_file_ref_; // track ref for lock file to preven removal
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // index_writer

}

#endif // IRESEARCH_INDEX_WRITER_H
