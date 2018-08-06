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
#include "utils/bitvector.hpp"
#include "utils/directory_utils.hpp"
#include "utils/index_utils.hpp"
#include "utils/timer_utils.hpp"
#include "utils/type_limits.hpp"
#include "index_writer.hpp"

#include <list>

NS_LOCAL

const size_t NON_UPDATE_RECORD = iresearch::integer_traits<size_t>::const_max; // non-update

// append file refs for files from the specified segments description
template<typename T, typename M>
void append_segments_refs(
  T& buf,
  iresearch::directory& dir,
  const M& meta
) {
  auto visitor = [&buf](const iresearch::index_file_refs::ref_t& ref)->bool {
    buf.emplace_back(ref);
    return true;
  };

  // track all files referenced in index_meta
  iresearch::directory_utils::reference(dir, meta, visitor, true);
}

const std::string& write_document_mask(
  iresearch::directory& dir,
  iresearch::segment_meta& meta,
  const iresearch::document_mask& docs_mask
) {
  assert(docs_mask.size() <= std::numeric_limits<uint32_t>::max());

  auto mask_writer = meta.codec->get_document_mask_writer();
  meta.files.erase(mask_writer->filename(meta)); // current filename
  ++meta.version; // segment modified due to new document_mask
  const auto& file = *meta.files.emplace(mask_writer->filename(meta)).first; // new/expected filename
  mask_writer->prepare(dir, meta);
  mask_writer->begin((uint32_t)docs_mask.size());
  write_all(*mask_writer, docs_mask.begin(), docs_mask.end());
  mask_writer->end();
  return file;
}

std::string write_segment_meta(
  iresearch::directory& dir,
  iresearch::segment_meta& meta) {
  auto writer = meta.codec->get_segment_meta_writer();
  writer->write(dir, meta);

  return writer->filename(meta);
}

NS_END // NS_LOCAL

NS_ROOT

// ----------------------------------------------------------------------------
// --SECTION--                                      index_writer implementation 
// ----------------------------------------------------------------------------

const std::string index_writer::WRITE_LOCK_NAME = "write.lock";

index_writer::flush_context::flush_context():
  generation_(0),
  writers_pool_(THREAD_COUNT) {
}

void index_writer::flush_context::reset() {
  consolidation_policies_.clear();
  generation_.store(0);
  dir_->clear_refs();
  modification_queries_.clear();
  pending_segments_.clear();
  segment_mask_.clear();
  writers_pool_.visit([](segment_writer& writer)->bool {
    writer.reset();
    return true;
  });
}

index_writer::index_writer( 
    index_lock::ptr&& lock,
    directory& dir,
    format::ptr codec,
    index_meta&& meta,
    committed_state_t&& committed_state
) NOEXCEPT:
    codec_(codec),
    committed_state_(std::move(committed_state)),
    dir_(dir),
    flush_context_pool_(2), // 2 because just swap them due to common commit lock
    meta_(std::move(meta)),
    writer_(codec->get_index_meta_writer()),
    write_lock_(std::move(lock)) {
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
  assert(write_lock_);
  SCOPED_LOCK(commit_lock_);

  if (!pending_state_
      && meta_.empty()
      && type_limits<type_t::index_gen_t>::valid(meta_.last_gen_)) {
    return; // already empty
  }

  auto ctx = get_flush_context(false);
  SCOPED_LOCK(ctx->mutex_); // ensure there are no active struct update operations

  auto pending_meta = memory::make_unique<index_meta>();

  cached_segment_readers_.clear(); // original readers no longer required
  pending_meta->update_generation(meta_); // clone index metadata generation
  pending_meta->seg_counter_.store(meta_.counter()); // ensure counter() >= max(seg#)
  //ctx.reset(); // clear context to avoid writing anything

  // write 1st phase of index_meta transaction
  if (!writer_->prepare(*(ctx->dir_), *(pending_meta))) {
    throw illegal_state();
  }

  // 1st phase of the transaction successfully finished here
  meta_.update_generation(*pending_meta); // ensure new generation reflected in 'meta_'
  pending_state_.ctx = std::move(ctx); // retain flush context reference
  pending_state_.meta = std::move(pending_meta); // retain meta pending flush
  finish();
  meta_.segments_.clear(); // noexcept op (clear after finish(), to match reset of pending_state_ inside finish(), allows recovery on clear() failure)
}

index_writer::ptr index_writer::make(directory& dir, format::ptr codec, OPEN_MODE mode) {
  // lock the directory
  auto lock = dir.make_lock(WRITE_LOCK_NAME);

  if (!lock || !lock->try_lock()) {
    throw lock_obtain_failed(WRITE_LOCK_NAME);
  }

  /* read from directory
   * or create index metadata */
  index_meta meta;
  std::vector<index_file_refs::ref_t> file_refs;
  {
    auto reader = codec->get_index_meta_reader();

    std::string segments_file;
    const bool index_exists = reader->last_segments_file(dir, segments_file);

    if (OM_CREATE == mode || (OM_CREATE_APPEND == mode && !index_exists)) {
      // Try to read. It allows us to
      // create against an index that's
      // currently open for searching

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

    auto lock_file_ref = iresearch::directory_utils::reference(dir, WRITE_LOCK_NAME);

    // file exists on fs_directory
    if (lock_file_ref) {
      file_refs.emplace_back(lock_file_ref);
    }
  }

  auto comitted_state = std::make_pair(
    memory::make_unique<index_meta>(meta),
    std::move(file_refs)
  );

  PTR_NAMED(
    index_writer,
    writer,
    std::move(lock), 
    dir, codec,
    std::move(meta),
    std::move(comitted_state)
  );

  directory_utils::remove_all_unreferenced(dir); // remove non-index files from directory

  return writer;
}

index_writer::~index_writer() {
  close();
}

void index_writer::close() {
  {
    SCOPED_LOCK(commit_lock_); // cached_segment_readers_ read/modified during flush()
    cached_segment_readers_.clear();
  }
  write_lock_.reset();
}

uint64_t index_writer::buffered_docs() const { 
  uint64_t docs_in_ram = 0;

  auto visitor = [&docs_in_ram](const segment_writer& writer) {
    docs_in_ram += writer.docs_cached();
    return true;
  };

  auto ctx = const_cast<index_writer*>(this)->get_flush_context();

  ctx->writers_pool_.visit(visitor, true);

  return docs_in_ram;
}

segment_reader index_writer::get_segment_reader(
  const segment_meta& meta
) {
  REGISTER_TIMER_DETAILED();
  auto it = cached_segment_readers_.find(meta.name);

  if (it == cached_segment_readers_.end()) {
    it = cached_segment_readers_.emplace(
      meta.name, segment_reader::open(dir_, meta)
    ).first;
  } else {
    it->second = it->second.reopen(meta);
  }

  if (!it->second) {
    cached_segment_readers_.erase(it);
  }

  return it->second;
}

bool index_writer::add_document_mask_modified_records(
    modification_requests_t& modification_queries,
    document_mask& docs_mask,
    const segment_meta& meta,
    size_t min_doc_id_generation /*= 0*/) {
  if (modification_queries.empty()) {
    return false; // nothing new to flush
  }

  bool modified = false;
  auto rdr = get_segment_reader(meta);

  if (!rdr) {
    throw index_error(); // failed to open segment
  }

  for (auto& mod : modification_queries) {
    auto prepared = mod.filter->prepare(rdr);

    for (auto docItr = prepared->execute(rdr); docItr->next();) {
      auto doc = docItr->value();

      // if indexed doc_id was not add()ed after the request for modification
      // and doc_id not already masked then mark query as seen and segment as modified
      if (mod.generation >= min_doc_id_generation &&
          docs_mask.insert(doc).second) {
        mod.seen = true;
        modified = true;
      }
    }
  }

  return modified;
}

bool index_writer::add_document_mask_modified_records(
  modification_requests_t& modification_queries,
  segment_writer& writer,
  const segment_meta& meta
) {
  if (modification_queries.empty()) {
    return false; // nothing new to flush
  }

  auto& doc_id_generation = writer.docs_context();
  bool modified = false;
  auto rdr = get_segment_reader(meta);

  if (!rdr) {
    throw index_error(); // failed to open segment
  }

  for (auto& mod : modification_queries) {
    if (!mod.filter) {
      continue; // skip invalid modification queries
    }

    auto prepared = mod.filter->prepare(rdr);

    for (auto docItr = prepared->execute(rdr); docItr->next();) {
      const auto doc = docItr->value() - (type_limits<type_t::doc_id_t>::min)();

      if (doc >= doc_id_generation.size()) {
        continue;
      }

      const auto& doc_ctx = doc_id_generation[doc];

      // if indexed doc_id was add()ed after the request for modification then it should be skipped
      if (mod.generation < doc_ctx.generation) {
        continue; // the current modification query does not match any records
      }

      // if not already masked
      if (writer.remove(doc)) {
        // if not an update modification (i.e. a remove modification) or
        // if non-update-value record or update-value record whose query was seen
        // for every update request a replacement 'update-value' is optimistically inserted
        if (!mod.update ||
            doc_ctx.update_id == NON_UPDATE_RECORD ||
            modification_queries[doc_ctx.update_id].seen) {
          mod.seen = true;
          modified = true;
        }
      }
    }
  }

  return modified;
}

/* static */ bool index_writer::add_document_mask_unused_updates(
    modification_requests_t& modification_queries,
    segment_writer& writer,
    const segment_meta& meta
) {
  UNUSED(meta);

  if (modification_queries.empty()) {
    return false; // nothing new to add
  }

  auto& doc_id_generation = writer.docs_context();
  bool modified = false;

  // the implementation generates doc_ids sequentially
//  for (doc_id_t doc = 0, count = meta.docs_count; doc < count; ++doc) {
//    if (doc >= doc_id_generation.size()) {
//      continue;
//    }
  doc_id_t doc = 0;
  for (const auto& doc_ctx : doc_id_generation) {

//    const auto& doc_ctx = doc_id_generation[doc];

    // if it's an update record placeholder who's query did not match any records
    if (doc_ctx.update_id != NON_UPDATE_RECORD
        && !modification_queries[doc_ctx.update_id].seen) {
      modified |= writer.remove(doc);
    }

    ++doc;
  }

  return modified;
}

bool index_writer::add_segment_mask_consolidated_records(
    index_meta::index_segment_t& segment,
    directory& dir,
    flush_context::segment_mask_t& segments_mask,
    const index_meta& meta, // current state to examine for consolidation candidates
    const consolidation_requests_t& policies // policies dictating which segments to consider
) {
  REGISTER_TIMER_DETAILED();
  std::vector<segment_reader> merge_candidates;
  const index_meta::index_segment_t* merge_candindate_default = nullptr;
  flush_context::segment_mask_t segment_mask;
  bitvector policy_candidates(meta.size()); // candidates as deemed by individual policies
  bitvector consolidation_mask(meta.size()); // consolidated segments
  size_t i = 0;

  // collect a list of consolidation candidates
  for (auto& policy: policies) {
    policy_candidates = consolidation_mask;
    (*(policy.policy))(policy_candidates, dir, meta);
    consolidation_mask |= policy_candidates;
  }

  // find merge candidates
  for (auto& seg: meta) {
    if (!consolidation_mask.test(i++)) {
      merge_candindate_default = &seg; // pick the last non-merged segment as default
      continue; // fill min threshold not reached
    }

    auto merge_candidate = get_segment_reader(seg.meta);

    if (!merge_candidate) {
      continue; // skip empty readers
    }

    merge_candidates.emplace_back(std::move(merge_candidate));
    segment_mask.insert(seg.meta.name);
  }

  if (merge_candidates.empty()) {
    return false; // nothing to merge
  }

  if (merge_candidates.size() < 2) {
    if (!merge_candindate_default) {
      if (merge_candidates[0].docs_count() == merge_candidates[0].live_docs_count()) {
        return false; // no reason to consolidate a segment without any masked documents
      }
    } else { // if only one merge candidate and another segment available then merge with other
      auto merge_candidate = get_segment_reader(merge_candindate_default->meta);

      if (!merge_candidate) {
        return false; // failed to open segment and nothing else to merge with
      }

      merge_candidates.emplace_back(std::move(merge_candidate));
      segment_mask.insert(merge_candindate_default->meta.name);
    }
  }

  REGISTER_TIMER_DETAILED();
  segment.meta.codec = codec_;
  segment.meta.name = file_name(meta_.increment()); // increment active meta, not fn arg

  merge_writer merge_writer(dir, segment.meta.name);

  for (auto& merge_candidate: merge_candidates) {
    merge_writer.add(merge_candidate);
  }

  if (!merge_writer.flush(segment.filename, segment.meta)) {
    return false; // import failure (no files created, nothing to clean up)
  }

  segments_mask.insert(segment_mask.begin(), segment_mask.end());

  return true;
}

void index_writer::consolidate(
  const consolidation_policy_t& policy, bool immediate
) {
  if (immediate) {
    REGISTER_TIMER_DETAILED();
    index_meta::index_segment_t segment;
    std::unordered_map<string_ref, const segment_meta*> segment_candidates;
    SCOPED_LOCK(commit_lock_); // ensure meta_ segments are not modified by concurrent consolidate()/commit()
    auto ctx = get_flush_context(); // can modify ctx->segment_mask_ without lock since have commit_lock_

    for (auto& seg: meta_) {
      if (ctx->segment_mask_.end() == ctx->segment_mask_.find(seg.meta.name)) {
        segment_candidates.emplace(seg.meta.name, &(seg.meta));
      }
    }

    auto meta = *(committed_state_.first);

    meta.clear();

    // add segments present in both 'committed_state_.first' and 'meta_' into meta
    for (auto& segment: *(committed_state_.first)) {
      auto itr = segment_candidates.find(segment.meta.name);

      if (segment_candidates.end() != itr
          && segment.meta.version == itr->second->version) {
        meta.add(&segment, &segment + 1);
      }
    }

    consolidation_requests_t policies;

    policies.emplace_back(policy);

    // for immediate consolidate consider only committed segments that are still unmodified in current meta
    if (add_segment_mask_consolidated_records(segment, *(ctx->dir_), ctx->segment_mask_, meta, policies)) {
      SCOPED_LOCK(ctx->mutex_); // lock due to context modification

      // add a policy to hold a reference to committed_meta so that segment refs do not disapear
      ctx->consolidation_policies_.emplace_back(
        [meta](bitvector&, const directory&, const index_meta&)->void {}
      );

      // 0 == merged segments existed before start of tx (all removes apply)
      ctx->pending_segments_.emplace_back(std::move(segment), 0);
    }

    return;
  }

  REGISTER_TIMER_DETAILED();
  auto ctx = get_flush_context();
  SCOPED_LOCK(ctx->mutex_); // lock due to context modification

  ctx->consolidation_policies_.emplace_back(policy);
}

void index_writer::consolidate(
  const std::shared_ptr<consolidation_policy_t>& policy, bool immediate
) {
  if (immediate) {
    consolidate(*policy, immediate);
    return;
  }

  auto ctx = get_flush_context();
  SCOPED_LOCK(ctx->mutex_); // lock due to context modification

  ctx->consolidation_policies_.emplace_back(policy);
}

void index_writer::consolidate(
  consolidation_policy_t&& policy, bool immediate
) {
  if (immediate) {
    consolidate(policy, immediate);
    return;
  }

  auto ctx = get_flush_context();
  SCOPED_LOCK(ctx->mutex_); // lock due to context modification

  ctx->consolidation_policies_.emplace_back(std::move(policy));
}

bool index_writer::import(const index_reader& reader) {
  if (!reader.live_docs_count()) {
    return true; // skip empty readers since no documents to import
  }

  auto ctx = get_flush_context();
  auto merge_segment_name = file_name(meta_.increment());
  merge_writer merge_writer(*(ctx->dir_), merge_segment_name);

  for (auto itr = reader.begin(), end = reader.end(); itr != end; ++itr) {
    merge_writer.add(*itr);
  }

  index_meta::index_segment_t segment(segment_meta(merge_segment_name, codec_));

  if (!merge_writer.flush(segment.filename, segment.meta)) {
    return false; // import failure (no files created, nothing to clean up)
  }

  SCOPED_LOCK(ctx->mutex_); // lock due to context modification

  ctx->pending_segments_.emplace_back(
    std::move(segment),
    ctx->generation_.load() // current modification generation
  );

  return true;
}

index_writer::flush_context::ptr index_writer::get_flush_context(bool shared /*= true*/) {
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

      return flush_context::ptr(ctx, false);
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

    return flush_context::ptr(ctx, true);
  }
}

index_writer::flush_context::segment_writers_t::ptr index_writer::get_segment_context(
    flush_context& ctx) {
  auto writer = ctx.writers_pool_.emplace(*(ctx.dir_));

  if (!writer->initialized()) {
    writer->reset(segment_meta(file_name(meta_.increment()), codec_));
  }

  return writer;
}

void index_writer::remove(const filter& filter) {
  auto ctx = get_flush_context();
  SCOPED_LOCK(ctx->mutex_); // lock due to context modification
  ctx->modification_queries_.emplace_back(filter, ctx->generation_++, false);
}

void index_writer::remove(const std::shared_ptr<filter>& filter) {
  if (!filter) {
    return; // skip empty filters
  }

  auto ctx = get_flush_context();
  SCOPED_LOCK(ctx->mutex_); // lock due to context modification
  ctx->modification_queries_.emplace_back(filter, ctx->generation_++, false);
}

void index_writer::remove(filter::ptr&& filter) {
  if (!filter) {
    return; // skip empty filters
  }

  auto ctx = get_flush_context();
  SCOPED_LOCK(ctx->mutex_); // lock due to context modification
  ctx->modification_queries_.emplace_back(std::move(filter), ctx->generation_++, false);
}

index_writer::pending_context_t index_writer::flush_all() {
  REGISTER_TIMER_DETAILED();
  bool modified = !type_limits<type_t::index_gen_t>::valid(meta_.last_gen_);
  index_meta::index_segments_t segments;
  std::unordered_set<string_ref> to_sync;

  auto ctx = get_flush_context(false);
  auto& dir = *(ctx->dir_);
  SCOPED_LOCK(ctx->mutex_); // ensure there are no active struct update operations

  // update document_mask for existing (i.e. sealed) segments
  for (auto& existing_segment: meta_) {
    // skip already masked segments
    if (ctx->segment_mask_.end() != ctx->segment_mask_.find(existing_segment.meta.name)) {
      continue;
    }

    segments.emplace_back(existing_segment);

    auto& segment = segments.back();
    document_mask docs_mask;

    index_utils::read_document_mask(docs_mask, dir, segment.meta);

    // write docs_mask if masks added, if all docs are masked then mask segment
    if (add_document_mask_modified_records(ctx->modification_queries_, docs_mask, segment.meta)) {
      // mask empty segments
      if (docs_mask.size() == segment.meta.docs_count) {
        segments.pop_back();
        modified = true; // removal of one fo the existing segments
        continue;
      }

      to_sync.emplace(write_document_mask(dir, segment.meta, docs_mask));
      segment.filename = write_segment_meta(dir, segment.meta); // write with new mask
    }
  }

  // add pending complete segments
  for (auto& pending_segment: ctx->pending_segments_) {
    segments.emplace_back(std::move(pending_segment.segment));

    auto& segment = segments.back();
    document_mask docs_mask;

    // flush document_mask after regular flush() so remove_query can traverse
    add_document_mask_modified_records(
      ctx->modification_queries_, docs_mask, segment.meta, pending_segment.generation
    );

    // remove empty segments
    if (docs_mask.size() == segment.meta.docs_count) {
      segments.pop_back();
      continue;
    }

    // write non-empty document mask
    if (!docs_mask.empty()) {
      write_document_mask(dir, segment.meta, docs_mask);
      segment.filename = write_segment_meta(dir, segment.meta); // write with new mask
    }

    // add files from segment to list of files to sync
    to_sync.insert(segment.meta.files.begin(), segment.meta.files.end());
  }

  {
    struct flush_context {
      size_t segment_offset;
      segment_writer& writer;
      flush_context(
        size_t v_segment_offset, segment_writer& v_writer
      ): segment_offset(v_segment_offset), writer(v_writer) {}
      flush_context& operator=(const flush_context&) = delete; // because of reference
    };

    std::vector<flush_context> segment_ctxs;
    auto flush = [this, &ctx, &segments, &segment_ctxs](segment_writer& writer) {
      if (!writer.initialized()) {
        return true;
      }

      segment_ctxs.emplace_back(segments.size(), writer);
      segments.emplace_back(segment_meta(writer.name(), codec_));

      auto& segment = segments.back();

      if (!writer.flush(segment.filename, segment.meta)) {
        return false;
      }

      // flush document_mask after regular flush() so remove_query can traverse
      add_document_mask_modified_records(
        ctx->modification_queries_, writer, segment.meta
      );

      return true;
    };

    if (!ctx->writers_pool_.visit(flush)) {
      return pending_context_t();
    }

    // write docs_mask if !empty(), if all docs are masked then remove segment altogether
    for (auto& segment_ctx: segment_ctxs) {
      auto& segment = segments[segment_ctx.segment_offset];
      auto& writer = segment_ctx.writer;

      // if have a writer with potential update-replacement records then check if they were seen
      add_document_mask_unused_updates(
        ctx->modification_queries_, writer, segment.meta
      );

      auto& docs_mask = writer.docs_mask();

      // mask empty segments
      if (docs_mask.size() == segment.meta.docs_count) {
        ctx->segment_mask_.emplace(writer.name()); // ref to writer name will not change
        continue;
      }

      // write non-empty document mask
      if (!docs_mask.empty()) {
        write_document_mask(dir, segment.meta, docs_mask);
        segment.filename = write_segment_meta(dir, segment.meta); // write with new mask
      }

      // add files from segment to list of files to sync
      to_sync.insert(segment.meta.files.begin(), segment.meta.files.end());
    }
  }

  auto pending_meta = memory::make_unique<index_meta>();

  pending_meta->update_generation(meta_); // clone index metadata generation
  pending_meta->segments_.reserve(segments.size());

  // retain list of only non-masked segments
  if (ctx->segment_mask_.empty()) {
    pending_meta->segments_.swap(segments);
  } else {
    for (auto& segment: segments) {
      if (ctx->segment_mask_.end() == ctx->segment_mask_.find(segment.meta.name)) {
        pending_meta->segments_.emplace_back(std::move(segment));
      } else {
        cached_segment_readers_.erase(segment.meta.name); // no longer required
      }
    }
  }

  // process deferred merge policies
  if (!ctx->consolidation_policies_.empty() && !pending_meta->empty()) {
    index_meta::index_segment_t segment;
    flush_context::segment_mask_t segment_mask;
    auto consolidated = add_segment_mask_consolidated_records(
      segment,
      *(ctx->dir_),
      segment_mask,
      *pending_meta,
      ctx->consolidation_policies_
    );

    if (consolidated) {
      // add files from segment to list of files to sync
      to_sync.insert(segment.meta.files.begin(), segment.meta.files.end());

      // retain list of only non-masked segments
      pending_meta->segments_.swap(segments);
      pending_meta->segments_.clear();
      pending_meta->segments_.emplace_back(std::move(segment));

      for (auto& segment: segments) {
        if (segment_mask.end() == segment_mask.find(segment.meta.name)) {
          pending_meta->segments_.emplace_back(std::move(segment));
        } else {
          cached_segment_readers_.erase(segment.meta.name); // no longer required
        }
      }
    }
  }

  pending_context_t pending_context;
  flush_context::segment_mask_t segment_names;

  // create list of segment names and files requiring FS sync
  // for refs to be valid this must be done only after all changes ctx->meta_.segments_
  for (auto& segment: *pending_meta) {
    bool sync_segment = false;
    segment_names.emplace(segment.meta.name);

    for (auto& file: segment.meta.files) {
      if (to_sync.erase(file)) {
        pending_context.to_sync.emplace_back(file); // add modified files requiring FS sync
        sync_segment = true; // at least one file in the segment was modified
      }
    }

    // must sync segment.filename if at least one file in the segment was modified
    // since segment moved above all its segment.filename references were invalidated
    if (sync_segment) {
      pending_context.to_sync.emplace_back(segment.filename); // add modified files requiring FS sync
    }
  }

  modified |= !pending_context.to_sync.empty(); // new files added

  // remove stale readers from cache (could still catch readers for segments that were pop_back()'ed)
  for (auto itr = cached_segment_readers_.begin(); itr != cached_segment_readers_.end();) {
    if (segment_names.find(itr->first) == segment_names.end()) {
      itr = cached_segment_readers_.erase(itr);
    } else {
      ++itr;
    }
  }

  // only flush a new index version upon a new index or a metadata change
  if (!modified) {
    return pending_context_t();
  }

  pending_meta->seg_counter_.store(meta_.counter()); // ensure counter() >= max(seg#)
  pending_context.ctx = std::move(ctx); // retain flush context reference
  pending_context.meta = std::move(pending_meta); // retain meta pending flush
  segments = pending_context.meta->segments_; // create copy
  meta_.segments_.swap(segments); // noexcept op

  return pending_context;
}

/*static*/ segment_writer::update_context index_writer::make_update_context(
    flush_context& ctx) {
  return segment_writer::update_context {
    ctx.generation_.load(), // current modification generation
    NON_UPDATE_RECORD
  };
}

segment_writer::update_context index_writer::make_update_context(
    flush_context& ctx, const filter& filter) {
  auto generation = ++ctx.generation_;
  SCOPED_LOCK(ctx.mutex_); // lock due to context modification
  size_t update_id = ctx.modification_queries_.size();

  ctx.modification_queries_.emplace_back(filter, generation - 1, true); // -1 for previous generation

  return segment_writer::update_context {
    generation, // current modification generation
    update_id // entry in modification_queries_
  };
}

segment_writer::update_context index_writer::make_update_context(
    flush_context& ctx, const std::shared_ptr<filter>& filter) {
  auto generation = ++ctx.generation_;
  SCOPED_LOCK(ctx.mutex_); // lock due to context modification
  size_t update_id = ctx.modification_queries_.size();

  ctx.modification_queries_.emplace_back(filter, generation - 1, true); // -1 for previous generation

  return segment_writer::update_context {
    generation, // current modification generation
    update_id // entry in modification_queries_
  };
}

segment_writer::update_context index_writer::make_update_context(
    flush_context& ctx, filter::ptr&& filter) {
  assert(filter);
  auto generation = ++ctx.generation_;
  SCOPED_LOCK(ctx.mutex_); // lock due to context modification
  size_t update_id = ctx.modification_queries_.size();

  ctx.modification_queries_.emplace_back(std::move(filter), generation - 1, true); // -1 for previous generation

  return segment_writer::update_context {
    generation, // current modification generation
    update_id // entry in modification_queries_
  };
}

bool index_writer::begin() {
  SCOPED_LOCK(commit_lock_);
  return start();
}

bool index_writer::start() {
  REGISTER_TIMER_DETAILED();
  assert(write_lock_);

  if (pending_state_) {
    // begin has been already called 
    // without corresponding call to commit
    return false;
  }

  // !!! remember that to_sync_ stores string_ref's to index_writer::meta !!!
  // it's valid since there is no small buffer optimization at the moment
  auto to_commit = flush_all(); // index metadata to commit

  if (!to_commit) {
    return true; // nothing to commit
  }

  // write 1st phase of index_meta transaction
  if (!writer_->prepare(*(to_commit.ctx->dir_), *(to_commit.meta))) {
    throw illegal_state();
  }

  // sync all pending files
  try {
    auto update_generation = make_finally([this, &to_commit] {
      meta_.update_generation(*(to_commit.meta));
    });

    // sync files
    for (auto& file: to_commit.to_sync) {
      if (!to_commit.ctx->dir_->sync(file)) {
        throw detailed_io_error("Failed to sync file, path: ") << file;
      }
    }
  } catch (...) {
    // in case of syncing error, just clear pending meta & peform rollback
    // next commit will create another meta & sync all pending files
    writer_->rollback();
    pending_state_.reset(); // flush is rolled back
    throw;
  }

  // 1st phase of the transaction successfully finished here,
  // set to_commit as active flush context containing pending meta
  pending_state_.ctx = std::move(to_commit.ctx);
  pending_state_.meta = std::move(to_commit.meta);

  return true;
}

void index_writer::finish() {
  REGISTER_TIMER_DETAILED();

  if (!pending_state_) {
    return;
  }

  auto& ctx = *(pending_state_.ctx);
  auto& dir = *(ctx.dir_);
  auto& meta = *(pending_state_.meta);
  committed_state_t committed_state;
  auto lock_file_ref = iresearch::directory_utils::reference(dir, WRITE_LOCK_NAME);

  if (lock_file_ref) {
    // file exists on fs_directory
    committed_state.second.emplace_back(lock_file_ref);
  }

  committed_state.second.emplace_back(iresearch::directory_utils::reference(dir, writer_->filename(meta), true));
  append_segments_refs(committed_state.second, dir, meta);
  writer_->commit();
  meta_.last_gen_ = meta.gen_; // update 'last_gen_' to last commited/valid generation

  // ...........................................................................
  // after here transaction successfull
  // ...........................................................................

  committed_state.first = std::move(pending_state_.meta);
  committed_state_ = std::move(committed_state);
  pending_state_.reset(); // flush is complete, release referecne to flush_context
}

void index_writer::commit() {
  assert(write_lock_);
  SCOPED_LOCK(commit_lock_);

  start();
  finish();
}

void index_writer::rollback() {
  assert(write_lock_);
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
  meta_.reset(*(committed_state_.first));
}

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------