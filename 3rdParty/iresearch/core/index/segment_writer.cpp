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
#include "utils/map_utils.hpp"
#include "utils/timer_utils.hpp"
#include "utils/type_limits.hpp"
#include "utils/version_utils.hpp"

#include <math.h>
#include <set>

NS_ROOT

segment_writer::column::column(
    const string_ref& name, 
    columnstore_writer& columnstore) {
  this->name.assign(name.c_str(), name.size());
  this->handle = columnstore.push_column();
}

doc_id_t segment_writer::begin(
    const update_context& ctx,
    size_t reserve_rollback_extra /*= 0*/
) {
  assert(docs_cached() + type_limits<type_t::doc_id_t>::min() - 1 < type_limits<type_t::doc_id_t>::eof());
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

  return doc_id_t(docs_cached() + type_limits<type_t::doc_id_t>::min() - 1); // -1 for 0-based offset
}

segment_writer::ptr segment_writer::make(directory& dir) {
  // can't use make_unique becuase of the private constructor
  return memory::maker<segment_writer>::make(dir);
}

size_t segment_writer::memory_active() const NOEXCEPT {
  const auto docs_mask_extra = docs_mask_.size() % sizeof(bitvector::word_t)
      ? sizeof(bitvector::word_t) : 0;

  return (docs_context_.size() * sizeof(update_contexts::value_type))
    + (docs_mask_.size() / 8 + docs_mask_extra) // FIXME too rough
    + fields_.memory_active();
}

size_t segment_writer::memory_reserved() const NOEXCEPT {
  const auto docs_mask_extra = docs_mask_.size() % sizeof(bitvector::word_t)
      ? sizeof(bitvector::word_t) : 0;

  return sizeof(segment_writer)
    + (sizeof(update_contexts::value_type) * docs_context_.size())
    + (sizeof(bitvector) + docs_mask_.size() / 8 + docs_mask_extra)
    + fields_.memory_reserved();
}

bool segment_writer::remove(doc_id_t doc_id) {
  if (!type_limits<type_t::doc_id_t>::valid(doc_id)
      || (doc_id - type_limits<type_t::doc_id_t>::min()) >= docs_cached()
      || docs_mask_.test(doc_id - type_limits<type_t::doc_id_t>::min())) {
    return false;
  }

  docs_mask_.set(doc_id - type_limits<type_t::doc_id_t>::min());

  return true;
}

segment_writer::segment_writer(directory& dir) NOEXCEPT
  : dir_(dir), initialized_(false) {
}

bool segment_writer::index(
    const hashed_string_ref& name,
    token_stream& tokens,
    const flags& features) {
  REGISTER_TIMER_DETAILED();

  assert(docs_cached() + type_limits<type_t::doc_id_t>::min() - 1 < type_limits<type_t::doc_id_t>::eof()); // user should check return of begin() != eof()
  auto doc_id =
    doc_id_t(docs_cached() + type_limits<type_t::doc_id_t>::min() - 1); // -1 for 0-based offset
  auto& slot = fields_.get(name);
  auto& slot_features = slot.meta().features;

  // invert only if new field features are a subset of slot features
  if ((slot.empty() || features.is_subset_of(slot_features)) &&
      slot.invert(tokens, slot.empty() ? features : slot_features, doc_id)) {
    if (features.check<norm>()) {
      norm_fields_.insert(&slot);
    }

    fields_ += features; // accumulate segment features
    return true;
  }

  return false;
}

columnstore_writer::column_output& segment_writer::stream(
    doc_id_t doc_id, const hashed_string_ref& name) {
  REGISTER_TIMER_DETAILED();

  static auto generator = [](
      const hashed_string_ref& key,
      const column& value) NOEXCEPT {
    // reuse hash but point ref at value
    return hashed_string_ref(key.hash(), value.name);
  };

  // replace original reference to 'name' provided by the caller
  // with a reference to the cached copy in 'value'
  return map_utils::try_emplace_update_key(
    columns_,                                     // container
    generator,                                    // key generator
    name,                                         // key
    name, *col_writer_                            // value
  ).first->second.handle.second(doc_id);
}

void segment_writer::finish() {
  REGISTER_TIMER_DETAILED();

  // write document normalization factors (for each field marked for normalization))
  float_t value;
  for (auto* field : norm_fields_) {
    value = 1.f / float_t(std::sqrt(double_t(field->size())));
    if (value != norm::DEFAULT()) {
      auto& stream = field->norms(*col_writer_);
      write_zvfloat(stream, value);
    }
  }
}

void segment_writer::flush_column_meta(const segment_meta& meta) {
  struct less_t {
    bool operator()(
        const column* lhs,
        const column* rhs
    ) const NOEXCEPT {
      return lhs->name < rhs->name;
    };
  };

  std::set<const column*, less_t> columns;

  // ensure columns are sorted
  for (auto& entry : columns_) {
    columns.emplace(&entry.second);
  }

  // flush columns meta
  try {
    col_meta_writer_->prepare(dir_, meta);
    for (auto& column: columns) {
      col_meta_writer_->write(column->name, column->handle.first);
    }
    col_meta_writer_->flush();
  } catch (...) {
    col_meta_writer_.reset(); // invalidate column meta writer

    throw;
  }
}

void segment_writer::flush_fields() {
  flush_state state;
  state.dir = &dir_;
  state.doc_count = docs_cached();
  state.name = seg_name_;

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
      assert(size_t(integer_traits<doc_id_t>::const_max) >= doc_id + type_limits<type_t::doc_id_t>::min());
      docs_mask.emplace(
        doc_id_t(doc_id + type_limits<type_t::doc_id_t>::min())
      );
    }
  }

  auto writer = meta.codec->get_document_mask_writer();
  writer->write(dir_, meta, docs_mask);

  return docs_mask.size();
}

void segment_writer::flush(index_meta::index_segment_t& segment) {
  REGISTER_TIMER_DETAILED();

  auto& meta = segment.meta;

  // flush columnstore and columns indices
  if (col_writer_->commit() && !columns_.empty()) {
    flush_column_meta(meta);
    meta.column_store = true;
  }

  // flush fields metadata & inverted data
  if (docs_cached()) {
    flush_fields();
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

void segment_writer::reset() NOEXCEPT {
  initialized_ = false;
  dir_.clear_tracked();
  docs_context_.clear();
  docs_mask_.clear();
  fields_.reset();
  columns_.clear();

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

NS_END
