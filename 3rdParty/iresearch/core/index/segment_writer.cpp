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
#include "segment_writer.hpp"
#include "store/store_utils.hpp"
#include "index_meta.hpp"
#include "analysis/token_stream.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/index_utils.hpp"
#include "utils/log.hpp"
#include "utils/lz4compression.hpp"
#include "utils/map_utils.hpp"
#include "utils/timer_utils.hpp"
#include "utils/type_limits.hpp"
#include "utils/version_utils.hpp"

#include "index/norm.hpp"

namespace {

using namespace irs;

[[maybe_unused]] inline bool is_subset_of(
    const features_t& lhs,
    const feature_map_t& rhs) noexcept {
  for (const irs::type_info::type_id type: lhs) {
    if (!rhs.count(type)) {
      return false;
    }
  }
  return true;
}

}

namespace iresearch {

segment_writer::stored_column::stored_column(
    const hashed_string_ref& name,
    columnstore_writer& columnstore,
    const column_info_provider_t& column_info,
    std::deque<cached_column>& cached_columns,
    bool cache)
  : name(name.c_str(), name.size()),
    name_hash(name.hash()) {
  const auto info = column_info(name);

  columnstore_writer::column_finalizer_f finalizer =
      [this](bstring&) noexcept {
    return string_ref{this->name};
  };

  if (!cache) {
    std::tie(id, writer) = columnstore.push_column(info, std::move(finalizer));
  } else {
    auto& cached = cached_columns.emplace_back(&id, info, std::move(finalizer));

    writer = [stream = &cached.stream](irs::doc_id_t doc)-> column_output& {
      stream->prepare(doc);
      return *stream;
    };
  }
}

doc_id_t segment_writer::begin(
    const update_context& ctx,
    size_t reserve_rollback_extra /*= 0*/) {
  assert(docs_cached() + doc_limits::min() - 1 < doc_limits::eof());
  valid_ = true;
  doc_.clear(); // clear norm fields

  if (docs_mask_.capacity() <= docs_mask_.size() + 1 + reserve_rollback_extra) {
    docs_mask_.reserve(
      math::roundup_power2(docs_mask_.size() + 1 + reserve_rollback_extra) // reserve in blocks of power-of-2
    ); // reserve space for potential rollback
  }

  if (docs_context_.size() >= docs_context_.capacity()) {
    docs_context_.reserve(math::roundup_power2(docs_context_.size() + 1)); // reserve in blocks of power-of-2
  }

  docs_context_.emplace_back(ctx);

  return doc_id_t(docs_cached() + doc_limits::min() - 1); // -1 for 0-based offset
}

segment_writer::ptr segment_writer::make(
    directory& dir,
    const column_info_provider_t& column_info,
    const feature_info_provider_t& feature_info,
    const comparer* comparator) {
  return memory::maker<segment_writer>::make(
    dir, column_info,
    feature_info, comparator);
}

size_t segment_writer::memory_active() const noexcept {
  const auto docs_mask_extra = (0 != (docs_mask_.size() % sizeof(bitvector::word_t)))
     ? sizeof(bitvector::word_t) : 0;

  auto column_cache_active = std::accumulate(
    columns_.begin(), columns_.end(), size_t(0),
    [](size_t lhs, const stored_column& rhs) noexcept {
      return lhs + rhs.name.size() + sizeof(rhs);
  });

  column_cache_active += std::accumulate(
    std::begin(cached_columns_),
    std::end(cached_columns_),
    column_cache_active,
    [](const auto lhs, const auto& rhs) {
      return lhs + rhs.stream.memory_active();
  });

  return (docs_context_.size() * sizeof(update_contexts::value_type))
    + (docs_mask_.size() / 8 + docs_mask_extra) // FIXME too rough
    + fields_.memory_active()
    + sort_.stream.memory_active()
    + column_cache_active;
}

size_t segment_writer::memory_reserved() const noexcept {
  const auto docs_mask_extra = (0 != (docs_mask_.size() % sizeof(bitvector::word_t)))
    ? sizeof(bitvector::word_t) : 0;

  auto column_cache_reserved
    = columns_.capacity()*sizeof(decltype(columns_)::value_type);

  column_cache_reserved += std::accumulate(
    std::begin(cached_columns_),
    std::end(cached_columns_),
    column_cache_reserved,
    [](const auto lhs, const auto& rhs) {
      return lhs + rhs.stream.memory_reserved();
  });

  return sizeof(segment_writer)
    + (sizeof(update_contexts::value_type) * docs_context_.size())
    + (sizeof(bitvector) + docs_mask_.size() / 8 + docs_mask_extra)
    + fields_.memory_reserved()
    + sort_.stream.memory_reserved()
    + column_cache_reserved;
}

bool segment_writer::remove(doc_id_t doc_id) {
  if (!doc_limits::valid(doc_id)
      || (doc_id - doc_limits::min()) >= docs_cached()
      || docs_mask_.test(doc_id - doc_limits::min())) {
    return false;
  }

  docs_mask_.set(doc_id - doc_limits::min());

  return true;
}

segment_writer::segment_writer(
    directory& dir,
    const column_info_provider_t& column_info,
    const feature_info_provider_t& feature_info,
    const comparer* comparator) noexcept
  : sort_(column_info, {}),
    fields_(feature_info, cached_columns_, comparator),
    column_info_(&column_info),
    dir_(dir),
    initialized_(false) {
}

bool segment_writer::index(
    const hashed_string_ref& name,
    const doc_id_t doc,
    IndexFeatures index_features,
    const features_t& features,
    token_stream& tokens) {
  REGISTER_TIMER_DETAILED();
  assert(col_writer_);

  auto* slot = fields_.emplace(name, index_features, features, *col_writer_);

  // invert only if new field index features are a subset of slot index features
  assert(::is_subset_of(features, slot->meta().features));
  if (is_subset_of(index_features, slot->meta().index_features) &&
      slot->invert(tokens, doc)) {
    if (!slot->seen() && slot->has_features()) {
      doc_.emplace_back(slot);
      slot->seen(true);
    }

    return true;
  }

  valid_ = false;
  return false;
}

column_output& segment_writer::stream(
    const hashed_string_ref& name,
    const doc_id_t doc_id) {
  REGISTER_TIMER_DETAILED();
  assert(column_info_);

  return columns_.lazy_emplace(
    name,
    [this, &name](const auto& ctor){
      ctor(name, *col_writer_, *column_info_,
           cached_columns_, nullptr != fields_.comparator());
  })->writer(doc_id);
}

void segment_writer::flush_fields(const doc_map& docmap) {
  flush_state state;
  state.dir = &dir_;
  state.doc_count = docs_cached();
  state.name = seg_name_;
  state.docmap = fields_.comparator() && !docmap.empty() ? &docmap : nullptr;

  try {
    fields_.flush(*field_writer_, state);
  } catch (...) {
    field_writer_.reset(); // invalidate field writer

    throw;
  }
}

size_t segment_writer::flush_doc_mask(const segment_meta &meta) {
  document_mask docs_mask;
  docs_mask.reserve(docs_mask_.size());

  for (size_t doc_id = 0, doc_id_end = docs_mask_.size();
       doc_id < doc_id_end;
       ++doc_id) {
    if (docs_mask_.test(doc_id)) {
      assert(size_t(std::numeric_limits<doc_id_t>::max()) >= doc_id + doc_limits::min());
      docs_mask.emplace(doc_id_t(doc_id + doc_limits::min()));
    }
  }

  auto writer = meta.codec->get_document_mask_writer();
  writer->write(dir_, meta, docs_mask);

  return docs_mask.size();
}

void segment_writer::flush(index_meta::index_segment_t& segment) {
  REGISTER_TIMER_DETAILED();

  auto& meta = segment.meta;

  doc_map docmap;
  flush_state state;
  state.dir = &dir_;
  state.doc_count = docs_cached();
  state.name = seg_name_;
  state.docmap = nullptr;

  if (fields_.comparator()) {
    std::tie(docmap, sort_.id) = sort_.stream.flush(
        *col_writer_, std::move(sort_.finalizer),
        doc_id_t(docs_cached()), *fields_.comparator());

    // flush all cached columns
    irs::sorted_column::flush_buffer_t buffer;
    for (auto& column : cached_columns_) {
      if (IRS_LIKELY(!field_limits::valid(*column.id))) {
        *column.id = column.stream.flush(
            *col_writer_, std::move(column.finalizer), docmap, buffer);
      }
    }

    meta.sort = sort_.id; // store sorted column id in segment meta

    if (!docmap.empty()) {
      state.docmap = &docmap;
    }
  }

  // flush columnstore
  meta.column_store = col_writer_->commit(state);

  // flush fields metadata & inverted data,
  if (docs_cached()) {
    flush_fields(docmap);
  }

  // write non-empty document mask
  size_t docs_mask_count = 0;
  if (docs_mask_.any()) {
    docs_mask_count = flush_doc_mask(meta);
  }

  // update segment metadata
  assert(docs_cached() >= docs_mask_count);
  meta.docs_count = docs_cached();
  meta.live_docs_count = meta.docs_count - docs_mask_count;
  meta.files.clear(); // prepare empy set to be swaped into dir_
  dir_.flush_tracked(meta.files);

  // flush segment metadata
  index_utils::flush_index_segment(dir_, segment);
}

void segment_writer::reset() noexcept {
  initialized_ = false;
  tick_ = 0;
  dir_.clear_tracked();
  docs_context_.clear();
  docs_mask_.clear();
  fields_.reset();
  columns_.clear();
  cached_columns_.clear(); // FIXME(@gnusi): we loose all per-column buffers
  sort_.stream.clear();

  if (col_writer_) {
    col_writer_->rollback();
  }
}

void segment_writer::reset(const segment_meta& meta) {
  reset();

  seg_name_ = meta.name;

  if (!field_writer_) {
    field_writer_ = meta.codec->get_field_writer(false);
    assert(field_writer_);
  }

  if (!col_writer_) {
    col_writer_ = meta.codec->get_columnstore_writer(false);
    assert(col_writer_);
  }

  col_writer_->prepare(dir_, meta);

  initialized_ = true;
}

}
