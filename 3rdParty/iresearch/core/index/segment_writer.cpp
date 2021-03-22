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

namespace iresearch {

segment_writer::stored_column::stored_column(
    const hashed_string_ref& name,
    columnstore_writer& columnstore,
    const column_info_provider_t& column_info,
    bool cache)
  : name(name.c_str(), name.size()),
    name_hash(name.hash()),
    stream(column_info(name)) {
  if (!cache) {
    auto& info = stream.info();
    std::tie(id, writer) = columnstore.push_column(info);
  } else {
    writer = [this](irs::doc_id_t doc)->columnstore_writer::column_output& {
      this->stream.prepare(doc);
      return this->stream;
    };
  }
}

doc_id_t segment_writer::begin(
    const update_context& ctx,
    size_t reserve_rollback_extra /*= 0*/) {
  assert(docs_cached() + doc_limits::min() - 1 < doc_limits::eof());
  valid_ = true;
  norm_fields_.clear(); // clear norm fields

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
    const comparer* comparator) {
  return memory::maker<segment_writer>::make(dir, column_info, comparator);
}

size_t segment_writer::memory_active() const noexcept {
  const auto docs_mask_extra = docs_mask_.size() % sizeof(bitvector::word_t)
    ? sizeof(bitvector::word_t) : 0;

  const auto column_cache_active = std::accumulate(
    columns_.begin(), columns_.end(), size_t(0),
    [](size_t lhs, const stored_column& rhs) noexcept {
      return lhs + rhs.stream.memory_active();
  });

  return (docs_context_.size() * sizeof(update_contexts::value_type))
    + (docs_mask_.size() / 8 + docs_mask_extra) // FIXME too rough
    + fields_.memory_active()
    + sort_.stream.memory_active()
    + column_cache_active;
}

size_t segment_writer::memory_reserved() const noexcept {
  const auto docs_mask_extra = docs_mask_.size() % sizeof(bitvector::word_t)
    ? sizeof(bitvector::word_t) : 0;

  const auto column_cache_reserved = std::accumulate(
    columns_.begin(), columns_.end(), size_t(0),
    [](size_t lhs, const stored_column& rhs) noexcept {
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
    const comparer* comparator) noexcept
  : sort_(column_info),
    fields_(comparator),
    column_info_(&column_info),
    dir_(dir),
    initialized_(false) {
}

bool segment_writer::index(
    const hashed_string_ref& name,
    const doc_id_t doc,
    const flags& features,
    token_stream& tokens) {
  REGISTER_TIMER_DETAILED();

  auto* slot = fields_.emplace(name);
  auto& slot_features = slot->meta().features;

  const bool slot_empty = slot->empty();

  // invert only if new field features are a subset of slot features
  if ((slot_empty || features.is_subset_of(slot_features)) &&
      slot->invert(tokens, slot_empty ? features : slot_features, doc)) {
    if (slot->size() && features.check<norm>()) {
      norm_fields_.insert(slot);
    }

    fields_ += features; // accumulate segment features
    return true;
  }

  return false;
}

columnstore_writer::column_output& segment_writer::sorted_stream(const doc_id_t doc_id) {
  sort_.stream.prepare(doc_id);
  return sort_.stream;
}

columnstore_writer::column_output& segment_writer::stream(
    const hashed_string_ref& name,
    const doc_id_t doc_id) {
  REGISTER_TIMER_DETAILED();
  assert(column_info_);

  return columns_.lazy_emplace(
    name,
    [this, &name](const auto& ctor){
      ctor(name, *col_writer_, *column_info_,
           nullptr != fields_.comparator());
  })->writer(doc_id);
}

void segment_writer::finish() {
  REGISTER_TIMER_DETAILED();

  // write document normalization factors (for each field marked for normalization))
  float_t value;
  for (const auto* field: norm_fields_) {
    assert(field && field->size() > 0);
    value = 1.f / float_t(std::sqrt(double_t(field->size())));
    if (value != norm::DEFAULT()) {
      auto& stream = field->norms(*col_writer_);
      write_zvfloat(stream, value);
    }
  }
}

void segment_writer::flush_column_meta(const segment_meta& meta) {
  // ensure columns are sorted
  sorted_columns_.resize(columns_.size());
  auto begin = sorted_columns_.begin();
  for (auto& entry : columns_) {
    *begin = &entry;
    ++begin;
  }
  std::sort(
    sorted_columns_.begin(), sorted_columns_.end(),
    [](const stored_column* lhs, const stored_column* rhs) noexcept {
      return lhs->name < rhs->name;
  });

  // flush columns meta
  try {
    col_meta_writer_->prepare(dir_, meta);
    for (const auto* column: sorted_columns_) {
      col_meta_writer_->write(column->name, column->id);
    }
    col_meta_writer_->flush();
  } catch (...) {
    col_meta_writer_.reset(); // invalidate column meta writer

    throw;
  }
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

  if (fields_.comparator()) {
    std::tie(docmap, sort_.id) = sort_.stream.flush(
      *col_writer_,
      doc_id_t(docs_cached()),
      *fields_.comparator()
    );

    irs::sorted_column::flush_buffer_t buffer;
    for (auto& column : columns_) {

      if (!field_limits::valid(column.id)) {
        // cached column
        column.id = column.stream.flush(*col_writer_, docmap, buffer);
      }
    }

    meta.sort = sort_.id; // store sorted column id in segment meta
  }

  // flush columnstore
  if (col_writer_->commit()) {
    if (!columns_.empty()) {
      flush_column_meta(meta);
    }

    meta.column_store = true;
  }

  // flush fields metadata & inverted data
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

  if (!col_meta_writer_) {
    col_meta_writer_ = meta.codec->get_column_meta_writer();
    assert(col_meta_writer_);
  }

  if (!col_writer_) {
    col_writer_ = meta.codec->get_columnstore_writer();
    assert(col_writer_);
  }

  col_writer_->prepare(dir_, meta);

  initialized_ = true;
}

}
