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

#include "shared.hpp"
#include "file_names.hpp"
#include "merge_writer.hpp"
#include "formats/format_utils.hpp"
#include "search/exclusion.hpp"
#include "utils/bitset.hpp"
#include "utils/bitvector.hpp"
#include "utils/directory_utils.hpp"
#include "utils/index_utils.hpp"
#include "utils/timer_utils.hpp"
#include "utils/type_limits.hpp"
#include "index_writer.hpp"

#include <list>

NS_LOCAL

const size_t NON_UPDATE_RECORD = irs::integer_traits<size_t>::const_max; // non-update

std::vector<irs::index_file_refs::ref_t> extract_refs(
    const irs::ref_tracking_directory& dir
) {
  std::vector<irs::index_file_refs::ref_t> refs;
  // FIXME reserve

  auto visitor = [&refs](const irs::index_file_refs::ref_t& ref) {
    refs.emplace_back(ref);
    return true;
  };
  dir.visit_refs(visitor);

  return refs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply any document removals based on filters in the segment
/// @param modifications_begin where to get document update_contexts from
/// @param modifications_end where to get document update_contexts from (end)
/// @param docs_mask where to apply document removals to
/// @param readers readers by segment name
/// @param meta key used to get reader for the segment to evaluate
/// @param min_modification_generation smallest consider modification generation
/// @return if any new records were added (modification_queries_ modified)
////////////////////////////////////////////////////////////////////////////////
template<typename ModificationItr>
bool add_document_mask_modified_records(
    ModificationItr modifications_begin, // where to get document update_contexts from (start)
    ModificationItr modifications_end, // where to get document update_contexts from (end)
    irs::document_mask& docs_mask, // where to apply document removals to
    irs::readers_cache& readers, // where to get segment readers from
    irs::segment_meta& meta, // key used to get reader for the segment to evaluate
    size_t min_modification_generation = 0
) {
  if (modifications_begin >= modifications_end) {
    return false; // nothing new to flush
  }

  auto reader = readers.emplace(meta);

  if (!reader) {
    throw irs::index_error(); // failed to open segment
  }

  bool modified = false;

  for (auto itr = modifications_begin; itr != modifications_end; ++itr) {
    auto& modification = *itr;

    if (!modification.filter) {
      continue; // skip invalid or uncommitted modification queries
    }

    auto prepared = modification.filter->prepare(reader);

    for (auto itr = prepared->execute(reader); itr->next();) {
      auto doc_id = itr->value();

      // if the indexed doc_id was insert()ed after the request for modification
      // or the indexed doc_id was already masked then it should be skipped
      if (modification.generation < min_modification_generation
          || !docs_mask.insert(doc_id).second) {
        continue; // the current modification query does not match any records
      }

      assert(meta.live_docs_count);
      --meta.live_docs_count; // decrement count of live docs
      modification.seen = true;
      modified = true;
    }
  }

  return modified;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply any document removals based on filters in the segment
/// @param modifications_begin where to get document update_contexts from
/// @param modifications_end where to get document update_contexts from (end)
/// @param segment where to apply document removals to
/// @param min_doc_id staring doc_id that should be considered
/// @param readers readers by segment name
/// @return if any new records were added (modification_queries_ modified)
////////////////////////////////////////////////////////////////////////////////
template<typename ModificationItr>
bool add_document_mask_modified_records(
    ModificationItr modifications_begin, // where to get document update_contexts from (start)
    ModificationItr modifications_end, // where to get document update_contexts from (end)
    irs::index_writer::segment_context& segment, // where to apply document removals to
    irs::doc_id_t min_doc_id, // staring doc_id in 'segment_writer::doc_contexts' that should be considered in the range [min_doc_id, uncomitted_document_contexts_)
    irs::readers_cache& readers // where to get segment readers from
) {
  if (modifications_begin >= modifications_end || !segment.writer_) {
    return false; // nothing new to flush
  }

  auto reader = readers.emplace(segment.flushed_meta_);

  if (!reader) {
    throw irs::index_error(); // failed to open segment
  }

  bool modified = false;
  assert(segment.writer_);
  auto& writer = *(segment.writer_);

  assert(segment.uncomitted_doc_ids_ <= writer.docs_cached());

  for (auto itr = modifications_begin; itr != modifications_end; ++itr) {
    auto& modification = *itr;

    if (!modification.filter) {
      continue; // skip invalid or uncommitted modification queries
    }

    auto prepared = modification.filter->prepare(reader);

    for (auto itr = prepared->execute(reader); itr->next();) {
      auto seg_doc_id =
        itr->value() - (irs::type_limits<irs::type_t::doc_id_t>::min)(); // 0-based

      if (seg_doc_id < min_doc_id
          || seg_doc_id >= segment.uncomitted_doc_ids_) {
        continue; // doc_id is not part of the current flush_context
      }

      auto& doc_ctx = writer.doc_context(seg_doc_id);

      // if the indexed doc_id was insert()ed after the request for modification
      // or the indexed doc_id was already masked then it should be skipped
      if (modification.generation < doc_ctx.generation
          || !writer.remove(seg_doc_id)) {
        continue; // the current modification query does not match any records
      }

      // if not an update modification (i.e. a remove modification) or
      // if non-update-value record or update-value record whose query was seen
      // i.e. the document is a removal candidate, it exists in the modification 'generation'
      // for every update request a replacement 'update-value' is optimistically inserted
      if (!modification.update
          || doc_ctx.update_id == NON_UPDATE_RECORD
          || segment.modification_queries_[doc_ctx.update_id].seen) {
        assert(segment.flushed_meta_.live_docs_count);
        --segment.flushed_meta_.live_docs_count; // decrement count of live docs
        modification.seen = true;
        modified = true;
      }
    }
  }

  return modified;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief mask documents created by updates which did not have any matches
/// @return if any new records were added (modification_queries_ modified)
////////////////////////////////////////////////////////////////////////////////
bool add_document_mask_unused_updates(
    irs::index_writer::segment_context& segment, // where to apply document removals to
    irs::doc_id_t min_doc_id // staring doc_id in 'segment_writer::doc_contexts' that should be considered in the range [min_doc_id, uncomitted_document_contexts_)
) {
  if (segment.modification_queries_.empty()) {
    return false; // nothing new to add
  }

  bool modified = false;
  assert(segment.writer_);
  auto& writer = *(segment.writer_);

  assert(min_doc_id <= segment.uncomitted_doc_ids_);
  assert(segment.uncomitted_doc_ids_ <= writer.docs_cached());

  for (auto doc_id = min_doc_id, count = segment.uncomitted_doc_ids_;
       doc_id < count;
       ++doc_id) {
    auto& doc_ctx = writer.doc_context(doc_id);

    if (doc_ctx.update_id == NON_UPDATE_RECORD) {
      continue; // not an update operation
    }

    assert(segment.modification_queries_.size() > doc_ctx.update_id);

    // if it's an update record placeholder who's query did not match any records
    if (!segment.modification_queries_[doc_ctx.update_id].seen
        && writer.remove(doc_id)) {
      assert(segment.flushed_meta_.live_docs_count);
      --segment.flushed_meta_.live_docs_count; // decrement count of live docs
      modified  = true;
    }
  }

  return modified;
}

// append file refs for files from the specified segments description
template<typename T, typename M>
void append_segments_refs(
    T& buf,
    irs::directory& dir,
    const M& meta
) {
  auto visitor = [&buf](const irs::index_file_refs::ref_t& ref)->bool {
    buf.emplace_back(ref);
    return true;
  };

  // track all files referenced in index_meta
  irs::directory_utils::reference(dir, meta, visitor, true);
}

const std::string& write_document_mask(
    iresearch::directory& dir,
    iresearch::segment_meta& meta,
    const iresearch::document_mask& docs_mask,
    bool increment_version = true
) {
  assert(docs_mask.size() <= std::numeric_limits<uint32_t>::max());

  auto mask_writer = meta.codec->get_document_mask_writer();
  if (increment_version) {
    meta.files.erase(mask_writer->filename(meta)); // current filename
    ++meta.version; // segment modified due to new document_mask
  }
  const auto& file = *meta.files.emplace(mask_writer->filename(meta)).first; // new/expected filename
  mask_writer->write(dir, meta, docs_mask);
  return file;
}

// mapping: name -> { new segment, old segment }
typedef std::map<
  irs::string_ref,
  std::pair<
    const irs::segment_meta*, // new segment
    std::pair<const irs::segment_meta*, size_t> // old segment + index within merge_writer
>> candidates_mapping_t;

/// @param candidates_mapping output mapping
/// @param candidates candidates for mapping
/// @param segments map against a specified segments
/// @returns first - has removals, second - number of mapped candidates
std::pair<bool, size_t> map_candidates(
    candidates_mapping_t& candidates_mapping,
    const std::set<const irs::segment_meta*> candidates,
    const irs::index_meta::index_segments_t& segments
) {
  size_t i = 0;
  for (auto* candidate : candidates) {
    candidates_mapping.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(candidate->name),
      std::forward_as_tuple(nullptr, std::make_pair(candidate, i++))
    );
  }

  size_t found = 0;
  bool has_removals = false;
  const auto candidate_not_found = candidates_mapping.end();

  for (const auto& segment : segments) {
    const auto& meta = segment.meta;
    const auto it = candidates_mapping.find(meta.name);

    if (candidate_not_found == it) {
      // not a candidate
      continue;
    }

    auto* new_segment = it->second.first;

    if (new_segment && new_segment->version >= meta.version) {
      // mapping already has a newer segment version
      continue;
    }

    ++found;

    assert(it->second.second.first);
    it->second.first = &meta;

    has_removals |= (meta.version != it->second.second.first->version);
  }

  return std::make_pair(has_removals, found);
}

void map_removals(
    const candidates_mapping_t& candidates_mapping,
    const irs::merge_writer& merger,
    irs::readers_cache& readers,
    irs::document_mask& docs_mask
) {
  assert(merger);

  for (auto& mapping : candidates_mapping) {
    const auto& segment_mapping = mapping.second;

    if (segment_mapping.first->version != segment_mapping.second.first->version) {
      auto& merge_ctx = merger[segment_mapping.second.second];
      auto reader = readers.emplace(*segment_mapping.first);
      irs::exclusion deleted_docs(
        merge_ctx.reader->docs_iterator(),
        reader->docs_iterator()
      );

      while (deleted_docs.next()) {
        docs_mask.insert(merge_ctx.doc_map(deleted_docs.value()));
      }
    }
  }
}

NS_END // NS_LOCAL

NS_ROOT

segment_reader readers_cache::emplace(const segment_meta& meta) {
  REGISTER_TIMER_DETAILED();

  segment_reader cached_reader;

  // FIXME consider moving open/reopen out of the scope of the lock
  SCOPED_LOCK(lock_);
  auto& reader = cache_[meta.name];

  cached_reader = std::move(reader); // clear existing reader

  // update cache, in case of failure reader stays empty
  reader = cached_reader
    ? cached_reader.reopen(meta)
    : segment_reader::open(dir_, meta);

  return reader;
}

void readers_cache::clear() NOEXCEPT {
  SCOPED_LOCK(lock_);
  cache_.clear();
}

size_t readers_cache::purge(const std::unordered_set<string_ref>& segments) NOEXCEPT {
  if (segments.empty()) {
    return 0;
  }

  size_t erased = 0;

  SCOPED_LOCK(lock_);

  for (auto it = cache_.begin(); it != cache_.end(); ) {
    if (segments.end() != segments.find(it->first)) {
      it = cache_.erase(it);
      ++erased;
    } else {
      ++it;
    }
  }

  return erased;
}

// ----------------------------------------------------------------------------
// --SECTION--                                      index_writer implementation 
// ----------------------------------------------------------------------------

const std::string index_writer::WRITE_LOCK_NAME = "write.lock";

index_writer::documents_context::document::document(
    const flush_context_ptr& ctx,
    const segment_context::ptr& segment,
    const segment_writer::update_context& update
): segment_writer::document(*(segment->writer_)),
   ctx_(*ctx),
   segment_(segment),
   update_id_(update.update_id) {
  assert(ctx);
  assert(segment_);
  assert(segment_->writer_);
  assert(segment->uncomitted_doc_ids_ <= segment_->writer_->docs_cached());
  segment_->busy_ = true; // guarded by flush_context::flush_mutex_
  segment_->writer_->begin(
    update,
    segment_->writer_->docs_cached() - segment->uncomitted_doc_ids_ // ensure reset() will be noexcept
  );
  segment_->buffered_docs.store(segment_->writer_->docs_cached());
}

index_writer::documents_context::document::document(document&& other)
  : segment_writer::document(*(other.segment_->writer_)),
    ctx_(other.ctx_), // GCC does not allow moving of references
    segment_(std::move(other.segment_)),
    update_id_(std::move(other.update_id_)) {
}

index_writer::documents_context::document::~document() {
  if (!segment_) {
    return; // another instance will call commit()
  }

  try {
    segment_->writer_->commit();
  } catch (...) {
    segment_->writer_->rollback();
  }

  if (!*this && update_id_ != NON_UPDATE_RECORD) {
    segment_->modification_queries_[update_id_].filter = nullptr; // mark invalid
  }

  SCOPED_LOCK(ctx_.mutex_); // lock due to context modification and notification
  segment_->busy_ = false; // guarded by flush_context::mutex_ @see flush_all()
  ctx_.pending_segment_context_cond_.notify_all(); // in case ctx is in flush_all()
}

index_writer::documents_context::~documents_context() {
  auto ctx = writer_.get_flush_context();

  for (auto& segment: segments_) {
    ctx->emplace(std::move(segment)); // commit segment
  }
}

void index_writer::documents_context::reset() NOEXCEPT {
  for (auto& segment: segments_) {
    auto& ctx = segment.ctx();

    if (!ctx || !ctx->writer_) {
      continue; // nothing to reset
    }

    auto& writer = *(ctx->writer_);

    assert(integer_traits<doc_id_t>::const_max >= writer.docs_cached());

    // rollback document insertions
    for (auto i = ctx->uncomitted_doc_ids_,
         count = doc_id_t(writer.docs_cached());
         i < count;
         ++i) {
      writer.remove(i); // expects 0-based doc_id
    }

    // rollback modification queries
    for (auto i = ctx->uncomitted_modification_queries_,
         count = ctx->modification_queries_.size();
         i < count;
         ++i) {
      ctx->modification_queries_[i].filter = nullptr; // mark invalid
    }
  }
}

index_writer::flush_context_ptr index_writer::documents_context::update_segment() {
  auto ctx = writer_.get_flush_context();

  // ...........................................................................
  // refresh segment if required (guarded by flush_context::flush_mutex_)
  // ...........................................................................

  if (segments_.empty()) {
    segments_.emplace_back(writer_.get_segment_context(*ctx));

    return ctx;
  }

  auto& segment = segments_.back();
  // FIXME TODO when flushing segment, flush to repository, track meta to be added to to imported segments, then reuse same writer
  if (!segment.ctx() // no segment (lazy initialized)
      || segment.ctx()->dirty_
      || (writer_.segment_limits_.segment_docs_max
          && writer_.segment_limits_.segment_docs_max <= segment.ctx()->writer_->docs_cached()) // too many docs
      || (writer_.segment_limits_.segment_memory_max
          && writer_.segment_limits_.segment_memory_max <= segment.ctx()->writer_->memory()) // too much memory
      || type_limits<type_t::doc_id_t>::eof(segment.ctx()->writer_->docs_cached())) { // segment full
    segments_.emplace_back(writer_.get_segment_context(*ctx));
  }

  return ctx;
}

void index_writer::flush_context::emplace(flush_segment_context_ref&& segment) {
  if (!segment.ctx_) {
    return; // nothing to do
  }

  auto& ctx = *(segment.ctx_);
  size_t generation_base;
  size_t modification_count;

  {
    SCOPED_LOCK(mutex_); // pending_segment_contexts_ may be asynchronously read

    // update pending_segment_context
    // this segment_context has not yet been seen by this flush_context
    // or was marked dirty imples flush_context switching making a full-circle
    if (!segment.flush_ctx_) { // FIXME TODO use below once col_writer tail is fixed to flush() multiple times without overwrite
    //if (this != segment.flush_ctx_ || ctx.dirty_) {
      pending_segment_contexts_.emplace_back(pending_segment_context{
        ctx.uncomitted_doc_ids_,
        ctx.uncomitted_modification_queries_,
        segment.ctx_
      });

      // mark segment as non-reusable if it was peviously registered with a different flush_context
      if (segment.flush_ctx_) {
        ctx.dirty_ = true;
      }
    }

    assert(ctx.uncomitted_modification_queries_ <= ctx.modification_queries_.size());
    modification_count =
      ctx.modification_queries_.size() - ctx.uncomitted_modification_queries_ + 1; // +1 for insertions before removals
    generation_base = generation_ += modification_count; // atomic increment to end of unique generation range
    generation_base -= modification_count; // start of generation range
  }

  // ...........................................................................
  // noexcept state update operations below here
  // ...........................................................................

  // ...........................................................................
  // update generation of segment operation
  // ...........................................................................

  assert(ctx.writer_);
  assert(integer_traits<doc_id_t>::const_max >= ctx.writer_->docs_cached());
  auto& writer = *(ctx.writer_);

  // update generations of segment_writer::doc_contexts
  for (auto i = ctx.uncomitted_doc_ids_, count = doc_id_t(writer.docs_cached());
       i < count;
       ++i) {
    assert(writer.doc_context(i).generation < modification_count);
    writer.doc_context(i).generation += generation_base; // update to flush_context generation
  }

  // update generations of modification_queries_
  for (auto i = ctx.uncomitted_modification_queries_,
       count = ctx.modification_queries_.size();
       i < count;
       ++i) {
    assert(ctx.modification_queries_[i].generation < modification_count);
    const_cast<size_t&>(ctx.modification_queries_[i].generation) += generation_base; // update to flush_context generation
  }

  // ...........................................................................
  // reset counters for segment reuse
  // ...........................................................................

  ctx.uncomitted_generation_offset_ = 0;
  ctx.uncomitted_doc_ids_ = writer.docs_cached();
  ctx.uncomitted_modification_queries_ = ctx.modification_queries_.size();
}

void index_writer::flush_context::reset() NOEXCEPT {
  // reset before returning to pool
  for (auto& entry: pending_segment_contexts_) {
    entry.segment_->reset();
  }

  generation_.store(0);
  dir_->clear_refs();
  pending_segments_.clear();
  pending_segment_contexts_.clear();
  segment_mask_.clear();
}

index_writer::segment_context::segment_context(directory& dir)
  : buffered_docs(0),
    busy_(false),
    dirty_(false),
    dir_(dir),
    uncomitted_doc_ids_(0),
    uncomitted_generation_offset_(0),
    uncomitted_modification_queries_(0),
    writer_(segment_writer::make(dir_)) {
}

index_writer::segment_context::ptr index_writer::segment_context::make(
    directory& dir
) {
  return memory::make_shared<segment_context>(dir);
}

segment_writer::update_context index_writer::segment_context::make_update_context() {
  return segment_writer::update_context {
    uncomitted_generation_offset_, // current modification generation
    NON_UPDATE_RECORD
  };
}

segment_writer::update_context index_writer::segment_context::make_update_context(
    const filter& filter
) {
  auto generation = ++uncomitted_generation_offset_; // increment generation due to removal
  auto update_id = modification_queries_.size();

  modification_queries_.emplace_back(filter, generation - 1, true); // -1 for previous generation

  return segment_writer::update_context {
    generation, // current modification generation
    update_id // entry in modification_queries_
  };
}

segment_writer::update_context index_writer::segment_context::make_update_context(
    const std::shared_ptr<filter>& filter
) {
  assert(filter);
  auto generation = ++uncomitted_generation_offset_; // increment generation due to removal
  auto update_id = modification_queries_.size();

  modification_queries_.emplace_back(filter, generation - 1, true); // -1 for previous generation

  return segment_writer::update_context {
    generation, // current modification generation
    update_id // entry in modification_queries_
  };
}

segment_writer::update_context index_writer::segment_context::make_update_context(
    filter::ptr&& filter
) {
  assert(filter);
  auto generation = ++uncomitted_generation_offset_; // increment generation due to removal
  auto update_id = modification_queries_.size();

  modification_queries_.emplace_back(std::move(filter), generation - 1, true); // -1 for previous generation

  return segment_writer::update_context {
    generation, // current modification generation
    update_id // entry in modification_queries_
  };
}

void index_writer::segment_context::remove(const filter& filter) {
  modification_queries_.emplace_back(
    filter, uncomitted_generation_offset_++, false
  );
}

void index_writer::segment_context::remove(
    const std::shared_ptr<filter>& filter
) {
  if (!filter) {
    return; // skip empty filters
  }

  modification_queries_.emplace_back(
    filter, uncomitted_generation_offset_++, false
  );
}

void index_writer::segment_context::remove(filter::ptr&& filter) {
  if (!filter) {
    return; // skip empty filters
  }

  modification_queries_.emplace_back(
    std::move(filter), uncomitted_generation_offset_++, false
  );
}

void index_writer::segment_context::reset() {
  busy_ = false;
  dirty_ = false;
  buffered_docs.store(0);
  dir_.clear_refs();
  flushed_meta_ = segment_meta();
  modification_queries_.clear();
  uncomitted_doc_ids_ = 0;
  uncomitted_generation_offset_ = 0;
  uncomitted_modification_queries_ = 0;
  writer_->reset();
}

index_writer::index_writer(
    index_lock::ptr&& lock,
    index_file_refs::ref_t&& lock_file_ref,
    directory& dir,
    format::ptr codec,
    size_t segment_pool_size,
    const segment_limits& segment_limits,
    index_meta&& meta,
    committed_state_t&& committed_state
) NOEXCEPT:
    cached_readers_(dir),
    codec_(codec),
    committed_state_(std::move(committed_state)),
    dir_(dir),
    flush_context_pool_(2), // 2 because just swap them due to common commit lock
    meta_(std::move(meta)),
    segment_limits_(segment_limits),
    segment_writer_pool_(segment_pool_size),
    segments_active_(0),
    writer_(codec->get_index_meta_writer()),
    write_lock_(std::move(lock)),
    write_lock_file_ref_(std::move(lock_file_ref)) {
  assert(codec);
  flush_context_.store(&flush_context_pool_[0]);

  // setup round-robin chain
  for (size_t i = 0, count = flush_context_pool_.size() - 1; i < count; ++i) {
    flush_context_pool_[i].dir_ = memory::make_unique<ref_tracking_directory>(dir);
    flush_context_pool_[i].next_context_ = &flush_context_pool_[i + 1];
  }

  // setup round-robin chain
  flush_context_pool_[flush_context_pool_.size() - 1].dir_ = memory::make_unique<ref_tracking_directory>(dir);
  flush_context_pool_[flush_context_pool_.size() - 1].next_context_ = &flush_context_pool_[0];
}

void index_writer::clear() {
  SCOPED_LOCK(commit_lock_);

  if (!pending_state_
      && meta_.empty()
      && type_limits<type_t::index_gen_t>::valid(meta_.last_gen_)) {
    return; // already empty
  }

  auto ctx = get_flush_context(false);
  SCOPED_LOCK(ctx->mutex_); // ensure there are no active struct update operations

  auto pending_commit = memory::make_shared<committed_state_t::element_type>(
    std::piecewise_construct,
    std::forward_as_tuple(memory::make_shared<index_meta>()),
    std::forward_as_tuple()
  );

  auto& dir = *ctx->dir_;
  auto& pending_meta = *pending_commit->first;
  auto& pending_refs = pending_commit->second;

  // setup new meta
  pending_meta.update_generation(meta_); // clone index metadata generation
  pending_meta.seg_counter_.store(meta_.counter()); // ensure counter() >= max(seg#)

  // write 1st phase of index_meta transaction
  if (!writer_->prepare(dir, pending_meta)) {
    throw illegal_state();
  }

  pending_refs.emplace_back(
    directory_utils::reference(dir, writer_->filename(pending_meta), true)
  );

  // 1st phase of the transaction successfully finished here
  meta_.update_generation(pending_meta); // ensure new generation reflected in 'meta_'
  pending_state_.ctx = std::move(ctx); // retain flush context reference
  pending_state_.commit = std::move(pending_commit);

  finish();

  // ...........................................................................
  // all functions below are noexcept
  // ...........................................................................

  meta_.segments_.clear(); // noexcept op (clear after finish(), to match reset of pending_state_ inside finish(), allows recovery on clear() failure)
  cached_readers_.clear(); // original readers no longer required

  // clear consolidating segments
  SCOPED_LOCK(consolidation_lock_);
  consolidating_segments_.clear();
}

index_writer::ptr index_writer::make(
    directory& dir,
    format::ptr codec,
    OpenMode mode,
    const options& opts /*= options()*/
) {
  std::vector<index_file_refs::ref_t> file_refs;
  index_lock::ptr lock;
  index_file_refs::ref_t lockfile_ref;

  if (opts.lock_repository) {
    // lock the directory
    lock = dir.make_lock(WRITE_LOCK_NAME);
    lockfile_ref = directory_utils::reference(dir, WRITE_LOCK_NAME, true); // will be created by try_lock

    if (!lock || !lock->try_lock()) {
      throw lock_obtain_failed(WRITE_LOCK_NAME);
    }
  }

  // read from directory or create index metadata
  index_meta meta;
  {
    auto reader = codec->get_index_meta_reader();
    std::string segments_file;
    const bool index_exists = reader->last_segments_file(dir, segments_file);

    if (OM_CREATE == mode
        || ((OM_CREATE | OM_APPEND) == mode && !index_exists)) {
      // Try to read. It allows us to
      // create writer against an index that's
      // currently opened for searching

      try {
        // for OM_CREATE meta must be fully recreated, meta read only to get last version
        if (index_exists) {
          reader->read(dir, meta, segments_file);
          meta.clear();
          meta.last_gen_ = type_limits<type_t::index_gen_t>::invalid(); // this meta is for a totaly new index
        }
      } catch (const error_base&) {
        meta = index_meta();
      }
    } else if (!index_exists) {
      throw file_not_found(); // no segments file found
    } else {
      reader->read(dir, meta, segments_file);
      append_segments_refs(file_refs, dir, meta);
      file_refs.emplace_back(iresearch::directory_utils::reference(dir, segments_file));
    }
  }

  auto comitted_state = memory::make_shared<committed_state_t::element_type>(
    memory::make_shared<index_meta>(meta),
    std::move(file_refs)
  );

  PTR_NAMED(
    index_writer,
    writer,
    std::move(lock), 
    std::move(lockfile_ref),
    dir,
    codec,
    opts.segment_pool_size,
    segment_limits(opts),
    std::move(meta),
    std::move(comitted_state)
  );

  directory_utils::ensure_allocator(dir, opts.memory_pool_size); // ensure memory_allocator set in directory
  directory_utils::remove_all_unreferenced(dir); // remove non-index files from directory

  return writer;
}

index_writer::~index_writer() {
  close();
  flush_context_ = nullptr;
  flush_context_pool_.clear(); // ensue all tracked segment_contexts are released before segment_writer_pool_ is deallocated
}

void index_writer::close() {
  cached_readers_.clear(); // cached_readers_ read/modified during flush()
  write_lock_.reset();
}

uint64_t index_writer::buffered_docs() const { 
  uint64_t docs_in_ram = 0;
  auto ctx = const_cast<index_writer*>(this)->get_flush_context();
  SCOPED_LOCK(ctx->mutex_); // 'pending_used_segment_contexts_'/'pending_free_segment_contexts_' may be modified

  for (auto& entry: ctx->pending_segment_contexts_) {
    docs_in_ram += entry.segment_->buffered_docs.load(); // reading segment_writer::docs_count() is not thread safe
  }

  return docs_in_ram;
}

bool index_writer::consolidate(
    const consolidation_policy_t& policy, format::ptr codec /*= nullptr*/
) {
  REGISTER_TIMER_DETAILED();

  if (!codec) {
    // use default codec if not specified
    codec = codec_;
  }

  std::set<const segment_meta*> candidates;

  // hold reference to the last committed state
  // to prevent files to be deleted by a cleaner
  // during upcoming consolidation
  const auto committed_state = committed_state_;
  assert(committed_state);
  auto committed_meta = committed_state->first;
  assert(committed_meta);

  // collect a list of consolidation candidates
  {
    SCOPED_LOCK(consolidation_lock_);
    policy(candidates, dir_, *committed_meta, consolidating_segments_);

    switch (candidates.size()) {
      case 0:
        // nothing to consolidate
        return true;
      case 1: {
        const auto* segment = *candidates.begin();

        if (!segment) {
          // invalid candidate
          return false;
        }

        if (segment->live_docs_count == segment->docs_count) {
          // no deletes, nothing to consolidate
          return true;
        }
      }
    }

    // check that candidates are not involved in ongoing merges
    for (const auto* candidate : candidates) {
      if (consolidating_segments_.end() != consolidating_segments_.find(candidate)) {
        // segment has been already chosen for consolidation, give up
        return false;
      }
    }

    // register for consolidation
    consolidating_segments_.insert(candidates.begin(), candidates.end());
  }

  // unregisterer for all registered candidates
  auto unregister_segments = irs::make_finally([&candidates, this]() NOEXCEPT {
    if (candidates.empty()) {
      return;
    }

    SCOPED_LOCK(consolidation_lock_);
    for (const auto* candidate : candidates) {
      consolidating_segments_.erase(candidate);
    }
  });

  // validate candidates
  {
    size_t found = 0;

    const auto candidate_not_found = candidates.end();

    for (const auto& segment : *committed_meta) {
      found += size_t(candidate_not_found != candidates.find(&segment.meta));
    }

    if (found != candidates.size()) {
      // not all candidates are valid
      IR_FRMT_WARN(
        "Failed to start consolidation for index generation '" IR_SIZE_T_SPECIFIER "', found only '" IR_SIZE_T_SPECIFIER "' out of '" IR_SIZE_T_SPECIFIER "' candidates",
        committed_meta->generation(),
        found,
        candidates.size()
      );
      return false;
    }
  }

  // do lock-free merge

  index_meta::index_segment_t consolidation_segment;
  consolidation_segment.meta.codec = codec_; // should use new codec
  consolidation_segment.meta.version = 0; // reset version for new segment
  consolidation_segment.meta.name = file_name(meta_.increment()); // increment active meta, not fn arg

  ref_tracking_directory dir(dir_); // track references for new segment
  merge_writer merger(dir, consolidation_segment.meta.name);
  merger.reserve(candidates.size());

  // add consolidated segments to the merge_writer
  for (const auto* segment : candidates) {
    // already checked validity
    assert(segment);

    auto reader = cached_readers_.emplace(*segment);

    if (reader) {
      // merge_writer holds a reference to reader
      merger.add(static_cast<irs::sub_reader::ptr>(reader));
    }
  }

  // we do not persist segment meta since some removals may come later
  if (!merger.flush(consolidation_segment.filename, consolidation_segment.meta, false)) {
    return false; // nothing to consolidate or consolidation failure
  }

  // commit merge
  {
    SCOPED_LOCK_NAMED(commit_lock_, lock); // ensure committed_state_ segments are not modified by concurrent consolidate()/commit()
    const auto current_committed_meta = committed_state_->first;
    assert(current_committed_meta);

    if (pending_state_) {
      // transaction has been started, we're somewhere in the middle
      auto ctx = get_flush_context(); // can modify ctx->segment_mask_ without lock since have commit_lock_

      // register consolidation for the next transaction
      ctx->pending_segments_.emplace_back(
        std::move(consolidation_segment),
        integer_traits<size_t>::max(), // skip deletes, will accumulate deletes from existing candidates
        extract_refs(dir), // do not forget to track refs
        std::move(candidates), // consolidation context candidates
        std::move(committed_meta), // consolidation context meta
        std::move(merger) // merge context
      );

    } else if (committed_meta == current_committed_meta) {
      // before new transaction was started:
      // no commits happened in since consolidation was started

      auto ctx = get_flush_context();
      SCOPED_LOCK(ctx->mutex_); // lock due to context modification

      lock.unlock(); // can release commit lock, we guarded against commit by locked flush context

      // persist segment meta
      consolidation_segment.filename = index_utils::write_segment_meta(
        dir, consolidation_segment.meta
      );

      ctx->segment_mask_.reserve(
        ctx->segment_mask_.size() + candidates.size()
      );

      ctx->pending_segments_.emplace_back(
        std::move(consolidation_segment),
        0, // deletes must be applied to the consolidated segment
        extract_refs(dir), // do not forget to track refs
        std::move(candidates), // consolidation context candidates
        std::move(committed_meta) // consolidation context meta
      );

      // filter out merged segments for the next commit
      const auto& consolidation_ctx = ctx->pending_segments_.back().consolidation_ctx;

      for (const auto* segment : consolidation_ctx.candidates) {
        ctx->segment_mask_.emplace(segment->name);
      }
    } else {
      // before new transaction was started:
      // there was a commit(s) since consolidation was started,

      auto ctx = get_flush_context();
      SCOPED_LOCK(ctx->mutex_); // lock due to context modification

      lock.unlock(); // can release commit lock, we guarded against commit by locked flush context

      candidates_mapping_t mappings;
      const auto res = map_candidates(mappings, candidates, current_committed_meta->segments());

      if (res.second != candidates.size()) {
        // at least one candidate is missing
        // can't finish consolidation
        IR_FRMT_WARN(
          "Failed to finish merge for segment '%s', found only '" IR_SIZE_T_SPECIFIER "' out of '" IR_SIZE_T_SPECIFIER "' candidates",
          consolidation_segment.meta.name.c_str(),
          res.second,
          candidates.size()
        );

        return false;
      }

      // handle deletes if something changed
      if (res.first) {
        irs::document_mask docs_mask;

        map_removals(mappings, merger, cached_readers_, docs_mask);

        if (!docs_mask.empty()) {
          consolidation_segment.meta.live_docs_count -= docs_mask.size();
          write_document_mask(dir, consolidation_segment.meta, docs_mask, false);
        }
      }

      // persist segment meta
      consolidation_segment.filename = index_utils::write_segment_meta(
        dir, consolidation_segment.meta
      );

      ctx->segment_mask_.reserve(
        ctx->segment_mask_.size() + candidates.size()
      );

      ctx->pending_segments_.emplace_back(
        std::move(consolidation_segment),
        0, // deletes must be applied to the consolidated segment
        extract_refs(dir), // do not forget to track refs
        std::move(candidates), // consolidation context candidates
        std::move(committed_meta) // consolidation context meta
      );

      // filter out merged segments for the next commit
      const auto& consolidation_ctx = ctx->pending_segments_.back().consolidation_ctx;

      for (const auto* segment : consolidation_ctx.candidates) {
        ctx->segment_mask_.emplace(segment->name);
      }
    }
  }

  return true;
}

bool index_writer::import(const index_reader& reader, format::ptr codec /*= nullptr*/) {
  if (!reader.live_docs_count()) {
    return true; // skip empty readers since no documents to import
  }

  if (!codec) {
    codec = codec_;
  }

  ref_tracking_directory dir(dir_); // track references

  index_meta::index_segment_t segment;
  segment.meta.name = file_name(meta_.increment());
  segment.meta.codec = codec;

  merge_writer merger(dir, segment.meta.name);
  merger.reserve(reader.size());

  for (auto& segment : reader) {
    merger.add(segment);
  }

  if (!merger.flush(segment.filename, segment.meta)) {
    return false; // import failure (no files created, nothing to clean up)
  }

  auto refs = extract_refs(dir);

  auto ctx = get_flush_context();
  SCOPED_LOCK(ctx->mutex_); // lock due to context modification

  ctx->pending_segments_.emplace_back(
    std::move(segment),
    ctx->generation_.load(), // current modification generation
    std::move(refs) // do not forget to track refs
  );

  return true;
}

index_writer::flush_context_ptr index_writer::get_flush_context(bool shared /*= true*/) {
  auto* ctx = flush_context_.load(); // get current ctx

  if (!shared) {
    for(;;) {
      async_utils::read_write_mutex::write_mutex mutex(ctx->flush_mutex_);
      SCOPED_LOCK_NAMED(mutex, lock); // lock ctx exchange (write-lock)

      // aquire the current flush_context and its lock
      if (!flush_context_.compare_exchange_strong(ctx, ctx->next_context_)) {
        ctx = flush_context_.load(); // it might have changed
        continue;
      }

      lock.release();

      return {
        ctx,
        [](flush_context* ctx) NOEXCEPT ->void {
          async_utils::read_write_mutex::write_mutex mutex(ctx->flush_mutex_);
          ADOPT_SCOPED_LOCK_NAMED(mutex, lock);

          ctx->reset(); // reset context and make ready for reuse
        }
      };
    }
  }

  for(;;) {
    async_utils::read_write_mutex::read_mutex mutex(ctx->flush_mutex_);
    TRY_SCOPED_LOCK_NAMED(mutex, lock); // lock current ctx (read-lock)

    if (!lock) {
      std::this_thread::yield(); // allow flushing thread to finish exchange
      ctx = flush_context_.load(); // it might have changed
      continue;
    }

    // at this point flush_context_ might have already changed
    // get active ctx, since initial_ctx is locked it will never be swapped with current until unlocked
    auto* flush_ctx = flush_context_.load();

    // primary_flush_context_ has changed
    if (ctx != flush_ctx) {
      ctx = flush_ctx;
      continue;
    }

    lock.release();

    return {
      ctx,
      [](flush_context* ctx) NOEXCEPT ->void {
        async_utils::read_write_mutex::read_mutex mutex(ctx->flush_mutex_);
        ADOPT_SCOPED_LOCK_NAMED(mutex, lock);
      }
    };
  }
}

index_writer::flush_segment_context_ref index_writer::get_segment_context(
    flush_context& ctx
) {
  SCOPED_LOCK_NAMED(ctx.mutex_, lock); // 'pending_segment_contexts_' may be asynchronously read

  for (;;) {
    // find a reusable slot (linear search on the assumption that there are not a lot of segments)
    // 'pending_segment_contexts_' contains only segments for the specific 'ctx' (not reused between contexts)
    // FIXME TODO use irs::memory::freelist
    for (auto& pending_segment_context: ctx.pending_segment_contexts_) {
      auto& segment = pending_segment_context.segment_;

      // reusable segment_contexts are not referenced by anyone and are not dirty
      if (segment.use_count() == 1 && !segment->dirty_) { // +1 for the reference in 'pending_segment_contexts_'
        return flush_segment_context_ref(segment, segments_active_, &ctx);
      }
    }

    if (!segment_limits_.segment_count_max
        || segments_active_.load() < segment_limits_.segment_count_max) {
      break; // should allocate a new segment_context from the pool
    }

    // retry aquiring 'segment_context' until it is aquired
    ctx.pending_segment_context_cond_.wait_for(
      lock, std::chrono::milliseconds(1000) // arbitrary sleep interval
    );
  }

  auto segment_ctx = segment_writer_pool_.emplace(dir_).release();

  segment_ctx->busy_ = false; // reset to default
  segment_ctx->dirty_ = false; // reset to default
  segment_ctx->writer_->reset(
    segment_meta(file_name(meta_.increment()), codec_)
  );

  return flush_segment_context_ref(segment_ctx, segments_active_);
}

index_writer::pending_context_t index_writer::flush_all() {
  REGISTER_TIMER_DETAILED();
  bool modified = !type_limits<type_t::index_gen_t>::valid(meta_.last_gen_);
  sync_context to_sync;
  document_mask docs_mask;

  auto pending_meta = memory::make_unique<index_meta>();
  auto& segments = pending_meta->segments_;

  auto ctx = get_flush_context(false);
  auto& dir = *(ctx->dir_);
  SCOPED_LOCK_NAMED(ctx->mutex_, lock); // ensure there are no active struct update operations

  //////////////////////////////////////////////////////////////////////////////
  /// Stage 0
  /// wait for any outstanding segments to settle to ensure that any rollbacks
  /// are properly tracked in 'modification_queries_'
  //////////////////////////////////////////////////////////////////////////////

  for (auto& entry: ctx->pending_segment_contexts_) {
    // mark the 'segment_context' as dirty so that it will not be reused if this
    // 'flush_context' once again becomes the active context while the
    // 'segment_context' handle is still held by documents()
    entry.segment_->dirty_ = true;

    // retry aquiring 'segment_context' until it is aquired
    // once !'busy_' it will not change since this 'flush_context' is not the
    // active context and hence will not give out this 'segment_context'
    while (entry.segment_->busy_
           || entry.segment_.use_count() != 1) { // FIXME TODO remove this condition once col_writer tail is writen correctly
      ctx->pending_segment_context_cond_.wait_for(
        lock, std::chrono::milliseconds(1000) // arbitrary sleep interval
      );
    }
  }

  /////////////////////////////////////////////////////////////////////////////
  /// Stage 1
  /// update document_mask for existing (i.e. sealed) segments
  /////////////////////////////////////////////////////////////////////////////

  for (auto& existing_segment: meta_) {
    // skip already masked segments
    if (ctx->segment_mask_.end() != ctx->segment_mask_.find(existing_segment.meta.name)) {
      continue;
    }

    const auto segment_id = segments.size();
    segments.emplace_back(existing_segment);

    auto mask_modified = false;
    auto& segment = segments.back();

    docs_mask.clear();
    index_utils::read_document_mask(docs_mask, dir, segment.meta);

    // mask documents matching filters from segment_contexts (i.e. from new operations)
    for (auto& modifications: ctx->pending_segment_contexts_) {
      // modification_queries_ range [flush_segment_context::modification_offset_begin_, segment_context::uncomitted_modification_queries_)
      auto modifications_begin = modifications.modification_offset_begin_;
      auto modifications_end = modifications.segment_->uncomitted_modification_queries_;

      assert(modifications_begin <= modifications_end);
      assert(modifications_end <= modifications.segment_->modification_queries_.size());
      mask_modified |= add_document_mask_modified_records(
        &modifications.segment_->modification_queries_[modifications_begin], // modification start
        &modifications.segment_->modification_queries_[modifications_end], // modification end (exclusive)
        docs_mask,
        cached_readers_, // reader cache for segments
        segment.meta
      );
    }

    // write docs_mask if masks added, if all docs are masked then mask segment
    if (mask_modified) {
      // mask empty segments
      if (!segment.meta.live_docs_count) {
        ctx->segment_mask_.emplace(existing_segment.meta.name); // mask segment to clear reader cache
        segments.pop_back(); // remove empty segment
        modified = true; // removal of one fo the existing segments
        continue;
      }

      to_sync.register_partial_sync(segment_id, write_document_mask(dir, segment.meta, docs_mask));
      segment.filename = index_utils::write_segment_meta(dir, segment.meta); // write with new mask
    }
  }

  /////////////////////////////////////////////////////////////////////////////
  /// Stage 2
  /// add pending complete segments registered by import or consolidation
  /////////////////////////////////////////////////////////////////////////////

  // number of candidates that have been registered for
  // pending consolidation
  size_t pending_candidates_count = 0;

  for (auto& pending_segment : ctx->pending_segments_) {
    // pending consolidation
    auto& candidates = pending_segment.consolidation_ctx.candidates;

    // unregisterer for all registered candidates
    auto unregister_segments = irs::make_finally([&candidates, this]() NOEXCEPT {
      if (candidates.empty()) {
        return;
      }

      SCOPED_LOCK(consolidation_lock_);
      for (const auto* candidate : candidates) {
        consolidating_segments_.erase(candidate);
      }
    });

    docs_mask.clear();

    bool pending_consolidation = pending_segment.consolidation_ctx.merger;

    if (pending_consolidation) {
      // pending consolidation request
      candidates_mapping_t mappings;
      const auto res = map_candidates(mappings, candidates, segments);

      if (res.second != candidates.size()) {
        // at least one candidate is missing
        // in pending meta can't finish consolidation
        IR_FRMT_WARN(
          "Failed to finish merge for segment '%s', found only '" IR_SIZE_T_SPECIFIER "' out of '" IR_SIZE_T_SPECIFIER "' candidates",
          pending_segment.segment.meta.name.c_str(),
          res.second,
          candidates.size()
        );

        continue; // skip this particular consolidation
      }

      // mask mapped candidates
      for (auto& mapping : mappings) {
        ctx->segment_mask_.emplace(mapping.first);
      }

      // have some changes, apply deletes
      if (res.first) {
        map_removals(
          mappings,
          pending_segment.consolidation_ctx.merger,
          cached_readers_,
          docs_mask
        );
      }

      // we're done with removals for pending consolidation
      // they have been already applied to candidates above
      // and succesfully remapped to consolidated segment
      pending_segment.segment.meta.live_docs_count -= docs_mask.size();

      // we've seen at least 1 successfully applied
      // pending consolidation request
      pending_candidates_count += candidates.size();
    } else {
      // pending already imported/consolidated segment, apply deletes
      // mask documents matching filters from segment_contexts (i.e. from new operations)
      for (auto& modifications: ctx->pending_segment_contexts_) {
        // modification_queries_ range [flush_segment_context::modification_offset_begin_, segment_context::uncomitted_modification_queries_)
        auto modifications_begin = modifications.modification_offset_begin_;
        auto modifications_end = modifications.segment_->uncomitted_modification_queries_;

        assert(modifications_begin <= modifications_end);
        assert(modifications_end <= modifications.segment_->modification_queries_.size());
        add_document_mask_modified_records(
          &modifications.segment_->modification_queries_[modifications_begin], // modification start
          &modifications.segment_->modification_queries_[modifications_end], // modification end (exclusive)
          docs_mask,
          cached_readers_, // reader cache for segments
          pending_segment.segment.meta,
          pending_segment.generation
        );
      }
    }

    // skip empty segments
    if (!pending_segment.segment.meta.live_docs_count) {
      ctx->segment_mask_.emplace(pending_segment.segment.meta.name);
      continue;
    }

    // write non-empty document mask
    if (!docs_mask.empty()) {
      write_document_mask(dir, pending_segment.segment.meta, docs_mask, !pending_consolidation);
      pending_consolidation = true; // force write new segment meta
    }

    // persist segment meta
    if (pending_consolidation) {
      pending_segment.segment.filename = index_utils::write_segment_meta(
        dir, pending_segment.segment.meta
      );
    }

    // register full segment sync
    to_sync.register_full_sync(segments.size());
    segments.emplace_back(std::move(pending_segment.segment));
  }

  if (pending_candidates_count) {
    // for pending consolidation we need to filter out
    // consolidation candidates after applying them
    index_meta::index_segments_t tmp;
    tmp.reserve(segments.size() - pending_candidates_count);

    auto begin = to_sync.segments.begin();
    auto end = to_sync.segments.end();

    for (size_t i = 0, size = segments.size(); i < size; ++i) {
      auto& segment = segments[i];

      // valid segment
      const bool valid = ctx->segment_mask_.end() == ctx->segment_mask_.find(segment.meta.name);

      if (begin != end && i == begin->first) {
        begin->first = valid ? tmp.size() : integer_traits<size_t>::const_max; // mark invalid
        ++begin;
      }

      if (valid) {
        tmp.emplace_back(std::move(segment));
      }
    }

    segments = std::move(tmp);
  }

  /////////////////////////////////////////////////////////////////////////////
  /// Stage 3
  /// create new segments
  /////////////////////////////////////////////////////////////////////////////

  {
    struct flush_segment_context {
      flush_context::pending_segment_context& ctx_;
      std::string filename_;

      flush_segment_context(flush_context::pending_segment_context& ctx)
        : ctx_(ctx) {
        assert(ctx.segment_);
        assert(ctx.segment_->writer_);
      }
    };

    std::vector<flush_segment_context> segment_ctxs;

    // proces all segments that have been seen by the current flush_context
    for (auto& pending_segment_context: ctx->pending_segment_contexts_) {
      auto& segment = pending_segment_context.segment_;

      if (!segment
          || !segment->writer_
          || !segment->writer_->initialized()) {
        continue; // skip empty segments (segmnets  without live docs will be removed in the next section)
      }

      segment_ctxs.emplace_back(pending_segment_context);

      auto& writer = *(segment->writer_);
      auto& pending_ctx = segment_ctxs.back();

      segment->flushed_meta_ = segment_meta(writer.name(), codec_); // prepare new meta for flush

      // flush segment, even for empty segments since this will clear internal segment_writer state
      if (!writer.flush(pending_ctx.filename_, segment->flushed_meta_)) {
        return pending_context_t();
      }
      // flush document_mask after regular flush() so remove_query can traverse

      if (!segment->writer_->docs_cached()) {
        continue; // a segment reader cannot be opened if there are no documents in the segment
      }

      // mask documents matching filters from all flushed segment_contexts (i.e. from new operations)
      for (auto& modifications: ctx->pending_segment_contexts_) {
        // modification_queries_ range [flush_segment_context::modification_offset_begin_, segment_context::uncomitted_modification_queries_)
        auto modifications_begin = modifications.modification_offset_begin_;
        auto modifications_end = modifications.segment_->uncomitted_modification_queries_;

        assert(modifications_begin <= modifications_end);
        assert(modifications_end <= modifications.segment_->modification_queries_.size());
        add_document_mask_modified_records(
          &modifications.segment_->modification_queries_[modifications_begin], // modification start
          &modifications.segment_->modification_queries_[modifications_end], // modification end (exclusive)
          *segment, // segment context holding documents
          pending_segment_context.doc_id_begin_, // document_contexts_ range [flush_segment_context::doc_id_begin_, segment_context::uncomitted_doc_ids_)
          cached_readers_ // reader cache for segments
        );
      }
    }

    // write docs_mask if !empty(), if all docs are masked then remove segment altogether
    for (auto& segment_ctx: segment_ctxs) {
      auto& pending_ctx = segment_ctx.ctx_;
      auto& segment = *(pending_ctx.segment_);
      auto& writer = *(segment.writer_);

      // if have a writer with potential update-replacement records then check if they were seen
      add_document_mask_unused_updates(segment, pending_ctx.doc_id_begin_);

      auto& docs_mask = writer.docs_mask();

      // mask empty segments
      if (!segment.flushed_meta_.live_docs_count) {
        ctx->segment_mask_.emplace(writer.name()); // ref to writer name will not change
        continue;
      }

      // write non-empty document mask
      if (!docs_mask.empty()) {
        write_document_mask(dir, segment.flushed_meta_, docs_mask);
        segment_ctx.filename_ =
          index_utils::write_segment_meta(dir, segment.flushed_meta_); // write with new mask
      }

      index_meta::index_segment_t index_segment(segment_meta(segment.flushed_meta_));

      index_segment.filename = std::move(segment_ctx.filename_);

      // register full segment sync
      to_sync.register_full_sync(segments.size());
      segments.emplace_back(std::move(index_segment));
    }
  }

  pending_meta->update_generation(meta_); // clone index metadata generation
  cached_readers_.purge(ctx->segment_mask_); // release cached readers

  modified |= !to_sync.empty(); // new files added

  // only flush a new index version upon a new index or a metadata change
  if (!modified) {
    return pending_context_t();
  }

  pending_meta->seg_counter_.store(meta_.counter()); // ensure counter() >= max(seg#)

  pending_context_t pending_context;
  pending_context.ctx = std::move(ctx); // retain flush context reference
  pending_context.meta = std::move(pending_meta); // retain meta pending flush
  pending_context.to_sync = std::move(to_sync);
  meta_.segments_ = pending_context.meta->segments_; // create copy

  return pending_context;
}

bool index_writer::begin() {
  SCOPED_LOCK(commit_lock_);
  return start();
}

bool index_writer::start() {
  REGISTER_TIMER_DETAILED();

  if (pending_state_) {
    // begin has been already called 
    // without corresponding call to commit
    return false;
  }

  auto to_commit = flush_all();

  if (!to_commit) {
    // nothing to commit, no transaction started
    return false;
  }

  auto& dir = *to_commit.ctx->dir_;
  auto& pending_meta = *to_commit.meta;

  // write 1st phase of index_meta transaction
  if (!writer_->prepare(dir, pending_meta)) {
    throw illegal_state();
  }

  // sync all pending files
  try {
    auto update_generation = make_finally([this, &pending_meta] {
      meta_.update_generation(pending_meta);
    });

    auto sync = [&dir](const std::string& file) {
      if (!dir.sync(file)) {
        throw detailed_io_error("Failed to sync file, path: ") << file;
      }

      return true;
    };

    // sync files
    to_commit.to_sync.visit(sync, pending_meta);
  } catch (...) {
    // in case of syncing error, just clear pending meta & peform rollback
    // next commit will create another meta & sync all pending files
    writer_->rollback();
    pending_state_.reset(); // flush is rolled back
    throw;
  }

  // track all refs
  file_refs_t pending_refs;

  append_segments_refs(pending_refs, dir, pending_meta);
  pending_refs.emplace_back(
    directory_utils::reference(dir, writer_->filename(pending_meta), true)
  );

  // 1st phase of the transaction successfully finished here,
  // set to_commit as active flush context containing pending meta
  pending_state_.commit = memory::make_shared<committed_state_t::element_type>(
    std::piecewise_construct,
    std::forward_as_tuple(std::move(to_commit.meta)),
    std::forward_as_tuple(std::move(pending_refs))
  );
  pending_state_.ctx = std::move(to_commit.ctx);

  return true;
}

void index_writer::finish() {
  REGISTER_TIMER_DETAILED();

  if (!pending_state_) {
    return;
  }

  // ...........................................................................
  // lightweight 2nd phase of the transaction
  // ...........................................................................

  writer_->commit();

  // ...........................................................................
  // after here transaction successfull (only noexcept operations below)
  // ...........................................................................

  committed_state_ = std::move(pending_state_.commit);
  meta_.last_gen_ = committed_state_->first->gen_; // update 'last_gen_' to last commited/valid generation
  pending_state_.reset(); // flush is complete, release reference to flush_context
}

void index_writer::commit() {
  SCOPED_LOCK(commit_lock_);

  start();
  finish();
}

void index_writer::rollback() {
  SCOPED_LOCK(commit_lock_);

  if (!pending_state_) {
    // there is no open transaction
    return;
  }

  // ...........................................................................
  // all functions below are noexcept 
  // ...........................................................................

  // guarded by commit_lock_
  writer_->rollback();
  pending_state_.reset();

  // reset actual meta, note that here we don't change 
  // segment counters since it can be changed from insert function
  meta_.reset(*(committed_state_->first));
}

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------