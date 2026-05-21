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

#include "index/segment_writer.hpp"

#include "analysis/token_attributes.hpp"
#include "analysis/token_stream.hpp"
#include "index_meta.hpp"
#include "shared.hpp"
#include "store/store_utils.hpp"
#include "utils/index_utils.hpp"
#include "utils/log.hpp"
#include "utils/lz4compression.hpp"
#include "utils/timer_utils.hpp"
#include "utils/type_limits.hpp"

namespace irs {
namespace {

[[maybe_unused]] inline bool IsSubsetOf(const features_t& lhs,
                                        const feature_map_t& rhs) noexcept {
  for (const irs::type_info::type_id type : lhs) {
    if (!rhs.contains(type)) {
      return false;
    }
  }
  return true;
}

}  // namespace

segment_writer::stored_column::stored_column(
  const hashed_string_view& name, columnstore_writer& columnstore,
  IResourceManager& rm, const ColumnInfoProvider& column_info,
  std::deque<cached_column, ManagedTypedAllocator<cached_column>>&
    cached_columns,
  bool cache)
  : name{name}, name_hash{name.hash()} {
  const auto info = column_info(std::string_view(name));

  columnstore_writer::column_finalizer_f finalizer = [this](bstring&) noexcept {
    return std::string_view{this->name};
  };

  if (!cache) {
    auto column = columnstore.push_column(info, std::move(finalizer));
    id = column.id;
    writer = &column.out;
  } else {
    cached = &cached_columns.emplace_back(&id, info, std::move(finalizer), rm);

    writer = &cached->Stream();
  }
}

doc_id_t segment_writer::begin(DocContext ctx) {
  IRS_ASSERT(LastDocId() < doc_limits::eof());
  valid_ = true;
  doc_.clear();  // clear norm fields

  const auto needed_docs = buffered_docs() + 1;

  if (needed_docs >= docs_mask_.set.capacity()) {
    // reserve in blocks of power-of-2
    const auto count = math::roundup_power2(needed_docs);
    docs_mask_.set.reserve(count);
  }

  if (needed_docs >= docs_context_.capacity()) {
    // reserve in blocks of power-of-2
    const auto count = math::roundup_power2(needed_docs);
    docs_context_.reserve(count);
  }

  docs_context_.emplace_back(ctx);

  return LastDocId();
}

std::unique_ptr<segment_writer> segment_writer::make(
  directory& dir, const SegmentWriterOptions& options) {
  return std::make_unique<segment_writer>(ConstructToken{}, dir, options);
}

size_t segment_writer::memory_active() const noexcept {
  auto column_cache_active =
    std::accumulate(columns_.begin(), columns_.end(), size_t{0},
                    [](size_t lhs, const stored_column& rhs) noexcept {
                      return lhs + rhs.name.size() + sizeof(rhs);
                    });

  column_cache_active +=
    std::accumulate(cached_columns_.begin(), cached_columns_.end(), size_t{0},
                    [](size_t lhs, const cached_column& rhs) {
                      return lhs + rhs.Stream().MemoryActive();
                    });

  return docs_context_.size() * sizeof(DocContext) +
         bitset::bits_to_words(docs_mask_.count) * sizeof(bitset::word_t) +
         fields_.memory_active() + sort_.stream.MemoryActive() +
         column_cache_active;
}

size_t segment_writer::memory_reserved() const noexcept {
  auto column_cache_reserved =
    columns_.capacity() * sizeof(decltype(columns_)::value_type);

  column_cache_reserved +=
    std::accumulate(columns_.begin(), columns_.end(), size_t{0},
                    [](size_t lhs, const stored_column& rhs) noexcept {
                      return lhs + rhs.name.size();
                    });

  column_cache_reserved +=
    std::accumulate(cached_columns_.begin(), cached_columns_.end(), size_t{0},
                    [](size_t lhs, const cached_column& rhs) {
                      return lhs + rhs.Stream().MemoryActive() + sizeof(rhs);
                    });

  return sizeof(segment_writer) +
         docs_context_.capacity() * sizeof(DocContext) +
         docs_mask_.set.capacity() / bits_required<char>() +
         fields_.memory_reserved() + sort_.stream.MemoryReserved() +
         column_cache_reserved;
}

bool segment_writer::remove(doc_id_t doc_id) noexcept {
  if (!doc_limits::valid(doc_id)) {
    return false;
  }
  const auto doc = doc_id - doc_limits::min();
  if (buffered_docs() <= doc) {
    return false;
  }
  if (docs_mask_.set.size() <= doc) {
    docs_mask_.set.resize</*Reserve=*/false>(doc + 1);
  }
  const bool inserted = docs_mask_.set.try_set(doc);
  docs_mask_.count += static_cast<size_t>(inserted);
  return inserted;
}

segment_writer::segment_writer(ConstructToken, directory& dir,
                               const SegmentWriterOptions& options) noexcept
  : scorers_{options.scorers},
    cached_columns_{{options.resource_manager}},
    sort_{options.column_info, {}, options.resource_manager},
    docs_context_{{options.resource_manager}},
    fields_{options.feature_info, cached_columns_, options.scorers_features,
            options.resource_manager, options.comparator},
    columns_{{options.resource_manager}},
    column_info_{&options.column_info},
    dir_{dir} {
  docs_mask_.set = decltype(docs_mask_.set){{options.resource_manager}};
}

bool segment_writer::index(const hashed_string_view& name, const doc_id_t doc,
                           IndexFeatures index_features,
                           const features_t& features, token_stream& tokens) {
  REGISTER_TIMER_DETAILED();
  IRS_ASSERT(col_writer_);

  auto* slot = fields_.emplace(name, index_features, features, *col_writer_);

  // invert only if new field index features are a subset of slot index features
  IRS_ASSERT(IsSubsetOf(features, slot->meta().features));
  if (IsSubsetOf(index_features, slot->requested_features()) &&
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

column_output& segment_writer::stream(const hashed_string_view& name,
                                      const doc_id_t doc_id) {
  REGISTER_TIMER_DETAILED();
  IRS_ASSERT(column_info_);
  auto& out =
    *columns_
       .lazy_emplace(name,
                     [this, &name](const auto& ctor) {
                       ctor(name, *col_writer_,
                            docs_context_.get_allocator().ResourceManager(),
                            *column_info_, cached_columns_,
                            nullptr != fields_.comparator());
                     })
       ->writer;
  out.Prepare(doc_id);
  return out;
}

void segment_writer::FlushFields(flush_state& state) {
  IRS_ASSERT(field_writer_);

  try {
    fields_.flush(*field_writer_, state);
  } catch (...) {
    field_writer_.reset();  // invalidate field writer
    throw;
  }
}

[[nodiscard]] DocMap segment_writer::flush(IndexSegment& segment,
                                           DocsMask& docs_mask) {
  REGISTER_TIMER_DETAILED();
  auto& meta = segment.meta;

  flush_state state{.dir = &dir_,
                    .columns = this,
                    .name = seg_name_,
                    .scorers = scorers_,
                    .doc_count = buffered_docs()};

  DocMap docmap;
  if (fields_.comparator() != nullptr) {
    std::tie(docmap, sort_.id) = sort_.stream.Flush(
      *col_writer_, std::move(sort_.finalizer),
      static_cast<doc_id_t>(state.doc_count), *fields_.comparator());

    IRS_ASSERT(meta.codec != nullptr);
    auto writer = meta.codec->get_segment_meta_writer();
    if (writer->SupportPrimarySort()) {
      meta.sort = sort_.id;  // Store sorted column id in segment meta
    }

    if (!docmap.empty()) {
      state.docmap = &docmap;
    }
  }

  // Flush all cached columns
  IRS_ASSERT(column_ids_.empty());
  column_ids_.reserve(cached_columns_.size());
  for (BufferedColumn::BufferedValues buffer{cached_columns_.get_allocator()};
       auto& column : cached_columns_) {
    if (IRS_LIKELY(!field_limits::valid(column.id()))) {
      column.Flush(*col_writer_, docmap, buffer);
    }
    // invalid when was empty column
    if (IRS_LIKELY(field_limits::valid(column.id()))) {
      [[maybe_unused]] auto [_, emplaced] =
        column_ids_.emplace(column.id(), &column);
      IRS_ASSERT(emplaced);
    }
  }

  // Flush columnstore
  meta.column_store = col_writer_->commit(state);

  // Flush fields metadata & inverted data,
  if (state.doc_count != 0) {
    FlushFields(state);
  }

  // Get document mask
  IRS_ASSERT(docs_mask_.set.count() == docs_mask_.count);
  docs_mask = std::move(docs_mask_);
  docs_mask_.count = 0;

  // Update segment metadata
  meta.docs_count = state.doc_count;
  meta.live_docs_count = meta.docs_count - docs_mask.count;
  meta.files = dir_.FlushTracked(meta.byte_size);

  return docmap;
}

void segment_writer::reset() noexcept {
  initialized_ = false;
  dir_.ClearTracked();
  docs_context_.clear();
  docs_mask_.set.clear();
  docs_mask_.count = 0;
  fields_.reset();
  columns_.clear();
  column_ids_.clear();
  cached_columns_.clear();  // FIXME(@gnusi): we loose all per-column buffers
  sort_.stream.Clear();
  if (col_writer_) {
    col_writer_->rollback();
  }
}

void segment_writer::reset(const SegmentMeta& meta) {
  reset();

  seg_name_ = meta.name;

  if (!field_writer_) {
    field_writer_ = meta.codec->get_field_writer(
      false, docs_context_.get_allocator().ResourceManager());
    IRS_ASSERT(field_writer_);
  }

  if (!col_writer_) {
    col_writer_ = meta.codec->get_columnstore_writer(
      false, docs_context_.get_allocator().ResourceManager());
    IRS_ASSERT(col_writer_);
  }

  col_writer_->prepare(dir_, meta);

  initialized_ = true;
}

}  // namespace irs
