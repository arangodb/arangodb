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
#include "comparer.hpp"
#include "formats/format_utils.hpp"
#include "search/exclusion.hpp"
#include "utils/bitvector.hpp"
#include "utils/compression.hpp"
#include "utils/directory_utils.hpp"
#include "utils/index_utils.hpp"
#include "utils/string_utils.hpp"
#include "utils/timer_utils.hpp"
#include "utils/type_limits.hpp"
#include "utils/range.hpp"
#include "index_writer.hpp"

#include <list>
#include <sstream>

namespace {

typedef range<irs::index_writer::modification_context> modification_contexts_ref;
typedef range<irs::segment_writer::update_context> update_contexts_ref;

const size_t NON_UPDATE_RECORD = irs::integer_traits<size_t>::const_max; // non-update

const irs::column_info_provider_t DEFAULT_COLUMN_INFO = [](const irs::string_ref&) {
  // no compression, no encryption
  return irs::column_info{ irs::type<irs::compression::none>::get(), {}, false };
};

struct flush_segment_context {
  const size_t doc_id_begin_; // starting doc_id to consider in 'segment.meta' (inclusive)
  const size_t doc_id_end_; // ending doc_id to consider in 'segment.meta' (exclusive)
  irs::document_mask docs_mask_; // doc_ids masked in segment_meta
  const modification_contexts_ref modification_contexts_; // modification contexts referenced by 'update_contexts_'
  irs::index_meta::index_segment_t segment_; // copy so that it can be moved into 'index_writer::pending_state_'
  const update_contexts_ref update_contexts_; // update contexts for documents in segment_meta

  flush_segment_context(
    const irs::index_meta::index_segment_t& segment,
    size_t doc_id_begin,
    size_t doc_id_end,
    const update_contexts_ref& update_contexts,
    const modification_contexts_ref& modification_contexts
  ): doc_id_begin_(doc_id_begin),
     doc_id_end_(doc_id_end),
     modification_contexts_(modification_contexts),
     segment_(segment),
     update_contexts_(update_contexts) {
    assert(doc_id_begin_ <= doc_id_end_);
    assert(doc_id_end_ - irs::doc_limits::min() <= segment_.meta.docs_count);
    assert(update_contexts.size() == segment_.meta.docs_count);
  }
};

std::vector<irs::index_file_refs::ref_t> extract_refs(
    const irs::ref_tracking_directory& dir) {
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
/// @param modifications where to get document update_contexts from
/// @param docs_mask where to apply document removals to
/// @param readers readers by segment name
/// @param meta key used to get reader for the segment to evaluate
/// @param min_modification_generation smallest consider modification generation
/// @return if any new records were added (modification_queries_ modified)
////////////////////////////////////////////////////////////////////////////////
bool add_document_mask_modified_records(
    modification_contexts_ref& modifications, // where to get document update_contexts from
    irs::document_mask& docs_mask, // where to apply document removals to
    irs::readers_cache& readers, // where to get segment readers from
    irs::segment_meta& meta, // key used to get reader for the segment to evaluate
    size_t min_modification_generation = 0
) {
  if (modifications.empty()) {
    return false; // nothing new to flush
  }

  auto reader = readers.emplace(meta);

  if (!reader) {
    throw irs::index_error(irs::string_utils::to_string(
      "while adding document mask modified records to document_mask of segment '%s', error: failed to open segment",
      meta.name.c_str()
    ));
  }

  bool modified = false;

  for (auto& modification : modifications) {
    if (!modification.filter) {
      continue; // skip invalid or uncommitted modification queries
    }

    auto prepared = modification.filter->prepare(reader);

    if (!prepared) {
      continue; // skip invalid prepared filters
    }

    auto itr = prepared->execute(reader);

    if (!itr) {
      continue; // skip invalid iterators
    }

    while (itr->next()) {
      const auto doc_id = itr->value();

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
/// @param modifications where to get document update_contexts from
/// @param segment where to apply document removals to
/// @param min_doc_id staring doc_id that should be considered
/// @param readers readers by segment name
/// @return if any new records were added (modification_queries_ modified)
////////////////////////////////////////////////////////////////////////////////
bool add_document_mask_modified_records(
    modification_contexts_ref& modifications, // where to get document update_contexts from
    flush_segment_context& ctx, // where to apply document removals to
    irs::readers_cache& readers // where to get segment readers from
) {
  if (modifications.empty()) {
    return false; // nothing new to flush
  }

  auto reader = readers.emplace(ctx.segment_.meta);

  if (!reader) {
    throw irs::index_error(irs::string_utils::to_string(
      "while adding document mask modified records to flush_segment_context of segment '%s', error: failed to open segment",
      ctx.segment_.meta.name.c_str()
    ));
  }

  assert(irs::doc_limits::valid(ctx.doc_id_begin_));
  assert(ctx.doc_id_begin_ <= ctx.doc_id_end_);
  assert(ctx.doc_id_end_ <= ctx.update_contexts_.size() + irs::doc_limits::min());
  bool modified = false;

  for (auto& modification : modifications) {
    if (!modification.filter) {
      continue; // skip invalid or uncommitted modification queries
    }

    auto prepared = modification.filter->prepare(reader);

    if (!prepared) {
      continue; // skip invalid prepared filters
    }

    auto itr = prepared->execute(reader);

    if (!itr) {
      continue; // skip invalid iterators
    }

    while (itr->next()) {
      const auto doc_id = itr->value();

      if (doc_id < ctx.doc_id_begin_ || doc_id >= ctx.doc_id_end_) {
        continue; // doc_id is not part of the current flush_context
      }

      auto& doc_ctx = ctx.update_contexts_[doc_id - irs::doc_limits::min()]; // valid because of asserts above

      // if the indexed doc_id was insert()ed after the request for modification
      // or the indexed doc_id was already masked then it should be skipped
      if (modification.generation < doc_ctx.generation
          || !ctx.docs_mask_.insert(doc_id).second) {
        continue; // the current modification query does not match any records
      }

      // if an update modification and update-value record whose query was not
      // seen (i.e. replacement value whose filter did not match any documents)
      // for every update request a replacement 'update-value' is optimistically inserted
      if (modification.update
          && doc_ctx.update_id != NON_UPDATE_RECORD
          && !ctx.modification_contexts_[doc_ctx.update_id].seen) {
        continue; // the current modification matched a replacement document which in turn did not match any records
      }

      assert(ctx.segment_.meta.live_docs_count);
      --ctx.segment_.meta.live_docs_count; // decrement count of live docs
      modification.seen = true;
      modified = true;
    }
  }

  return modified;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief mask documents created by updates which did not have any matches
/// @return if any new records were added (modification_contexts_ modified)
////////////////////////////////////////////////////////////////////////////////
bool add_document_mask_unused_updates(flush_segment_context& ctx) {
  if (ctx.modification_contexts_.empty()) {
    return false; // nothing new to add
  }
  assert(irs::doc_limits::valid(ctx.doc_id_begin_));
  assert(ctx.doc_id_begin_ <= ctx.doc_id_end_);
  assert(ctx.doc_id_end_ <= ctx.update_contexts_.size() + irs::doc_limits::min());
  bool modified = false;

  for (auto doc_id = ctx.doc_id_begin_; doc_id < ctx.doc_id_end_; ++doc_id) {
    auto& doc_ctx = ctx.update_contexts_[doc_id - irs::doc_limits::min()]; // valid because of asserts above

    if (doc_ctx.update_id == NON_UPDATE_RECORD) {
      continue; // not an update operation
    }

    assert(ctx.modification_contexts_.size() > doc_ctx.update_id);

    // if it's an update record placeholder who's query already match some record
    if (ctx.modification_contexts_[doc_ctx.update_id].seen
        || !ctx.docs_mask_.insert(doc_id).second) {
      continue; // the current placeholder record is in-use and valid
    }

    assert(ctx.segment_.meta.live_docs_count);
    --ctx.segment_.meta.live_docs_count; // decrement count of live docs
    modified  = true;
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
    irs::directory& dir,
    irs::segment_meta& meta,
    const irs::document_mask& docs_mask,
    bool increment_version = true
) {
  assert(docs_mask.size() <= std::numeric_limits<uint32_t>::max());

  auto mask_writer = meta.codec->get_document_mask_writer();

  if (increment_version) {
    meta.files.erase(mask_writer->filename(meta)); // current filename
    ++meta.version; // segment modified due to new document_mask

    // a second time +1 to avoid overlap with version increment due to commit of
    // uncommited segment tail which must mask committed segment head
    // NOTE0: +1 extra is enough since a segment can reside in at most 2
    //        flush_contexts, there fore no more than 1 tail
    // NOTE1: flush_all() Stage3 increments version by _only_ 1 to avoid overlap
    //        with here, i.e. segment tail version will always be odd due to the
    //        aforementioned and because there is at most 1 tail
    ++meta.version;
  }

  const auto& file = *meta.files.emplace(mask_writer->filename(meta)).first; // new/expected filename

  mask_writer->write(dir, meta, docs_mask);
  meta.size = 0; // reset no longer valid size, to be recomputed on index_utils::write_index_segment(...)

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
    const std::set<const irs::segment_meta*>& candidates,
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

    auto& mapping = it->second;
    auto* new_segment = mapping.first;

    if (new_segment && new_segment->version >= meta.version) {
      // mapping already has a newer segment version
      continue;
    }

    ++found;

    assert(mapping.second.first);
    mapping.first = &meta;

    has_removals |= (meta.version != it->second.second.first->version);
  }

  return std::make_pair(has_removals, found);
}

bool map_removals(
    const candidates_mapping_t& candidates_mapping,
    const irs::merge_writer& merger,
    irs::readers_cache& readers,
    irs::document_mask& docs_mask) {
  assert(merger);

  for (auto& mapping : candidates_mapping) {
    const auto& segment_mapping = mapping.second;

    if (segment_mapping.first->version != segment_mapping.second.first->version) {
      auto& merge_ctx = merger[segment_mapping.second.second];
      auto reader = readers.emplace(*segment_mapping.first);
      auto merged_itr = merge_ctx.reader->docs_iterator();
      auto current_itr = reader->docs_iterator();

      // this only masks documents of a single segment
      // this works due to the current architectural approach of segments,
      // either removals are new and will be applied during flush_all()
      // or removals are in the docs_mask and swill be applied by the reader
      // passed to the merge_writer

      // no more docs in merged reader
      if (!merged_itr->next()) {
        if (current_itr->next()) {
          IR_FRMT_WARN(
            "Failed to map removals for consolidated segment '%s' version '" IR_UINT64_T_SPECIFIER "' from current segment '%s' version '" IR_UINT64_T_SPECIFIER "', current segment has doc_id '" IR_UINT32_T_SPECIFIER "' not present in the consolidated segment",
            segment_mapping.second.first->name.c_str(),
            segment_mapping.second.first->version,
            segment_mapping.first->name.c_str(),
            segment_mapping.first->version,
            current_itr->value()
          );

          return false; // current reader has unmerged docs
        }

        continue; // continue wih next mapping
      }

      // mask all remaining doc_ids
      if (!current_itr->next()) {
        do {
          assert(irs::doc_limits::valid(merge_ctx.doc_map(merged_itr->value()))); // doc_id must have a valid mapping
          docs_mask.insert(merge_ctx.doc_map(merged_itr->value()));
        } while (merged_itr->next());

        continue; // continue wih next mapping
      }

      // validate that all docs in the current reader were merged, and add any removed docs to the meged mask
      for (;;) {
        while (merged_itr->value() < current_itr->value()) {
          assert(irs::doc_limits::valid(merge_ctx.doc_map(merged_itr->value()))); // doc_id must have a valid mapping
          docs_mask.insert(merge_ctx.doc_map(merged_itr->value()));

          if (!merged_itr->next()) {
            IR_FRMT_WARN(
              "Failed to map removals for consolidated segment '%s' version '" IR_UINT64_T_SPECIFIER "' from current segment '%s' version '" IR_UINT64_T_SPECIFIER "', current segment has doc_id '" IR_UINT32_T_SPECIFIER "' not present in the consolidated segment",
              segment_mapping.second.first->name.c_str(),
              segment_mapping.second.first->version,
              segment_mapping.first->name.c_str(),
              segment_mapping.first->version,
              current_itr->value()
            );

            return false; // current reader has unmerged docs
          }
        }

        if (merged_itr->value() > current_itr->value()) {
          IR_FRMT_WARN(
            "Failed to map removals for consolidated segment '%s' version '" IR_UINT64_T_SPECIFIER "' from current segment '%s' version '" IR_UINT64_T_SPECIFIER "', current segment has doc_id '" IR_UINT32_T_SPECIFIER "' not present in the consolidated segment",
            segment_mapping.second.first->name.c_str(),
            segment_mapping.second.first->version,
            segment_mapping.first->name.c_str(),
            segment_mapping.first->version,
            current_itr->value()
          );

          return false; // current reader has unmerged docs
        }

        // no more docs in merged reader
        if (!merged_itr->next()) {
          if (current_itr->next()) {
            IR_FRMT_WARN(
              "Failed to map removals for consolidated segment '%s' version '" IR_UINT64_T_SPECIFIER "' from current segment '%s' version '" IR_UINT64_T_SPECIFIER "', current segment has doc_id '" IR_UINT32_T_SPECIFIER "' not present in the consolidated segment",
              segment_mapping.second.first->name.c_str(),
              segment_mapping.second.first->version,
              segment_mapping.first->name.c_str(),
              segment_mapping.first->version,
              current_itr->value()
            );

            return false; // current reader has unmerged docs
          }

          break; // continue wih next mapping
        }

        // mask all remaining doc_ids
        if (!current_itr->next()) {
          do {
            assert(irs::doc_limits::valid(merge_ctx.doc_map(merged_itr->value()))); // doc_id must have a valid mapping
            docs_mask.insert(merge_ctx.doc_map(merged_itr->value()));
          } while (merged_itr->next());

          break;  // continue wih next mapping
        }
      }
    }
  }

  return true;
}

std::string to_string(std::set<const irs::segment_meta*>& consolidation) {
  std::stringstream ss;
  size_t total_size = 0;
  size_t total_docs_count = 0;
  size_t total_live_docs_count = 0;

  for (const auto* meta : consolidation) {
    ss << "Name='" << meta->name
       << "', docs_count=" << meta->docs_count
       << ", live_docs_count=" << meta->live_docs_count
       << ", size=" << meta->size
       << std::endl;

    total_docs_count += meta->docs_count;
    total_live_docs_count += meta->live_docs_count;
    total_size += meta->size;
  }

  ss << "Total: segments=" << consolidation.size()
     << ", docs_count=" << total_docs_count
     << ", live_docs_count=" << total_live_docs_count
     << " size=" << total_size << "";

  return ss.str();
}

} // NS_LOCAL

namespace iresearch {

readers_cache::key_t::key_t(const segment_meta& meta)
  : name(meta.name), version(meta.version) {
}

segment_reader readers_cache::emplace(const segment_meta& meta) {
  REGISTER_TIMER_DETAILED();

  segment_reader cached_reader;

  // FIXME consider moving open/reopen out of the scope of the lock
  auto lock = make_lock_guard(lock_);
  auto& reader = cache_[meta];

  cached_reader = std::move(reader); // clear existing reader

  // update cache, in case of failure reader stays empty
  reader = cached_reader
    ? cached_reader.reopen(meta)
    : segment_reader::open(dir_, meta);

  return reader;
}

void readers_cache::clear() noexcept {
  auto lock = make_lock_guard(lock_);
  cache_.clear();
}

size_t readers_cache::purge(
    const std::unordered_set<key_t, key_hash_t>& segments) noexcept {
  if (segments.empty()) {
    return 0;
  }

  size_t erased = 0;

  auto lock = make_lock_guard(lock_);

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

index_writer::active_segment_context::active_segment_context(
    segment_context_ptr ctx,
    std::atomic<size_t>& segments_active,
    flush_context* flush_ctx /*= nullptr*/, // the flush_context the segment_context is currently registered with
    size_t pending_segment_context_offset /*= integer_traits<size_t>::const_max*/ // the segment offset in flush_ctx_->pending_segments_
) noexcept
  : ctx_(ctx),
    flush_ctx_(flush_ctx),
    pending_segment_context_offset_(pending_segment_context_offset),
    segments_active_(&segments_active) {
#ifdef IRESEARCH_DEBUG
  if (flush_ctx) {
    // ensure there are no active struct update operations (only needed for assert)
    auto lock = make_lock_guard(flush_ctx->mutex_);
    // assert that flush_ctx and ctx are compatible
    assert(flush_ctx->pending_segment_contexts_[pending_segment_context_offset_].segment_ == ctx_);
  }
#endif

  if (ctx_) {
    ++*segments_active_; // track here since garanteed to have 1 ref per active segment
  }
}

index_writer::active_segment_context::~active_segment_context() {
  if (ctx_) {
    --*segments_active_; // track here since garanteed to have 1 ref per active segment
  }

  if (flush_ctx_) {
    ctx_.reset();

    try {
      auto lock = make_lock_guard(flush_ctx_->mutex_);
      flush_ctx_->pending_segment_context_cond_.notify_all();
    } catch (...) {
      // lock may throw
    }
  } // FIXME TODO remove once col_writer tail is fixed to flush() multiple times without overwrite (since then the tail will be in a different context)
}

index_writer::active_segment_context& index_writer::active_segment_context::operator=(
    active_segment_context&& other
) noexcept {
  if (this != &other) {
    if (ctx_) {
      --*segments_active_; // track here since garanteed to have 1 ref per active segment
    }

    ctx_ = std::move(other.ctx_);
    flush_ctx_ = std::move(other.flush_ctx_);
    pending_segment_context_offset_ = std::move(other.pending_segment_context_offset_);
    segments_active_ = std::move(other.segments_active_);
  }

  return *this;
}

index_writer::documents_context::document::document(
    flush_context_ptr&& ctx,
    const segment_context_ptr& segment,
    const segment_writer::update_context& update
): segment_writer::document(*(segment->writer_)),
   ctx_(*ctx),
   segment_(segment),
   update_id_(update.update_id) {
  assert(ctx);
  assert(segment_);
  assert(segment_->writer_);
  auto& writer = *(segment_->writer_);
  auto uncomitted_doc_id_begin =
    segment_->uncomitted_doc_id_begin_ > segment_->flushed_update_contexts_.size()
    ? (segment_->uncomitted_doc_id_begin_ - segment_->flushed_update_contexts_.size()) // uncomitted start in 'writer_'
    : doc_limits::min(); // uncommited start in 'flushed_'
  assert(uncomitted_doc_id_begin <= writer.docs_cached() + doc_limits::min());
  auto rollback_extra =
    writer.docs_cached() + doc_limits::min() - uncomitted_doc_id_begin; // ensure reset() will be noexcept
  ++segment->active_count_;
  writer.begin(update, rollback_extra); // ensure reset() will be noexcept
  segment_->buffered_docs_.store(writer.docs_cached());
}

index_writer::documents_context::document::document(document&& other) noexcept
  : segment_writer::document(*(other.segment_->writer_)),
    ctx_(other.ctx_), // GCC does not allow moving of references
    segment_(std::move(other.segment_)),
    update_id_(std::move(other.update_id_)) {
}

index_writer::documents_context::document::~document() noexcept {
  if (!segment_) {
    return; // another instance will call commit()
  }

  assert(segment_->writer_);

  try {
    segment_->writer_->commit();
  } catch (...) {
    segment_->writer_->rollback();
  }

  if (!*this && update_id_ != NON_UPDATE_RECORD) {
    segment_->modification_queries_[update_id_].filter = nullptr; // mark invalid
  }

  // optimization to notify any ongoing flush_all() operations so they wake up earlier
  if (!--segment_->active_count_) {
    // lock due to context modification and notification, note: std::mutex::try_lock() does not throw exceptions as per
    // documentation @see https://en.cppreference.com/w/cpp/named_req/Mutex
    auto lock = make_unique_lock(ctx_.mutex_, std::try_to_lock);

    if (lock.owns_lock()) {
      ctx_.pending_segment_context_cond_.notify_all(); // ignore if lock failed because it imples that flush_all() is not waiting for a notification
    }
  }
}

index_writer::documents_context::~documents_context() noexcept {
  // failure may indicate a dangling 'document' instance
  assert(segment_.ctx().use_count() >= 0 &&
         static_cast<uint64_t>(segment_.ctx().use_count()) == segment_use_count_);

  if (segment_.ctx()) {
    auto& writer = *segment_.ctx()->writer_;

    if (writer.tick() < tick_) {
      writer.tick(tick_);
    }
  }

  try {
    // FIXME TODO move emplace into active_segment_context destructor
    writer_.get_flush_context()->emplace(std::move(segment_)); // commit segment
  } catch (...) {
    reset(); // abort segment
  }
}

void index_writer::documents_context::reset() noexcept {
  tick_ = 0; // reset tick

  auto& ctx = segment_.ctx();

  if (!ctx) {
    return; // nothing to reset
  }

  // rollback modification queries
  for (auto i = ctx->uncomitted_modification_queries_,
       count = ctx->modification_queries_.size();
       i < count;
       ++i) {
    ctx->modification_queries_[i].filter = nullptr; // mark invalid
  }

  auto& flushed_update_contexts = ctx->flushed_update_contexts_;

  // find and mask/truncate uncomitted tail
  for (size_t i = 0, count = ctx->flushed_.size(), flushed_docs_count = 0;
       i < count;
       ++i) {
    auto& segment = ctx->flushed_[i];
    auto flushed_docs_start = flushed_docs_count;

    flushed_docs_count += segment.meta.docs_count; // sum of all previous segment_meta::docs_count including this meta

    if (flushed_docs_count <= ctx->uncomitted_doc_id_begin_ - doc_limits::min()) {
      continue; // all documents in this this index_meta have been commited
    }

    auto docs_mask_tail_doc_id =
      ctx->uncomitted_doc_id_begin_ - flushed_docs_start;

    assert(docs_mask_tail_doc_id <= segment.meta.live_docs_count);
    assert(docs_mask_tail_doc_id <= integer_traits<doc_id_t>::const_max);
    segment.docs_mask_tail_doc_id = doc_id_t(docs_mask_tail_doc_id);

    if (docs_mask_tail_doc_id - doc_limits::min() >= segment.meta.docs_count) {
      ctx->flushed_.resize(i); // truncate including current empty meta
    } else {
      ctx->flushed_.resize(i + 1); // truncate starting from subsequent meta
    }

    assert(flushed_update_contexts.size() >= flushed_docs_count);
    flushed_update_contexts.resize(flushed_docs_count); // truncate 'flushed_update_contexts_'
    ctx->uncomitted_doc_id_begin_ = flushed_update_contexts.size() + doc_limits::min(); // reset to start of 'writer_'

    break;
  }

  if (!ctx->writer_) {
    assert(ctx->uncomitted_doc_id_begin_ - doc_limits::min() == flushed_update_contexts.size());
    ctx->buffered_docs_.store(flushed_update_contexts.size());

    return; // nothing to reset
  }

  auto& writer = *(ctx->writer_);
  auto writer_docs = writer.initialized() ? writer.docs_cached() : 0;

  assert(integer_traits<doc_id_t>::const_max >= writer.docs_cached());
  assert(ctx->uncomitted_doc_id_begin_ - doc_limits::min() >= flushed_update_contexts.size()); // update_contexts located inside th writer
  assert(ctx->uncomitted_doc_id_begin_ - doc_limits::min() <= flushed_update_contexts.size() + writer_docs);
  ctx->buffered_docs_.store(flushed_update_contexts.size() + writer_docs);

  // rollback document insertions
  // cannot segment_writer::reset(...) since documents_context::reset() noexcept
  for (auto doc_id = ctx->uncomitted_doc_id_begin_ - flushed_update_contexts.size(),
       doc_id_end = writer_docs + doc_limits::min();
       doc_id < doc_id_end;
       ++doc_id) {
    assert(doc_id <= integer_traits<doc_id_t>::const_max);
    writer.remove(doc_id_t(doc_id));
  }
}

index_writer::flush_context_ptr index_writer::documents_context::update_segment() {
  auto ctx = writer_.get_flush_context();

  // ...........................................................................
  // refresh segment if required (guarded by flush_context::flush_mutex_)
  // ...........................................................................

  while (!segment_.ctx()) { // no segment (lazy initialized)
    segment_ = writer_.get_segment_context(*ctx);
    segment_use_count_ = segment_.ctx().use_count();

    // must unlock/relock flush_context before retrying to get a new segment so
    // as to avoid a deadlock due to a read-write-read situation for
    // flush_context::flush_mutex_ with threads trying to lock
    // flush_context::flush_mutex_ to return their segment_context
    if (!segment_.ctx()) {
      ctx.reset(); // reset before reaquiring
      ctx = writer_.get_flush_context();
    }
  }

  assert(segment_.ctx());
  assert(segment_.ctx()->writer_);
  auto& segment = *(segment_.ctx());
  auto& writer = *segment.writer_;

  if (writer.initialized()) {
    auto segment_docs_max = writer_.segment_limits_.segment_docs_max.load();
    auto segment_memory_max = writer_.segment_limits_.segment_memory_max.load();

    // if not reached the limit of the current segment then use it
    if ((!segment_docs_max || segment_docs_max > writer.docs_cached()) // too many docs
        && (!segment_memory_max || segment_memory_max > writer.memory_active()) // too much memory
        && !doc_limits::eof(writer.docs_cached())) { // segment full
      return ctx;
    }

    // force a flush of a full segment
    IR_FRMT_TRACE(
      "Flushing segment '%s', docs=" IR_SIZE_T_SPECIFIER ", memory=" IR_SIZE_T_SPECIFIER ", docs limit=" IR_SIZE_T_SPECIFIER ", memory limit=" IR_SIZE_T_SPECIFIER "",
      writer.name().c_str(), writer.docs_cached(), writer.memory_active(), segment_docs_max, segment_memory_max
    );

    try {
      segment.flush();
    } catch (...) {
      IR_FRMT_ERROR(
        "while flushing segment '%s', error: failed to flush segment",
        segment.writer_meta_.meta.name.c_str()
      );

      segment.reset();

      throw;
    }
  }

  segment.prepare();
  assert(segment.writer_->initialized());

  return ctx;
}

void index_writer::flush_context::emplace(active_segment_context&& segment) {
  if (!segment.ctx_) {
    return; // nothing to do
  }

  assert( // failure may indicate a dangling 'document' instance
    (!segment.flush_ctx_ && segment.ctx_.use_count() == 1) // +1 for 'active_segment_context::ctx_'
    ||(this == segment.flush_ctx_ && segment.ctx_->dirty_ && segment.ctx_.use_count() == 1) // +1 for 'active_segment_context::ctx_' (flush_context switching made a full-circle)
    ||(this == segment.flush_ctx_ && !segment.ctx_->dirty_ && segment.ctx_.use_count() == 2) // +1 for 'active_segment_context::ctx_', +1 for 'pending_segment_context::segment_'
    ||(this != segment.flush_ctx_ && segment.flush_ctx_ && segment.ctx_.use_count() == 2) // +1 for 'active_segment_context::ctx_', +1 for 'pending_segment_context::segment_'
    ||(this != segment.flush_ctx_ && segment.flush_ctx_ && segment.ctx_.use_count() == 1) // +1 for 'active_segment_context::ctx_', +0 for 'pending_segment_context::segment_' that was already cleared
  );

  auto& ctx = *(segment.ctx_);
  freelist_t::node_type* freelist_node = nullptr;
  size_t generation_base;
  size_t modification_count;

  // prevent concurrent flush related modifications,
  // i.e. if segment is also owned by another flush_context
  auto flush_lock = make_unique_lock(ctx.flush_mutex_, std::defer_lock);

  {
    // pending_segment_contexts_ may be asynchronously read
    auto lock = make_lock_guard(mutex_);

    // update pending_segment_context
    // this segment_context has not yet been seen by this flush_context
    // or was marked dirty imples flush_context switching making a full-circle
    if (this != segment.flush_ctx_ || ctx.dirty_) {
      pending_segment_contexts_.emplace_back(
        segment.ctx_, pending_segment_contexts_.size()
      );
      freelist_node = &(pending_segment_contexts_.back());

      // mark segment as non-reusable if it was peviously registered with a different flush_context
      // NOTE: 'ctx.dirty_' implies flush_context switching making a full-circle
      //       and this emplace(...) call being the first and only call for this
      //       segment (not given out again via free-list) so no 'dirty_' check
      if (segment.flush_ctx_ && this != segment.flush_ctx_) {
        ctx.dirty_ = true;
        flush_lock.lock(); // 'segment.flush_ctx_' may be asynchronously flushed
        assert(segment.flush_ctx_->pending_segment_contexts_[segment.pending_segment_context_offset_].segment_ == segment.ctx_); // thread-safe because pending_segment_contexts_ is a deque
        // ^^^ FIXME TODO remove last line
        /* FIXME TODO uncomment once col_writer tail is writen correctly (need to track tail in new segment
        // if this segment is still referenced by the previous flush_context then
        // store 'pending_segment_contexts_' and 'uncomitted_modification_queries_'
        // in the previous flush_context because they will be modified lower down
        if (segment.ctx_.use_count() != 2) {
          assert(segment.flush_ctx_->pending_segment_contexts_.size() > segment.pending_segment_context_offset_);
          assert(segment.flush_ctx_->pending_segment_contexts_[segment.pending_segment_context_offset_].segment_ == segment.ctx_); // thread-safe because pending_segment_contexts_ is a deque
          assert(segment.flush_ctx_->pending_segment_contexts_[segment.pending_segment_context_offset_].segment_.use_count() == 3); // +1 for the reference in 'pending_segment_contexts_', +1 for the reference in other flush_context 'pending_segment_contexts_', +1 for the reference in 'active_segment_context'
          segment.flush_ctx_->pending_segment_contexts_[segment.pending_segment_context_offset_].doc_id_end_ = ctx.uncomitted_doc_id_begin_;
          segment.flush_ctx_->pending_segment_contexts_[segment.pending_segment_context_offset_].modification_offset_end_ = ctx.uncomitted_modification_queries_;
        }
        */
      }

      if (segment.flush_ctx_ && this != segment.flush_ctx_) { pending_segment_contexts_.pop_back(); freelist_node = nullptr; } // FIXME TODO remove this condition once col_writer tail is writen correctly
    } else { // the segment is present in this flush_context 'pending_segment_contexts_'
      assert(pending_segment_contexts_.size() > segment.pending_segment_context_offset_);
      assert(pending_segment_contexts_[segment.pending_segment_context_offset_].segment_ == segment.ctx_);
      assert(pending_segment_contexts_[segment.pending_segment_context_offset_].segment_.use_count() == 2); // +1 for the reference in 'pending_segment_contexts_', +1 for the reference in 'active_segment_context'
      freelist_node = &(pending_segment_contexts_[segment.pending_segment_context_offset_]);
    }

    // NOTE: if the first uncommitted operation is a removal operation then it
    //       is fully valid for its 'committed' generation value to equal the
    //       generation of the last 'committed' insert operation since removals
    //       are applied to documents with generation <= removal
    assert(ctx.uncomitted_modification_queries_ <= ctx.modification_queries_.size());
    modification_count =
      ctx.modification_queries_.size() - ctx.uncomitted_modification_queries_;
    if (segment.flush_ctx_ && this != segment.flush_ctx_) generation_base = segment.flush_ctx_->generation_ += modification_count; else  // FIXME TODO remove this condition once col_writer tail is writen correctly
    generation_base = generation_ += modification_count; // atomic increment to end of unique generation range
    generation_base -= modification_count; // start of generation range
  }

  // ...........................................................................
  // noexcept state update operations below here
  // no need for segment lock since flush_all() operates on values < '*_end_'
  // ...........................................................................

  // ...........................................................................
  // update generation of segment operation
  // ...........................................................................

  // update generations of modification_queries_
  for (auto i = ctx.uncomitted_modification_queries_,
       count = ctx.modification_queries_.size();
       i < count;
       ++i) {
    assert(ctx.modification_queries_[i].generation < modification_count); // must be < modification_count since inserts come after modification
    const_cast<size_t&>(ctx.modification_queries_[i].generation) += generation_base; // update to flush_context generation
  }

  auto uncomitted_doc_id_begin = ctx.uncomitted_doc_id_begin_;

  // update generations of segment_context::flushed_update_contexts_
  for (auto i = uncomitted_doc_id_begin - doc_limits::min(),
       end = ctx.flushed_update_contexts_.size();
       i < end;
       ++i, ++uncomitted_doc_id_begin) {
    assert(ctx.flushed_update_contexts_[i].generation <= modification_count); // can == modification_count if inserts come after modification
    ctx.flushed_update_contexts_[i].generation += generation_base; // update to flush_context generation
  }

  assert(ctx.writer_);
  assert(integer_traits<doc_id_t>::const_max >= ctx.writer_->docs_cached());
  auto& writer = *(ctx.writer_);
  auto writer_docs = writer.initialized() ? writer.docs_cached() : 0;

  assert(uncomitted_doc_id_begin - doc_limits::min() >= ctx.flushed_update_contexts_.size()); // update_contexts located inside th writer
  assert(uncomitted_doc_id_begin - doc_limits::min() <= ctx.flushed_update_contexts_.size() + writer_docs);

  // update generations of segment_writer::doc_contexts
  for (auto doc_id = uncomitted_doc_id_begin - ctx.flushed_update_contexts_.size(),
       doc_id_end = writer_docs + doc_limits::min();
       doc_id < doc_id_end;
       ++doc_id) {
    assert(doc_id <= integer_traits<doc_id_t>::const_max);
    assert(writer.doc_context(doc_id).generation <= modification_count); // can == modification_count if inserts come after modification
    writer.doc_context(doc_id_t(doc_id)).generation += generation_base; // update to flush_context generation
  }

  // ...........................................................................
  // reset counters for segment reuse
  // ...........................................................................

  ctx.uncomitted_generation_offset_ = 0;
  ctx.uncomitted_doc_id_begin_ =
    ctx.flushed_update_contexts_.size() + writer_docs + doc_limits::min();
  ctx.uncomitted_modification_queries_ = ctx.modification_queries_.size();
  if (!freelist_node) return; // FIXME TODO remove this condition once col_writer tail is writen correctly

  // do not reuse segments that are present in another flush_context
  if (!ctx.dirty_) {
    assert(freelist_node);
    assert(segment.ctx_.use_count() == 2); // +1 for 'active_segment_context::ctx_', +1 for 'pending_segment_context::segment_'
    auto& segments_active = *(segment.segments_active_);
    auto segments_active_decrement =
      irs::make_finally([&segments_active]()->void { --segments_active; }); // release hold (delcare before aquisition since operator++() is noexcept)

    ++segments_active; // increment counter to hold reservation while segment_context is being released and added to the freelist
    segment = active_segment_context(); // reset before adding to freelist to garantee proper use_count() in get_segment_context(...)
    pending_segment_contexts_freelist_.push(*freelist_node); // add segment_context to free-list
  }
}

void index_writer::flush_context::reset() noexcept {
  // reset before returning to pool
  for (auto& entry: pending_segment_contexts_) {
    if (entry.segment_.use_count() == 1) {
      entry.segment_->reset(); // reset only if segment not tracked anywhere else
    }
  }

  while(pending_segment_contexts_freelist_.pop()); // clear() before pending_segment_contexts_

  generation_.store(0);
  dir_->clear_refs();
  pending_segments_.clear();
  pending_segment_contexts_.clear();
  segment_mask_.clear();
}

index_writer::segment_context::segment_context(
    directory& dir,
    segment_meta_generator_t&& meta_generator,
    const column_info_provider_t& column_info,
    const comparer* comparator)
  : active_count_(0),
    buffered_docs_(0),
    dirty_(false),
    dir_(dir),
    meta_generator_(std::move(meta_generator)),
    uncomitted_doc_id_begin_(doc_limits::min()),
    uncomitted_generation_offset_(0),
    uncomitted_modification_queries_(0),
    writer_(segment_writer::make(dir_, column_info, comparator)) {
  assert(meta_generator_);
}

uint64_t index_writer::segment_context::flush() {
  // prevent concurrent flush related modifications
  auto lock = make_lock_guard(flush_mutex_);

  if (!writer_ || !writer_->initialized() || !writer_->docs_cached()) {
    return 0; // skip flushing an empty writer
  }

  auto flushed_docs_count = flushed_update_contexts_.size();

  assert(integer_traits<doc_id_t>::const_max >= writer_->docs_cached());
  flushed_update_contexts_.reserve(flushed_update_contexts_.size() + writer_->docs_cached());
  flushed_.emplace_back(std::move(writer_meta_.meta));

  // copy over update_contexts
  for (size_t doc_id = doc_limits::min(),
       doc_id_end = writer_->docs_cached() + doc_limits::min();
       doc_id < doc_id_end;
       ++doc_id) {
    assert(doc_id <= integer_traits<doc_id_t>::const_max);
    flushed_update_contexts_.emplace_back(writer_->doc_context(doc_id_t(doc_id)));
  }

  auto& segment = flushed_.back();

  // flush segment_writer
  try {
    writer_->flush(segment);
  } catch (...) {
    // failed to flush segment
    flushed_.pop_back();
    flushed_update_contexts_.resize(flushed_docs_count);

    throw;
  }

  auto const tick = writer_->tick();
  writer_->reset(); // mark segment as already flushed
  return tick;
}

index_writer::segment_context::ptr index_writer::segment_context::make(
    directory& dir,
    segment_meta_generator_t&& meta_generator,
    const column_info_provider_t& column_info,
    const comparer* comparator) {
  return memory::make_shared<segment_context>(dir, std::move(meta_generator), column_info, comparator);
}

segment_writer::update_context index_writer::segment_context::make_update_context() {
  return segment_writer::update_context {
    uncomitted_generation_offset_, // current modification generation
    NON_UPDATE_RECORD
  };
}

segment_writer::update_context index_writer::segment_context::make_update_context(
    const filter& filter) {
  auto generation = ++uncomitted_generation_offset_; // increment generation due to removal
  auto update_id = modification_queries_.size();

  modification_queries_.emplace_back(filter, generation - 1, true); // -1 for previous generation

  return segment_writer::update_context {
    generation, // current modification generation
    update_id // entry in modification_queries_
  };
}

segment_writer::update_context index_writer::segment_context::make_update_context(
    const std::shared_ptr<filter>& filter) {
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
    filter::ptr&& filter) {
  assert(filter);
  auto generation = ++uncomitted_generation_offset_; // increment generation due to removal
  auto update_id = modification_queries_.size();

  modification_queries_.emplace_back(std::move(filter), generation - 1, true); // -1 for previous generation

  return segment_writer::update_context {
    generation, // current modification generation
    update_id // entry in modification_queries_
  };
}

void index_writer::segment_context::prepare() {
  assert(writer_);

  if (!writer_->initialized()) {
    writer_meta_ = meta_generator_();
    writer_->reset(writer_meta_.meta);
  }
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

void index_writer::segment_context::reset() noexcept {
  active_count_.store(0);
  buffered_docs_.store(0);
  dirty_ = false;
  flushed_.clear();
  flushed_update_contexts_.clear();
  modification_queries_.clear();
  uncomitted_doc_id_begin_ = doc_limits::min();
  uncomitted_generation_offset_ = 0;
  uncomitted_modification_queries_ = 0;

  if (writer_->initialized()) {
    writer_->reset(); // try to reduce number of files flushed below
  }

  dir_.clear_refs(); // release refs only after clearing writer state to ensure 'writer_' does not hold any files
}

index_writer::index_writer(
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
    committed_state_t&& committed_state)
  : column_info_(column_info),
    meta_payload_provider_(meta_payload_provider),
    comparator_(comparator),
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
  assert(column_info); // ensured by 'make'
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

void index_writer::clear(uint64_t tick) {
  auto commit_lock = make_lock_guard(commit_lock_);

  if (!pending_state_
      && meta_.empty()
      && type_limits<type_t::index_gen_t>::valid(meta_.last_gen_)) {
    return; // already empty
  }

  auto ctx = get_flush_context(false);
  auto ctx_lock = make_lock_guard(ctx->mutex_); // ensure there are no active struct update operations

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
  pending_meta.payload_buf_.clear();
  if (meta_payload_provider_ && meta_payload_provider_(tick, pending_meta.payload_buf_)) {
    pending_meta.payload_ = pending_meta.payload_buf_;
  }
  pending_meta.seg_counter_.store(meta_.counter()); // ensure counter() >= max(seg#)

  // rollback already opened transaction if any
  writer_->rollback();

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
  auto lock = make_lock_guard(consolidation_lock_);
  consolidating_segments_.clear();
}

index_writer::ptr index_writer::make(
    directory& dir,
    format::ptr codec,
    OpenMode mode,
    const init_options& opts /*= init_options()*/) {
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
      file_refs.emplace_back(directory_utils::reference(dir, segments_file));
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
    segment_options(opts),
    opts.comparator,
    opts.column_info ? opts.column_info : DEFAULT_COLUMN_INFO,
    opts.meta_payload_provider,
    std::move(meta),
    std::move(comitted_state)
  );

  directory_utils::ensure_allocator(dir, opts.memory_pool_size); // ensure memory_allocator set in directory
  directory_utils::remove_all_unreferenced(dir); // remove non-index files from directory

  return writer;
}

index_writer::~index_writer() noexcept {
  assert(!segments_active_.load()); // failure may indicate a dangling 'document' instance
  cached_readers_.clear();
  write_lock_.reset(); // reset write lock if any
  pending_state_.reset(); // reset pending state (if any) before destroying flush contexts
  flush_context_ = nullptr;
  flush_context_pool_.clear(); // ensue all tracked segment_contexts are released before segment_writer_pool_ is deallocated
}

uint64_t index_writer::buffered_docs() const {
  uint64_t docs_in_ram = 0;
  auto ctx = const_cast<index_writer*>(this)->get_flush_context();
  // 'pending_used_segment_contexts_'/'pending_free_segment_contexts_' may be modified
  auto lock = make_lock_guard(ctx->mutex_);

  for (auto& entry: ctx->pending_segment_contexts_) {
    docs_in_ram += entry.segment_->buffered_docs_.load(); // reading segment_writer::docs_count() is not thread safe
  }

  return docs_in_ram;
}

index_writer::consolidation_result index_writer::consolidate(
    const consolidation_policy_t& policy,
    format::ptr codec /*= nullptr*/,
    const merge_writer::flush_progress_t& progress /*= {}*/) {
  REGISTER_TIMER_DETAILED();
  if (!codec) {
    // use default codec if not specified
    codec = codec_;
  }

  std::set<const segment_meta*> candidates;
  const auto run_id = reinterpret_cast<size_t>(&candidates);

  // hold a reference to the last committed state to prevent files from being
  // deleted by a cleaner during the upcoming consolidation
  // use atomic_load(...) since finish() may modify the pointer
  auto committed_state = committed_state_helper::atomic_load(&committed_state_);
  assert(committed_state);
  auto committed_meta = committed_state->first;
  assert(committed_meta);

  // collect a list of consolidation candidates
  {
    auto lock = make_lock_guard(consolidation_lock_);
    // FIXME TODO remove from 'consolidating_segments_' any segments in 'committed_state_' or 'pending_state_' to avoid data duplication
    policy(candidates, *committed_meta, consolidating_segments_);

    switch (candidates.size()) {
      case 0:
        // nothing to consolidate
        return { 0, ConsolidationError::OK };
      case 1: {
        const auto* segment = *candidates.begin();

        if (!segment) {
          // invalid candidate
          return { 0, ConsolidationError::FAIL };
        }

        if (segment->live_docs_count == segment->docs_count) {
          // no deletes, nothing to consolidate
          return { 0, ConsolidationError::OK };
        }
      }
    }

    // check that candidates are not involved in ongoing merges
    for (const auto* candidate : candidates) {
      // segment has been already chosen for consolidation (or at least was choosen), give up
      if (consolidating_segments_.end() != consolidating_segments_.find(candidate)) {
        return { 0, ConsolidationError::FAIL };
      }
    }
    try {
      // register for consolidation
      consolidating_segments_.insert(candidates.begin(), candidates.end());
    } catch(...) {
      // rollback in case of insertion fails (finalizer below won`t handle partial insert
      // as concurrent consolidation is free to select same candidate before finalizer
      // reacquires the consolidation_lock)
      for (const auto* candidate : candidates) {
        consolidating_segments_.erase(candidate);
      }
      throw;
    }
  }

  // unregisterer for all registered candidates
  auto unregister_segments = irs::make_finally([&candidates, this]() {
    if (candidates.empty()) {
      return;
    }
    auto lock = make_lock_guard(consolidation_lock_);
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
      IR_FRMT_DEBUG(
        "Failed to start consolidation for index generation '" IR_UINT64_T_SPECIFIER "', "
        "found only '" IR_SIZE_T_SPECIFIER "' out of '" IR_SIZE_T_SPECIFIER "' candidates",
        committed_meta->generation(),
        found,
        candidates.size());
      return { 0, ConsolidationError::FAIL };
    }
  }

  IR_FRMT_TRACE(
    "Starting consolidation id='" IR_SIZE_T_SPECIFIER "':\n%s",
    run_id,
    ::to_string(candidates).c_str());

  // do lock-free merge

  consolidation_result result { candidates.size(),  ConsolidationError::FAIL };

  index_meta::index_segment_t consolidation_segment;
  consolidation_segment.meta.codec = codec_; // should use new codec
  consolidation_segment.meta.version = 0; // reset version for new segment
  consolidation_segment.meta.name = file_name(meta_.increment()); // increment active meta, not fn arg

  ref_tracking_directory dir(dir_); // track references for new segment
  merge_writer merger(dir, column_info_, comparator_);
  merger.reserve(result.size);

  // add consolidated segments to the merge_writer
  for (const auto* segment : candidates) {
    // already checked validity
    assert(segment);

    auto reader = cached_readers_.emplace(*segment);

    if (reader) {
      // merge_writer holds a reference to reader
      merger.add(static_cast<sub_reader::ptr>(reader));
    }
  }

  // we do not persist segment meta since some removals may come later
  if (!merger.flush(consolidation_segment, progress)) {
    // nothing to consolidate or consolidation failure
    return result;
  }

  // commit merge
  {
    // ensure committed_state_ segments are not modified by concurrent consolidate()/commit()
    auto lock = make_unique_lock(commit_lock_);
    const auto current_committed_meta = committed_state_->first;
    assert(current_committed_meta);

    auto cleanup_cached_readers = [&current_committed_meta, &candidates, this]() {
        if (!candidates.empty()) {
          decltype(flush_context::segment_mask_) cached_mask;
          // pointers are different so check by name
          for (const auto& candidate : candidates) {
            if (current_committed_meta->end() ==
              std::find_if(current_committed_meta->begin(),
                current_committed_meta->end(),
                [&candidate](const index_meta::index_segment_t& s) {
                  return candidate->name == s.meta.name; })) {
              // found missing segment. Mask it!
              cached_mask.insert(*candidate);
            }
          }
          if (!cached_mask.empty()) {
            cached_readers_.purge(cached_mask);
          }
        }
      };

    if (pending_state_) {
      // we could possibly need cleanup
      auto unregister_missing_cached_readers = irs::make_finally(std::move(cleanup_cached_readers));

      // check we didn`t added to reader cache already absent readers
      // only if we have different index meta
      if (committed_meta != current_committed_meta) {
        // pointers are different so check by name
        for (const auto& candidate : candidates) {
          if (current_committed_meta->end() ==
              std::find_if(current_committed_meta->begin(),
                           current_committed_meta->end(),
                           [&candidate](const index_meta::index_segment_t& s) {
                             return candidate->name == s.meta.name;} )) {
            // not all candidates are valid
            IR_FRMT_DEBUG(
              "Failed to start consolidation for index generation '" IR_UINT64_T_SPECIFIER
              "', not found segment %s in committed state",
              committed_meta->generation(),
              candidate->name.c_str());
            return result;
          }
        }
      }

      result.error = ConsolidationError::PENDING;

      // transaction has been started, we're somewhere in the middle
      auto ctx = get_flush_context(); // can modify ctx->segment_mask_ without lock since have commit_lock_

      // register consolidation for the next transaction
      ctx->pending_segments_.emplace_back(
        std::move(consolidation_segment),
        integer_traits<size_t>::max(), // skip deletes, will accumulate deletes from existing candidates
        extract_refs(dir), // do not forget to track refs
        std::move(candidates), // consolidation context candidates
        std::move(committed_meta), // consolidation context meta
        std::move(merger)); // merge context

      IR_FRMT_TRACE(
        "Consolidation id='" IR_SIZE_T_SPECIFIER "' successfully finished: pending",
        run_id);
    } else if (committed_meta == current_committed_meta) {
      // before new transaction was started:
      // no commits happened in since consolidation was started

      auto ctx = get_flush_context();
      auto ctx_lock = make_lock_guard(ctx->mutex_); // lock due to context modification

      lock.unlock(); // can release commit lock, we guarded against commit by locked flush context

      auto& segment_mask = ctx->segment_mask_;

      index_utils::flush_index_segment(dir, consolidation_segment); // persist segment meta
      segment_mask.reserve(segment_mask.size() + candidates.size());
      ctx->pending_segments_.emplace_back(
        std::move(consolidation_segment),
        0, // deletes must be applied to the consolidated segment
        extract_refs(dir), // do not forget to track refs
        std::move(candidates), // consolidation context candidates
        std::move(committed_meta) // consolidation context meta
      );

      // filter out merged segments for the next commit
      const auto& pending_segment = ctx->pending_segments_.back();
      const auto& consolidation_ctx = pending_segment.consolidation_ctx;
      const auto& consolidation_meta = pending_segment.segment.meta;

      // mask mapped candidates
      // segments from the to-be added new segment
      for (const auto* segment : consolidation_ctx.candidates) {
        segment_mask.emplace(*segment);
      }

      IR_FRMT_TRACE(
        "Consolidation id='" IR_SIZE_T_SPECIFIER "' successfully finished: "
        "Name='%s', docs_count=" IR_UINT64_T_SPECIFIER ", "
        "live_docs_count=" IR_UINT64_T_SPECIFIER ", "
        "size=" IR_SIZE_T_SPECIFIER "",
        run_id,
        consolidation_meta.name.c_str(),
        consolidation_meta.docs_count,
        consolidation_meta.live_docs_count,
        consolidation_meta.size);
    } else {
      // before new transaction was started:
      // there was a commit(s) since consolidation was started,

      // we could possibly need cleanup
      auto unregister_missing_cached_readers = irs::make_finally(std::move(cleanup_cached_readers));

      auto ctx = get_flush_context();
      auto ctx_lock = make_lock_guard(ctx->mutex_); // lock due to context modification

      lock.unlock(); // can release commit lock, we guarded against commit by locked flush context

      auto& segment_mask = ctx->segment_mask_;

      candidates_mapping_t mappings;
      const auto res = map_candidates(mappings, candidates, current_committed_meta->segments());

      if (res.second != candidates.size()) {
        // at least one candidate is missing
        // can't finish consolidation
        IR_FRMT_DEBUG(
          "Failed to finish consolidation id='" IR_SIZE_T_SPECIFIER "' for segment '%s', "
          "found only '" IR_SIZE_T_SPECIFIER "' out of '" IR_SIZE_T_SPECIFIER "' candidates",
          run_id,
          consolidation_segment.meta.name.c_str(),
          res.second,
          candidates.size());

        return result;
      }

      // handle deletes if something changed
      if (res.first) {
        irs::document_mask docs_mask;

        if (!map_removals(mappings, merger, cached_readers_, docs_mask)) {
          // consolidated segment has docs missing from current_committed_meta->segments()
          IR_FRMT_DEBUG(
            "Failed to finish consolidation id='" IR_SIZE_T_SPECIFIER "' for segment '%s', "
            "due removed documents still present the consolidation candidates",
            run_id,
            consolidation_segment.meta.name.c_str());

          return result;
        }

        if (!docs_mask.empty()) {
          consolidation_segment.meta.live_docs_count -= docs_mask.size();
          write_document_mask(dir, consolidation_segment.meta, docs_mask, false);
        }
      }

      index_utils::flush_index_segment(dir, consolidation_segment);// persist segment meta
      segment_mask.reserve(segment_mask.size() + candidates.size());
      ctx->pending_segments_.emplace_back(
        std::move(consolidation_segment),
        0, // deletes must be applied to the consolidated segment
        extract_refs(dir), // do not forget to track refs
        std::move(candidates), // consolidation context candidates
        std::move(committed_meta) // consolidation context meta
      );

      // filter out merged segments for the next commit
      const auto& pending_segment = ctx->pending_segments_.back();
      const auto& consolidation_ctx = pending_segment.consolidation_ctx;
      const auto& consolidation_meta = pending_segment.segment.meta;

      // mask mapped candidates
      // segments from the to-be added new segment
      for (const auto* segment : consolidation_ctx.candidates) {
        segment_mask.emplace(*segment);
      }

      // mask mapped (matched) segments
      // segments from the already finished commit
      for (auto& segment: current_committed_meta->segments()) {
        if (mappings.end() != mappings.find(segment.meta.name)) {
          segment_mask.emplace(segment.meta);
        }
      }

      IR_FRMT_TRACE(
        "Consolidation id='" IR_SIZE_T_SPECIFIER "' successfully finished:\nName='%s', "
        "docs_count=" IR_UINT64_T_SPECIFIER ", "
        "live_docs_count=" IR_UINT64_T_SPECIFIER ", "
        "size=" IR_SIZE_T_SPECIFIER "",
        run_id,
        consolidation_meta.name.c_str(),
        consolidation_meta.docs_count,
        consolidation_meta.live_docs_count,
        consolidation_meta.size);
    }
  }

  result.error = ConsolidationError::OK;
  return result;
}

bool index_writer::import(
    const index_reader& reader,
    format::ptr codec /*= nullptr*/,
    const merge_writer::flush_progress_t& progress /*= {}*/) {
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

  merge_writer merger(dir, column_info_, comparator_);
  merger.reserve(reader.size());

  for (auto& segment : reader) {
    merger.add(segment);
  }

  if (!merger.flush(segment, progress)) {
    return false; // import failure (no files created, nothing to clean up)
  }

  index_utils::flush_index_segment(dir, segment);

  auto refs = extract_refs(dir);

  auto ctx = get_flush_context();
  auto lock = make_lock_guard(ctx->mutex_); // lock due to context modification

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
      auto lock = make_unique_lock(mutex); // lock ctx exchange (write-lock)

      // aquire the current flush_context and its lock
      if (!flush_context_.compare_exchange_strong(ctx, ctx->next_context_)) {
        ctx = flush_context_.load(); // it might have changed
        continue;
      }

      lock.release();

      return {
        ctx,
        [](flush_context* ctx) noexcept ->void {
          async_utils::read_write_mutex::write_mutex mutex(ctx->flush_mutex_);
          auto lock = make_unique_lock(mutex, std::adopt_lock);

          ctx->reset(); // reset context and make ready for reuse
        }
      };
    }
  }

  for(;;) {
    async_utils::read_write_mutex::read_mutex mutex(ctx->flush_mutex_);
    auto lock = make_unique_lock(mutex, std::try_to_lock); // lock current ctx (read-lock)

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
      [](flush_context* ctx) noexcept ->void {
        async_utils::read_write_mutex::read_mutex mutex(ctx->flush_mutex_);
        auto lock = make_unique_lock(mutex, std::adopt_lock);
      }
    };
  }
}

index_writer::active_segment_context index_writer::get_segment_context(
    flush_context& ctx) {
  // release reservation (delcare before aquisition since operator++() is noexcept)
  auto segments_active_decrement = irs::make_finally([this]()->void { --segments_active_; });
  // increment counter to aquire reservation, if another thread
  // tries to reserve last context then it'll be over limit
  auto segments_active = ++segments_active_;
  auto segment_count_max = segment_limits_.segment_count_max.load();

  // no free segment_context available and maximum number of segments reached
  // must return to caller so as to unlock/relock flush_context before retrying
  // to get a new segment so as to avoid a deadlock due to a read-write-read
  // situation for flush_context::flush_mutex_ with threads trying to lock
  // flush_context::flush_mutex_ to return their segment_context
  if (segment_count_max && segment_count_max < segments_active) { // '<' to account for +1 reservation
    return active_segment_context();
  }

  auto* freelist_node = static_cast<flush_context::pending_segment_context*>(
    ctx.pending_segment_contexts_freelist_.pop()
  ); // only nodes of type 'pending_segment_context' are added to 'pending_segment_contexts_freelist_'

  if (freelist_node) {
    assert(freelist_node->segment_.use_count() == 1); // +1 for the reference in 'pending_segment_contexts_'
    assert(!freelist_node->segment_->dirty_);
    return active_segment_context(
      freelist_node->segment_, segments_active_, &ctx, freelist_node->value
    );
  }

  // ...........................................................................
  // should allocate a new segment_context from the pool
  // ...........................................................................

  auto meta_generator = [this]()->segment_meta {
    return segment_meta(file_name(meta_.increment()), codec_);
  };
  auto segment_ctx = segment_writer_pool_.emplace(
    dir_, std::move(meta_generator),
    column_info_, comparator_
  ).release();
  auto segment_memory_max = segment_limits_.segment_memory_max.load();

  // recreate writer if it reserved more memory than allowed by current limits
  if (segment_memory_max &&
      segment_memory_max < segment_ctx->writer_->memory_reserved()) {
    segment_ctx->writer_ = segment_writer::make(segment_ctx->dir_, column_info_, comparator_);
  }

  return active_segment_context(segment_ctx, segments_active_);
}

index_writer::pending_context_t index_writer::flush_all() {
  REGISTER_TIMER_DETAILED();
  using namespace std::chrono_literals;

  bool modified = !type_limits<type_t::index_gen_t>::valid(meta_.last_gen_);
  sync_context to_sync;
  document_mask docs_mask;

  auto pending_meta = memory::make_unique<index_meta>();
  auto& segments = pending_meta->segments_;

  auto ctx = get_flush_context(false);
  auto& dir = *(ctx->dir_);
  std::vector<std::unique_lock<decltype(segment_context::flush_mutex_)>> segment_flush_locks;
  auto lock = make_unique_lock(ctx->mutex_); // ensure there are no active struct update operations

  // register consolidating segments cleanup.
  // we need raw ptr as ctx may be moved
  auto unregister_segments = irs::make_finally([ctx_raw = ctx.get(), this]()
    {
      assert(ctx_raw);
      if (ctx_raw->pending_segments_.empty()) {
        return;
      }
      auto lock = make_lock_guard(consolidation_lock_);

      for (auto& pending_segment : ctx_raw->pending_segments_) {
        auto& candidates = pending_segment.consolidation_ctx.candidates;
        for (const auto* candidate : candidates) {
          consolidating_segments_.erase(candidate);
        }
      }
    });
  //////////////////////////////////////////////////////////////////////////////
  /// Stage 0
  /// wait for any outstanding segments to settle to ensure that any rollbacks
  /// are properly tracked in 'modification_queries_'
  //////////////////////////////////////////////////////////////////////////////

  uint64_t max_tick = 0;

  for (auto& entry : ctx->pending_segment_contexts_) {
    // mark the 'segment_context' as dirty so that it will not be reused if this
    // 'flush_context' once again becomes the active context while the
    // 'segment_context' handle is still held by documents()
    entry.segment_->dirty_ = true;

    // wait for the segment to no longer be active
    // i.e. wait for all ongoing document operations to finish (insert/replace)
    // the segment will not be given out again by the active 'flush_context'
    // because it was started by a different 'flush_context', i.e. by 'ctx'
    while (entry.segment_->active_count_.load()
           || entry.segment_.use_count() != 1) { // FIXME TODO remove this condition once col_writer tail is writen correctly
      ctx->pending_segment_context_cond_.wait_for(lock, 1000ms); // arbitrary sleep interval
    }

    // FIXME TODO flush_all() blocks flush_context::emplace(...) and insert()/remove()/replace()
    segment_flush_locks.emplace_back(entry.segment_->flush_mutex_); // prevent concurrent modification of segment_context properties during flush_context::emplace(...)

    // force a flush of the underlying segment_writer
    max_tick = std::max(entry.segment_->flush(), max_tick);

    entry.doc_id_end_ = // may be integer_traits<size_t>::const_max if segment_meta only in this flush_context
      std::min(entry.segment_->uncomitted_doc_id_begin_, entry.doc_id_end_); // update so that can use valid value below
    entry.modification_offset_end_ = std::min(
      entry.segment_->uncomitted_modification_queries_,
      entry.modification_offset_end_
    ); // update so that can use valid value below
  }

  /////////////////////////////////////////////////////////////////////////////
  /// Stage 1
  /// update document_mask for existing (i.e. sealed) segments
  /////////////////////////////////////////////////////////////////////////////

  auto& segment_mask = ctx->segment_mask_;

  for (auto& existing_segment : meta_) {
    // skip already masked segments
    if (segment_mask.end() != segment_mask.find(existing_segment.meta)) {
      continue;
    }

    const auto segment_id = segments.size();
    segments.emplace_back(existing_segment);

    auto mask_modified = false;
    auto& segment = segments.back();

    docs_mask.clear();
    index_utils::read_document_mask(docs_mask, dir, segment.meta);

    // mask documents matching filters from segment_contexts (i.e. from new operations)
    for (auto& modifications : ctx->pending_segment_contexts_) {
      // modification_queries_ range [flush_segment_context::modification_offset_begin_, segment_context::uncomitted_modification_queries_)
      auto modifications_begin = modifications.modification_offset_begin_;
      auto modifications_end = modifications.modification_offset_end_;

      assert(modifications_begin <= modifications_end);
      assert(modifications_end <= modifications.segment_->modification_queries_.size());
      modification_contexts_ref modification_queries(
        modifications.segment_->modification_queries_.data() + modifications_begin,
        modifications_end - modifications_begin
      );

      mask_modified |= add_document_mask_modified_records(
        modification_queries,
        docs_mask,
        cached_readers_, // reader cache for segments
        segment.meta
      );
    }

    // write docs_mask if masks added, if all docs are masked then mask segment
    if (mask_modified) {
      // mask empty segments
      if (!segment.meta.live_docs_count) {
        segment_mask.emplace(existing_segment.meta); // mask segment to clear reader cache
        segments.pop_back(); // remove empty segment
        modified = true; // removal of one of the existing segments
        continue;
      }

      segment_mask.emplace(existing_segment.meta); // mask segment since write_document_mask(...) will increment version
      to_sync.register_partial_sync(segment_id, write_document_mask(dir, segment.meta, docs_mask));
      segment.meta.size = 0; // reset for new write
      index_utils::flush_index_segment(dir, segment); // write with new mask
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
    docs_mask.clear();
    bool pending_consolidation = pending_segment.consolidation_ctx.merger;
    if (pending_consolidation) {
      // pending consolidation request
      candidates_mapping_t mappings;
      const auto res = map_candidates(mappings, candidates, segments);

      if (res.second != candidates.size()) {
        // at least one candidate is missing
        // in pending meta can't finish consolidation
        IR_FRMT_DEBUG(
          "Failed to finish merge for segment '%s', found only '" IR_SIZE_T_SPECIFIER "' out of '" IR_SIZE_T_SPECIFIER "' candidates",
          pending_segment.segment.meta.name.c_str(),
          res.second,
          candidates.size()
        );

        continue; // skip this particular consolidation
      }

      // mask mapped candidates
      // segments from the to-be added new segment
      for (auto& mapping : mappings) {
        ctx->segment_mask_.emplace(*(mapping.second.second.first));
      }

      // mask mapped (matched) segments
      // segments from the currently ongoing commit
      for (auto& segment: segments) {
        if (mappings.end() != mappings.find(segment.meta.name)) {
          ctx->segment_mask_.emplace(segment.meta);
        }
      }

      // have some changes, apply deletes
      if (res.first) {
        auto success = map_removals(
          mappings,
          pending_segment.consolidation_ctx.merger,
          cached_readers_,
          docs_mask
        );

        if (!success) {
          // consolidated segment has docs missing from 'segments'
          IR_FRMT_WARN(
            "Failed to finish merge for segment '%s', due removed documents still present the consolidation candidates",
            pending_segment.segment.meta.name.c_str()
          );

          continue; // skip this particular consolidation
        }
      }

      // we're done with removals for pending consolidation
      // they have been already applied to candidates above
      // and succesfully remapped to consolidated segment
      pending_segment.segment.meta.live_docs_count -= docs_mask.size();

      // we've seen at least 1 successfully applied
      // pending consolidation request
      pending_candidates_count += candidates.size();
    } else {
      // during consolidation doc_mask could be already populated even for just merged segment
      if (pending_segment.segment.meta.docs_count != pending_segment.segment.meta.live_docs_count) {
        index_utils::read_document_mask(docs_mask, dir, pending_segment.segment.meta);
      }
      bool docs_mask_modified = false;
      // pending already imported/consolidated segment, apply deletes
      // mask documents matching filters from segment_contexts (i.e. from new operations)
      for (auto& modifications: ctx->pending_segment_contexts_) {
        // modification_queries_ range [flush_segment_context::modification_offset_begin_, segment_context::uncomitted_modification_queries_)
        auto modifications_begin = modifications.modification_offset_begin_;
        auto modifications_end = modifications.modification_offset_end_;

        assert(modifications_begin <= modifications_end);
        assert(modifications_end <= modifications.segment_->modification_queries_.size());
        modification_contexts_ref modification_queries(
          modifications.segment_->modification_queries_.data() + modifications_begin,
          modifications_end - modifications_begin
        );

        docs_mask_modified |= add_document_mask_modified_records(
          modification_queries,
          docs_mask,
          cached_readers_, // reader cache for segments
          pending_segment.segment.meta,
          pending_segment.generation
        );
      }

      // if mask left untouched, reset it, to prevent unnecessary writes
      if (!docs_mask_modified) {
        docs_mask.clear();
      }
    }

    // skip empty segments
    if (!pending_segment.segment.meta.live_docs_count) {
      ctx->segment_mask_.emplace(pending_segment.segment.meta);
      continue;
    }

    // write non-empty document mask
    if (!docs_mask.empty()) {
      if (!pending_consolidation) {
        // if this is pending consolidation,
        // this segment is already in the mask (see assert below)
        // new version will be created. Remove old version from cache!
        ctx->segment_mask_.emplace(pending_segment.segment.meta);
      }
      write_document_mask(dir, pending_segment.segment.meta, docs_mask, !pending_consolidation);
      pending_consolidation = true; // force write new segment meta
    }

    // persist segment meta
    if (pending_consolidation) {
      index_utils::flush_index_segment(dir, pending_segment.segment);
    }

    // register full segment sync
    to_sync.register_full_sync(segments.size());
    segments.emplace_back(std::move(pending_segment.segment));
  }

  if (pending_candidates_count) {
    // for pending consolidation we need to filter out
    // consolidation candidates after applying them
    index_meta::index_segments_t tmp;
    decltype(sync_context::segments) tmp_sync;

    tmp.reserve(segments.size() - pending_candidates_count);
    tmp_sync.reserve(to_sync.segments.size());

    auto begin = to_sync.segments.begin();
    auto end = to_sync.segments.end();

    for (size_t i = 0, size = segments.size(); i < size; ++i) {
      auto& segment = segments[i];

      // valid segment
      const bool valid = ctx->segment_mask_.end() == ctx->segment_mask_.find(segment.meta);

      if (begin != end && i == begin->first) {
        if (valid) {
          tmp_sync.emplace_back(tmp.size(), begin->second);
        }

        ++begin;
      }

      if (valid) {
        tmp.emplace_back(std::move(segment));
      }
    }

    segments = std::move(tmp);
    to_sync.segments = std::move(tmp_sync);
  }

  /////////////////////////////////////////////////////////////////////////////
  /// Stage 3
  /// create new segments
  /////////////////////////////////////////////////////////////////////////////

  {
    std::vector<flush_segment_context> segment_ctxs;

    // proces all segments that have been seen by the current flush_context
    for (auto& pending_segment_context: ctx->pending_segment_contexts_) {
      if (!pending_segment_context.segment_) {
        continue; // skip empty segments
      }

      size_t flushed_docs_count = 0;
      auto flushed_doc_id_end = pending_segment_context.doc_id_end_; // was updated after flush
      assert(pending_segment_context.doc_id_begin_ <= flushed_doc_id_end);
      assert(flushed_doc_id_end - doc_limits::min() <= pending_segment_context.segment_->flushed_update_contexts_.size());

      // process individually each flushed segment_meta from the segment_context
      for (auto& flushed: pending_segment_context.segment_->flushed_) {
        auto flushed_docs_start = flushed_docs_count;

        flushed_docs_count += flushed.meta.docs_count; // sum of all previous segment_meta::docs_count including this meta

        if (!flushed.meta.live_docs_count // empty segment_meta
            || flushed_doc_id_end - doc_limits::min() <= flushed_docs_start // segment_meta fully before the start of this flush_context
            || pending_segment_context.doc_id_begin_ - doc_limits::min() >= flushed_docs_count) { // segment_meta fully after the start of this flush_context
          continue;
        }

        auto update_contexts_begin = std::max( // 0-based
          pending_segment_context.doc_id_begin_ - doc_limits::min(),
          flushed_docs_start
        );
        auto update_contexts_end = std::min( // 0-based
          flushed_doc_id_end - doc_limits::min(),
          flushed_docs_count
        );
        assert(update_contexts_begin <= update_contexts_end);
        auto valid_doc_id_begin =
          update_contexts_begin - flushed_docs_start + doc_limits::min(); // begining doc_id in this segment_meta
        auto valid_doc_id_end = std::min(
          update_contexts_end - flushed_docs_start + doc_limits::min(),
          size_t(flushed.docs_mask_tail_doc_id)
        );
        assert(valid_doc_id_begin <= valid_doc_id_end);

        if (valid_doc_id_begin == valid_doc_id_end) {
          continue; // empty segment since head+tail == 'docs_count'
        }

        modification_contexts_ref segment_modification_contexts(
          pending_segment_context.segment_->modification_queries_.data(),
          pending_segment_context.segment_->modification_queries_.size()
        );
        update_contexts_ref flush_update_contexts(
          pending_segment_context.segment_->flushed_update_contexts_.data() + flushed_docs_start,
          flushed.meta.docs_count
        );

        segment_ctxs.emplace_back(
          flushed,
          valid_doc_id_begin,
          valid_doc_id_end,
          flush_update_contexts,
          segment_modification_contexts
        );
        ++flushed.meta.version; // increment version for next run due to documents masked from this run, similar to write_document_mask(...)

        auto& flush_segment_ctx = segment_ctxs.back();

        // read document_mask as was originally flushed
        // could be due to truncated records due to rollback of uncommitted data
        index_utils::read_document_mask(
          flush_segment_ctx.docs_mask_,
          pending_segment_context.segment_->dir_,
          flush_segment_ctx.segment_.meta
        );

        // add doc_ids before start of this flush_context to document_mask
        for (size_t doc_id = doc_limits::min();
             doc_id < valid_doc_id_begin;
             ++doc_id) {
          assert(integer_traits<doc_id_t>::const_max >= doc_id);
          if (flush_segment_ctx.docs_mask_.emplace(doc_id_t(doc_id)).second) {
            assert(flush_segment_ctx.segment_.meta.live_docs_count);
            --flush_segment_ctx.segment_.meta.live_docs_count; // decrement count of live docs
          }
        }

        // add tail doc_ids not part of this flush_context to documents_mask (including truncated)
        for (size_t doc_id = valid_doc_id_end,
             doc_id_end = flushed.meta.docs_count + doc_limits::min();
             doc_id < doc_id_end;
             ++doc_id) {
          assert(integer_traits<doc_id_t>::const_max >= doc_id);
          if (flush_segment_ctx.docs_mask_.emplace(doc_id_t(doc_id)).second) {
            assert(flush_segment_ctx.segment_.meta.live_docs_count);
            --flush_segment_ctx.segment_.meta.live_docs_count; // decrement count of live docs
          }
        }

        bool segment_modified{ false };
        // mask documents matching filters from all flushed segment_contexts (i.e. from new operations)
        for (auto& modifications: ctx->pending_segment_contexts_) {
          auto modifications_begin = modifications.modification_offset_begin_;
          auto modifications_end = modifications.modification_offset_end_;

          assert(modifications_begin <= modifications_end);
          assert(modifications_end <= modifications.segment_->modification_queries_.size());
          modification_contexts_ref modification_queries(
            modifications.segment_->modification_queries_.data() + modifications_begin,
            modifications_end - modifications_begin
          );

          segment_modified |= add_document_mask_modified_records(
            modification_queries, flush_segment_ctx, cached_readers_);
        }
        if (segment_modified) {
          ctx->segment_mask_.emplace(flush_segment_ctx.segment_.meta);
        }
      }
    }

    // write docs_mask if !empty(), if all docs are masked then remove segment altogether
    for (auto& segment_ctx: segment_ctxs) {
      // if have a writer with potential update-replacement records then check if they were seen
      add_document_mask_unused_updates(segment_ctx);

      // after mismatched replaces here could be also empty segment
      // so masking is needed
      if (!segment_ctx.segment_.meta.live_docs_count) {
        ctx->segment_mask_.emplace(segment_ctx.segment_.meta);
        continue;
      }
      // write non-empty document mask
      if (!segment_ctx.docs_mask_.empty()) {
        write_document_mask(
          dir, segment_ctx.segment_.meta, segment_ctx.docs_mask_
        );
        index_utils::flush_index_segment(dir, segment_ctx.segment_); // write with new mask
      }

      // register full segment sync
      to_sync.register_full_sync(segments.size());
      segments.emplace_back(std::move(segment_ctx.segment_));
    }
  }

  pending_meta->update_generation(meta_); // clone index metadata generation

  modified |= !to_sync.empty();

  // only flush a new index version upon a new index or a metadata change
  if (!modified) {
    // even if nothing to commit, we may have populated readers cache! Need to cleanup.
    if (!ctx->segment_mask_.empty()) {
      cached_readers_.purge(ctx->segment_mask_);
    }
    return pending_context_t();
  }

  pending_meta->payload_buf_.clear();
  if (meta_payload_provider_ && meta_payload_provider_(max_tick, pending_meta->payload_buf_)) {
    pending_meta->payload_ = pending_meta->payload_buf_;
  }

  pending_meta->seg_counter_.store(meta_.counter()); // ensure counter() >= max(seg#)

  pending_context_t pending_context;
  pending_context.ctx = std::move(ctx); // retain flush context reference
  pending_context.meta = std::move(pending_meta); // retain meta pending flush
  pending_context.to_sync = std::move(to_sync);

  return pending_context;
}

bool index_writer::start() {
  assert(!commit_lock_.try_lock()); // already locked

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

  auto update_generation = make_finally([this, &pending_meta]()noexcept{
    meta_.update_generation(pending_meta);
  });

  auto sync = [&dir](const std::string& file) {
    if (!dir.sync(file)) {
      throw io_error(string_utils::to_string(
        "failed to sync file, path: %s",
        file.c_str()
      ));
    }

    return true;
  };

  try {
    // sync all pending files
    to_commit.to_sync.visit(sync, pending_meta);

    // track all refs
    file_refs_t pending_refs;
    append_segments_refs(pending_refs, dir, pending_meta);
    pending_refs.emplace_back(
      directory_utils::reference(dir, writer_->filename(pending_meta), true)
    );

    meta_.segments_ = to_commit.meta->segments_; // create copy

    // 1st phase of the transaction successfully finished here,
    // set to_commit as active flush context containing pending meta
    pending_state_.commit = memory::make_shared<committed_state_t::element_type>(
      std::piecewise_construct,
      std::forward_as_tuple(std::move(to_commit.meta)),
      std::forward_as_tuple(std::move(pending_refs))
    );
  } catch (...) {
    writer_->rollback(); // rollback started transaction

    throw;
  }

  // ...........................................................................
  // only noexcept operations below
  // ...........................................................................

  cached_readers_.purge(to_commit.ctx->segment_mask_); // release cached readers
  pending_state_.ctx = std::move(to_commit.ctx);

  return true;
}

void index_writer::finish() {
  assert(!commit_lock_.try_lock()); // already locked

  REGISTER_TIMER_DETAILED();

  if (!pending_state_) {
    return;
  }

  auto reset_state = irs::make_finally([this]()noexcept {
    // release reference to flush_context
    pending_state_.reset();
  });

  // ...........................................................................
  // lightweight 2nd phase of the transaction
  // ...........................................................................

  try {
    if (!writer_->commit()) {
      throw illegal_state();
    }
#ifndef __APPLE__
    // atomic_store may throw
    static_assert(!noexcept
      (committed_state_helper::atomic_store(&committed_state_,
        std::move(pending_state_.commit))));
#endif
    committed_state_helper::atomic_store(&committed_state_,
      std::move(pending_state_.commit));
  } catch (...) {
    abort(); // rollback transaction

    throw;
  }

  // ...........................................................................
  // after here transaction successfull (only noexcept operations below)
  // ...........................................................................
  meta_.last_gen_ = committed_state_->first->gen_; // update 'last_gen_' to last commited/valid generation
}

void index_writer::abort() {
  assert(!commit_lock_.try_lock()); // already locked

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

}
