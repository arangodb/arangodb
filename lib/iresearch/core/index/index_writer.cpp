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

#include "index_writer.hpp"

#include <cstdint>

#include "formats/format_utils.hpp"
#include "index/comparer.hpp"
#include "index/directory_reader_impl.hpp"
#include "index/file_names.hpp"
#include "index/index_meta.hpp"
#include "index/merge_writer.hpp"
#include "index/segment_reader_impl.hpp"
#include "shared.hpp"
#include "utils/compression.hpp"
#include "utils/directory_utils.hpp"
#include "utils/index_utils.hpp"
#include "utils/timer_utils.hpp"
#include "utils/type_limits.hpp"

#include <absl/container/flat_hash_map.h>
#include <absl/strings/str_cat.h>

namespace irs {
namespace {

// do-nothing progress reporter, used as fallback if no other progress
// reporter is used
const ProgressReportCallback kNoProgress =
  [](std::string_view /*phase*/, size_t /*current*/, size_t /*total*/) {
    // intentionally do nothing
  };

const ColumnInfoProvider kDefaultColumnInfo = [](std::string_view) {
  // no compression, no encryption
  return ColumnInfo{irs::type<compression::none>::get(), {}, false};
};

const FeatureInfoProvider kDefaultFeatureInfo = [](irs::type_info::type_id) {
  // no compression, no encryption
  return std::pair{ColumnInfo{irs::type<compression::none>::get(), {}, false},
                   FeatureWriterFactory{}};
};

struct FlushedSegmentContext {
  FlushedSegmentContext(std::shared_ptr<const SegmentReaderImpl>&& reader,
                        IndexWriter::SegmentContext& segment,
                        IndexWriter::FlushedSegment& flushed,
                        const ResourceManagementOptions& rm)
    : reader{std::move(reader)}, segment{segment}, flushed{flushed} {
    IRS_ASSERT(this->reader != nullptr);
    if (flushed.docs_mask.count != doc_limits::eof()) {
      IRS_ASSERT(flushed.document_mask.empty());
      Init(rm);
    }
  }

  std::shared_ptr<const SegmentReaderImpl> reader;
  IndexWriter::SegmentContext& segment;
  IndexWriter::FlushedSegment& flushed;

  bool MakeDocumentMask(uint64_t tick, DocumentMask& document_mask,
                        IndexSegment& index) {
    if (flushed.document_mask.size() == flushed.meta.docs_count) {
      return true;
    }
    const auto begin = flushed.GetDocsBegin();
    const auto end = flushed.GetDocsEnd();
    IRS_ASSERT(begin < end);
    IRS_ASSERT(segment.flushed_docs_[begin].tick <= tick);
    const auto flushed_last_tick = segment.flushed_docs_[end - 1].tick;
    if (flushed_last_tick <= tick) {
      document_mask = std::move(flushed.document_mask);
      index = std::move(flushed);
      return false;
    }
    // intentionally copy
    document_mask = flushed.document_mask;
    for (auto rbegin = end - 1;
         rbegin > begin && segment.flushed_docs_[rbegin].tick > tick;
         --rbegin) {
      const auto old_doc = rbegin - begin + doc_limits::min();
      const auto new_doc = Old2New(old_doc);
      document_mask.insert(new_doc);
    }
    if (document_mask.size() == flushed.meta.docs_count) {
      return true;
    }
    index = flushed;
    return false;
  }

  void Remove(IndexWriter::QueryContext& query);
  void MaskUnusedReplace(uint64_t first_tick, uint64_t last_tick);

 private:
  void Init(const ResourceManagementOptions& rm) {
    if (!flushed.old2new.empty() && flushed.new2old.empty()) {
      flushed.new2old =
        decltype(flushed.new2old){flushed.old2new.size(), {*rm.transactions}};
      for (doc_id_t old_id = 0; const auto new_id : flushed.old2new) {
        flushed.new2old[new_id] = old_id++;
      }
    }

    DocumentMask document_mask{{*rm.readers}};

    IRS_ASSERT(flushed.GetDocsBegin() < flushed.GetDocsEnd());
    const auto end = flushed.GetDocsEnd() - flushed.GetDocsBegin();
    const auto invalid_end = static_cast<size_t>(flushed.meta.docs_count);

    // TODO(MBkkt) Is it good?
    document_mask.reserve(flushed.docs_mask.count + (invalid_end - end));

    // translate removes
    // https://lemire.me/blog/2018/02/21/iterating-over-set-bits-quickly
    const auto word_count =
      std::min(bitset::bits_to_words(end), flushed.docs_mask.set.words());
    for (size_t word_idx = 0; word_idx != word_count; ++word_idx) {
      auto word = flushed.docs_mask.set[word_idx];
      const auto old_doc =
        word_idx * bits_required<bitset::word_t>() + doc_limits::min();
      while (word != 0) {
        const auto t = word & -word;
        const auto offset = std::countr_zero(word);
        const auto new_doc = Old2New(old_doc + offset);
        document_mask.insert(new_doc);
        word ^= t;
      }
    }
    IRS_ASSERT(document_mask.size() == flushed.docs_mask.count);

    // in case of bad_alloc it's possible that we still need docs_mask
    // so we cannot reset it here: flushed.docs_mask = {};

    // lazy apply rollback
    for (auto old_doc = end + doc_limits::min(),
              end_doc = invalid_end + doc_limits::min();
         old_doc < end_doc; ++old_doc) {
      const auto new_doc = Old2New(old_doc);
      document_mask.insert(new_doc);
    }

    flushed.document_mask = std::move(document_mask);
    flushed.docs_mask.set = {};
    // set count to invalid
    flushed.docs_mask.count = doc_limits::eof();
  }

  static doc_id_t Translate(const auto& map, auto from) noexcept {
    IRS_ASSERT(doc_limits::invalid() < from);
    IRS_ASSERT(from <= doc_limits::eof());
    if (map.empty()) {
      return static_cast<doc_id_t>(from);
    }
    IRS_ASSERT(from < map.size());
    return map[from];
  }

  doc_id_t New2Old(auto new_doc) const noexcept {
    return Translate(flushed.new2old, new_doc);
  }
  doc_id_t Old2New(auto old_doc) const noexcept {
    return Translate(flushed.old2new, old_doc);
  }
};

// Apply any document removals based on filters in the segment.
// modifications where to get document update_contexts from
// docs_mask where to apply document removals to
// readers readers by segment name
// meta key used to get reader for the segment to evaluate
// Return if any new records were added (modification_queries_ modified).
void RemoveFromExistingSegment(DocumentMask& deleted_docs,
                               IndexWriter::QueryContext& query,
                               const SubReader& reader) {
  if (query.filter == nullptr) {
    return;
  }

  auto prepared = query.filter->prepare({.index = reader});

  if (IRS_UNLIKELY(!prepared)) {
    return;  // skip invalid prepared filters
  }

  auto itr = prepared->execute({.segment = reader});

  if (IRS_UNLIKELY(!itr)) {
    return;  // skip invalid iterators
  }

  const auto& docs_mask = *reader.docs_mask();
  while (itr->next()) {
    const auto doc_id = itr->value();

    // if the indexed doc_id was already masked then it should be skipped
    if (docs_mask.contains(doc_id)) {
      continue;  // the current modification query does not match any records
    }
    if (deleted_docs.insert(doc_id).second) {
      query.ForceDone();
    }
  }
}

bool RemoveFromImportedSegment(DocumentMask& deleted_docs,
                               IndexWriter::QueryContext& query,
                               const SubReader& reader) {
  if (query.filter == nullptr) {
    return false;
  }

  auto prepared = query.filter->prepare({.index = reader});
  if (IRS_UNLIKELY(!prepared)) {
    return false;  // skip invalid prepared filters
  }

  auto itr = prepared->execute({.segment = reader});
  if (IRS_UNLIKELY(!itr)) {
    return false;  // skip invalid iterators
  }

  bool modified = false;
  while (itr->next()) {
    const auto doc_id = itr->value();

    // if the indexed doc_id was already masked then it should be skipped
    if (!deleted_docs.insert(doc_id).second) {
      continue;  // the current modification query does not match any records
    }

    query.ForceDone();
    modified = true;
  }

  return modified;
}

// Apply any document removals based on filters in the segment.
// modifications where to get document update_contexts from
// segment where to apply document removals to
// min_doc_id staring doc_id that should be considered
// readers readers by segment name
void FlushedSegmentContext::Remove(IndexWriter::QueryContext& query) {
  if (query.filter == nullptr) {
    return;
  }

  auto& document_mask = flushed.document_mask;

  auto prepared = query.filter->prepare({.index = *reader});

  if (IRS_UNLIKELY(!prepared)) {
    return;  // Skip invalid prepared filters
  }

  auto itr = prepared->execute({.segment = *reader});

  if (IRS_UNLIKELY(!itr)) {
    return;  // Skip invalid iterators
  }

  auto* flushed_docs = segment.flushed_docs_.data() + flushed.GetDocsBegin();
  while (itr->next()) {
    const auto new_doc = itr->value();
    const auto old_doc = New2Old(new_doc);

    const auto& doc = flushed_docs[old_doc - doc_limits::min()];

    if (query.tick < doc.tick || !document_mask.insert(new_doc).second ||
        query.IsDone()) {
      continue;
    }
    if (doc.query_id == writer_limits::kInvalidOffset) {
      query.Done();
    } else {
      query.DependsOn(segment.queries_[doc.query_id]);
    }
  }
}

// Mask documents created by replace which did not have any matches.
void FlushedSegmentContext::MaskUnusedReplace(uint64_t first_tick,
                                              uint64_t last_tick) {
  const auto begin = flushed.GetDocsBegin();
  const auto end = flushed.GetDocsEnd();
  const std::span docs{segment.flushed_docs_.data() + begin,
                       segment.flushed_docs_.data() + end};
  for (const auto& doc : docs) {
    if (doc.tick <= first_tick) {
      continue;
    }
    if (last_tick < doc.tick) {
      break;
    }
    if (doc.query_id == writer_limits::kInvalidOffset) {
      continue;
    }
    IRS_ASSERT(doc.query_id < segment.queries_.size());
    if (!segment.queries_[doc.query_id].IsDone()) {
      const auto new_doc =
        Old2New(static_cast<size_t>(&doc - docs.data()) + doc_limits::min());
      flushed.document_mask.insert(new_doc);
    }
  }
}

// Write the specified document mask and adjust version and
// live documents count of the specified meta.
// Return index of the mask file withing segment file list
size_t WriteDocumentMask(directory& dir, SegmentMeta& meta,
                         const DocumentMask& docs_mask,
                         bool increment_version = true) {
  IRS_ASSERT(!docs_mask.empty());
  IRS_ASSERT(docs_mask.size() <= std::numeric_limits<uint32_t>::max());

  // Update live docs count
  IRS_ASSERT(docs_mask.size() < meta.docs_count);
  meta.live_docs_count =
    meta.docs_count - static_cast<doc_id_t>(docs_mask.size());

  auto mask_writer = meta.codec->get_document_mask_writer();

  auto it = meta.files.end();
  if (increment_version) {
    // Current filename
    it = std::find(meta.files.begin(), meta.files.end(),
                   mask_writer->filename(meta));
    // Segment modified due to new document_mask
    meta.version += 2;  // TODO(MBkkt) divide by 2
  }
  IRS_ASSERT(increment_version || it == std::find(meta.files.begin(), it,
                                                  mask_writer->filename(meta)));

  // FIXME(gnusi): Consider returning mask file name from `write` to avoiding
  // calling `filename`.
  // Write docs mask file after version is incremented
  meta.byte_size += mask_writer->write(dir, meta, docs_mask);

  if (it != meta.files.end()) {
    // FIXME(gnusi): We can avoid calling `length` in case if size of
    // the previous mask file would be known.
    auto get_file_size = [&dir](std::string_view file) -> uint64_t {
      uint64_t size;
      if (!dir.length(size, file)) {
        throw io_error{
          absl::StrCat("Failed to get length of the file '", file, "'")};
      }
      return size;
    };

    meta.byte_size -= get_file_size(*it);

    // Replace existing mask file with the new one
    *it = mask_writer->filename(meta);
  } else {
    // Add mask file to the list of files
    meta.files.emplace_back(mask_writer->filename(meta));
    it = std::prev(meta.files.end());
  }

  IRS_ASSERT(meta.files.begin() <= it);
  return static_cast<size_t>(it - meta.files.begin());
}

struct CandidateMapping {
  const SubReader* new_segment{};
  struct Old {
    const SubReader* segment{};
    size_t index{};  // within merge_writer
  } old;
};

// mapping: name -> { new segment, old segment }
using CandidatesMapping =
  absl::flat_hash_map<std::string_view, CandidateMapping>;

struct MapCandidatesResult {
  // Number of mapped candidates.
  size_t count{0};
  bool has_removals{false};  // cppcheck-suppress unusedStructMember
};

// candidates_mapping output mapping
// candidates candidates for mapping
// segments map against a specified segments
MapCandidatesResult MapCandidates(CandidatesMapping& candidates_mapping,
                                  ConsolidationView candidates,
                                  const auto& index) {
  size_t num_candidates = 0;
  for (const auto* candidate : candidates) {
    candidates_mapping.emplace(
      candidate->Meta().name,
      CandidateMapping{.old = {candidate, num_candidates++}});
  }

  size_t found = 0;
  bool has_removals = false;
  const auto candidate_not_found = candidates_mapping.end();

  for (const auto& segment : index) {
    const auto& meta = segment.Meta();
    const auto it = candidates_mapping.find(meta.name);

    if (candidate_not_found == it) {
      // not a candidate
      continue;
    }

    auto& mapping = it->second;
    const auto* new_segment = mapping.new_segment;

    if (new_segment && new_segment->Meta().version >= meta.version) {
      // mapping already has a newer segment version
      continue;
    }

    IRS_ASSERT(mapping.old.segment);
    if constexpr (std::is_same_v<SegmentReader,
                                 std::decay_t<decltype(segment)>>) {
      mapping.new_segment = segment.GetImpl().get();
    } else {
      mapping.new_segment = &segment;
    }

    // FIXME(gnusi): can't we just check pointers?
    IRS_ASSERT(mapping.old.segment);
    has_removals |= (meta.version != mapping.old.segment->Meta().version);

    if (++found == num_candidates) {
      break;
    }
  }

  return {found, has_removals};
}

bool MapRemovals(const CandidatesMapping& candidates_mapping,
                 const MergeWriter& merger, DocumentMask& docs_mask) {
  IRS_ASSERT(merger);

  for (auto& mapping : candidates_mapping) {
    const auto& segment_mapping = mapping.second;
    const auto* new_segment = segment_mapping.new_segment;
    IRS_ASSERT(new_segment);
    const auto& new_meta = new_segment->Meta();
    IRS_ASSERT(segment_mapping.old.segment);
    const auto& old_meta = segment_mapping.old.segment->Meta();

    if (new_meta.version != old_meta.version) {
      const auto& merge_ctx = merger[segment_mapping.old.index];
      auto merged_itr = merge_ctx.reader->docs_iterator();
      auto current_itr = new_segment->docs_iterator();

      // this only masks documents of a single segment
      // this works due to the current architectural approach of segments,
      // either removals are new and will be applied during flush_all()
      // or removals are in the docs_mask and still be applied by the reader
      // passed to the merge_writer

      // no more docs in merged reader
      if (!merged_itr->next()) {
        if (current_itr->next()) {
          IRS_LOG_WARN(absl::StrCat(
            "Failed to map removals for consolidated segment '", old_meta.name,
            "' version '", old_meta.version, "' from current segment '",
            new_meta.name, "' version '", new_meta.version,
            "', current segment has doc_id '", current_itr->value(),
            "' not present in the consolidated segment"));

          return false;  // current reader has unmerged docs
        }

        continue;  // continue wih next mapping
      }

      // mask all remaining doc_ids
      if (!current_itr->next()) {
        do {
          IRS_ASSERT(doc_limits::valid(merge_ctx.doc_map(
            merged_itr->value())));  // doc_id must have a valid mapping
          docs_mask.insert(merge_ctx.doc_map(merged_itr->value()));
        } while (merged_itr->next());

        continue;  // continue wih next mapping
      }

      // validate that all docs in the current reader were merged, and add any
      // removed docs to the merged mask
      for (;;) {
        while (merged_itr->value() < current_itr->value()) {
          // doc_id must have a valid mapping
          IRS_ASSERT(doc_limits::valid(merge_ctx.doc_map(merged_itr->value())));
          docs_mask.insert(merge_ctx.doc_map(merged_itr->value()));

          if (!merged_itr->next()) {
            IRS_LOG_WARN(absl::StrCat(
              "Failed to map removals for consolidated segment '",
              old_meta.name, "' version '", old_meta.version,
              "' from current segment '", new_meta.name, "' version '",
              new_meta.version, "', current segment has doc_id '",
              current_itr->value(),
              "' not present in the consolidated segment"));

            return false;  // current reader has unmerged docs
          }
        }

        if (merged_itr->value() > current_itr->value()) {
          IRS_LOG_WARN(absl::StrCat(
            "Failed to map removals for consolidated segment '", old_meta.name,
            "' version '", old_meta.version, "' from current segment '",
            new_meta.name, "' version '", new_meta.version,
            "', current segment has doc_id '", current_itr->value(),
            "' not present in the consolidated segment"));

          return false;  // current reader has unmerged docs
        }

        // no more docs in merged reader
        if (!merged_itr->next()) {
          if (current_itr->next()) {
            IRS_LOG_WARN(absl::StrCat(
              "Failed to map removals for consolidated segment '",
              old_meta.name, "' version '", old_meta.version,
              "' from current segment '", new_meta.name, "' version '",
              new_meta.version, "', current segment has doc_id '",
              current_itr->value(),
              "' not present in the consolidated segment"));

            return false;  // current reader has unmerged docs
          }

          break;  // continue wih next mapping
        }

        // mask all remaining doc_ids
        if (!current_itr->next()) {
          do {
            // doc_id must have a valid mapping
            IRS_ASSERT(
              doc_limits::valid(merge_ctx.doc_map(merged_itr->value())));
            docs_mask.insert(merge_ctx.doc_map(merged_itr->value()));
          } while (merged_itr->next());

          break;  // continue wih next mapping
        }
      }
    }
  }

  return true;
}

std::string ToString(ConsolidationView consolidation) {
  std::string str;

  size_t total_size = 0;
  size_t total_docs_count = 0;
  size_t total_live_docs_count = 0;

  for (const auto* segment : consolidation) {
    auto& meta = segment->Meta();

    absl::StrAppend(&str, "Name='", meta.name,
                    "', docs_count=", meta.docs_count,
                    ", live_docs_count=", meta.live_docs_count,
                    ", size=", meta.byte_size, "\n");

    total_docs_count += meta.docs_count;
    total_live_docs_count += meta.live_docs_count;
    total_size += meta.byte_size;
  }

  absl::StrAppend(&str, "Total: segments=", consolidation.size(),
                  ", docs_count=", total_docs_count,
                  ", live_docs_count=", total_live_docs_count,
                  " size=", total_size, "");

  return str;
}

bool IsInitialCommit(const DirectoryMeta& meta) noexcept {
  // Initial commit is always for required for empty directory
  return meta.filename.empty();
}

struct PartialSync {
  PartialSync(size_t segment_index, size_t file_index) noexcept
    : segment_index{segment_index}, file_index{file_index} {}

  size_t segment_index;  // Index of the segment within index meta
  size_t file_index;     // Index of the file in segment file list
};

std::vector<std::string_view> GetFilesToSync(
  std::span<const IndexSegment> segments,
  std::span<const PartialSync> partial_sync, size_t partial_sync_threshold) {
  // FIXME(gnusi): make format dependent?
  static constexpr size_t kFilesPerSegment = 9;

  IRS_ASSERT(partial_sync_threshold <= segments.size());
  const size_t full_sync_count = segments.size() - partial_sync_threshold;

  std::vector<std::string_view> files_to_sync;
  // +1 for index meta
  files_to_sync.reserve(1 + partial_sync.size() * 2 +
                        full_sync_count * kFilesPerSegment);

  for (auto sync : partial_sync) {
    IRS_ASSERT(sync.segment_index < partial_sync_threshold);
    const auto& segment = segments[sync.segment_index];
    files_to_sync.emplace_back(segment.filename);
    files_to_sync.emplace_back(segment.meta.files[sync.file_index]);
  }

  std::for_each(segments.begin() + partial_sync_threshold, segments.end(),
                [&files_to_sync](const IndexSegment& segment) {
                  files_to_sync.emplace_back(segment.filename);
                  const auto& files = segment.meta.files;
                  IRS_ASSERT(files.size() <= kFilesPerSegment);
                  files_to_sync.insert(files_to_sync.end(), files.begin(),
                                       files.end());
                });

  return files_to_sync;
}

}  // namespace

using namespace std::chrono_literals;

IndexWriter::ActiveSegmentContext::ActiveSegmentContext(
  std::shared_ptr<SegmentContext> segment, std::atomic_size_t& segments_active,
  FlushContext* flush, size_t pending_segment_offset) noexcept
  : segment_{std::move(segment)},
    segments_active_{&segments_active},
    flush_{flush},
    pending_segment_offset_{pending_segment_offset} {
  IRS_ASSERT(segment_ != nullptr);
}

IndexWriter::ActiveSegmentContext::~ActiveSegmentContext() {
  if (segments_active_ == nullptr) {
    IRS_ASSERT(segment_ == nullptr);
    IRS_ASSERT(flush_ == nullptr);
    return;
  }
  segment_.reset();
  segments_active_->fetch_sub(1, std::memory_order_relaxed);
  if (flush_ != nullptr) {
    flush_->pending_.Done();
  }
}

IndexWriter::ActiveSegmentContext::ActiveSegmentContext(
  ActiveSegmentContext&& other) noexcept
  : segment_{std::move(other.segment_)},
    segments_active_{std::exchange(other.segments_active_, nullptr)},
    flush_{std::exchange(other.flush_, nullptr)},
    pending_segment_offset_{std::exchange(other.pending_segment_offset_,
                                          writer_limits::kInvalidOffset)} {}

IndexWriter::ActiveSegmentContext& IndexWriter::ActiveSegmentContext::operator=(
  ActiveSegmentContext&& other) noexcept {
  if (this != &other) {
    std::swap(segment_, other.segment_);
    std::swap(segments_active_, other.segments_active_);
    std::swap(flush_, other.flush_);
    std::swap(pending_segment_offset_, other.pending_segment_offset_);
  }
  return *this;
}

IndexWriter::Document::Document(SegmentContext& segment,
                                segment_writer::DocContext doc,
                                QueryContext* query)
  : writer_{*segment.writer_}, query_{query} {
  IRS_ASSERT(segment.writer_ != nullptr);
  writer_.begin(doc);  // ensure Reset() will be noexcept
  segment.buffered_docs_.store(writer_.buffered_docs(),
                               std::memory_order_relaxed);
}

IndexWriter::Document::~Document() noexcept {
  try {
    writer_.commit();
  } catch (...) {
    writer_.rollback();
  }
  if (!writer_.valid() && query_ != nullptr) {
    // TODO(MBkkt) segment.queries.pop_back()?
    query_->filter = nullptr;
  }
}

void IndexWriter::Transaction::Reset() noexcept {
  // TODO(MBkkt) rename Reset() to Rollback()
  if (auto* segment = active_.Segment(); segment != nullptr) {
    segment->Rollback();
  }
}

void IndexWriter::Transaction::RegisterFlush() {
  if (active_.Segment() != nullptr && active_.Flush() == nullptr) {
    writer_->GetFlushContext()->AddToPending(active_);
  }
}

bool IndexWriter::Transaction::CommitImpl(uint64_t last_tick) noexcept try {
  auto* segment = active_.Segment();
  IRS_ASSERT(segment != nullptr);
  segment->Commit(queries_, last_tick);
  writer_->GetFlushContext()->Emplace(std::move(active_));
  IRS_ASSERT(active_.Segment() == nullptr);
  return true;
} catch (...) {
  IRS_ASSERT(active_.Segment() != nullptr);
  // TODO(MBkkt) Use intrusive list to avoid possibility bad_alloc here
  Abort();
  return false;
}

void IndexWriter::Transaction::Abort() noexcept {
  auto* segment = active_.Segment();
  if (segment == nullptr) {
    return;  // nothing to do
  }
  if (active_.Flush() == nullptr) {
    segment->Reset();  // reset before returning to pool
    active_ = {};      // back to pool no needed Rollback
    return;
  }
  segment->Rollback();
  // cannot throw because active_.Flush() not null
  writer_->GetFlushContext()->Emplace(std::move(active_));
  IRS_ASSERT(active_.Segment() == nullptr);
}

void IndexWriter::Transaction::UpdateSegment(bool disable_flush) {
  IRS_ASSERT(Valid());
  while (active_.Segment() == nullptr) {  // lazy init
    active_ = writer_->GetSegmentContext();
  }

  auto& segment = *active_.Segment();
  auto& writer = *segment.writer_;

  if (IRS_LIKELY(writer.initialized())) {
    if (disable_flush || !writer_->FlushRequired(writer)) {
      return;
    }
    // Force flush of a full segment
    IRS_LOG_TRACE(absl::StrCat(
      "Flushing segment '", writer.name(), "', docs=", writer.buffered_docs(),
      ", memory=", writer.memory_active(),
      ", docs limit=", writer_->segment_limits_.segment_docs_max.load(),
      ", memory limit=", writer_->segment_limits_.segment_memory_max.load()));

    try {
      segment.Flush();
    } catch (...) {
      IRS_LOG_ERROR(absl::StrCat("while flushing segment '",
                                 segment.writer_meta_.meta.name,
                                 "', error: failed to flush segment"));
      // TODO(MBkkt) What the goal are we want to achieve
      //  with keeping already flushed data?
      segment.Reset(true);
      throw;
    }
  }
  segment.Prepare();
}

bool IndexWriter::FlushRequired(const segment_writer& segment) const noexcept {
  const auto& limits = segment_limits_;
  const auto docs_max = limits.segment_docs_max.load();
  const auto memory_max = limits.segment_memory_max.load();

  const auto docs = segment.buffered_docs();
  const auto memory = segment.memory_active();

  return memory_max <= memory || docs_max <= docs;
}

void IndexWriter::FlushContext::Emplace(ActiveSegmentContext&& active) {
  IRS_ASSERT(active.segment_ != nullptr);

  if (active.segment_->first_tick_ == writer_limits::kMaxTick) {
    // Reset all segment data because there wasn't successful transactions
    active.segment_->Reset();
    active = {};  // release
    return;
  }

  auto* flush = active.flush_;
  const bool is_null = flush == nullptr;
  if (!is_null && flush != this) {
    active = {};  // release
    return;
  }

  std::lock_guard lock{pending_.Mutex()};
  auto* node = [&] {
    if (is_null) {
      return &pending_segments_.emplace_back(std::move(active.segment_),
                                             pending_segments_.size());
    }
    IRS_ASSERT(active.pending_segment_offset_ < pending_segments_.size());
    auto& segment_context = pending_segments_[active.pending_segment_offset_];
    IRS_ASSERT(segment_context.segment_ == active.segment_);
    return &segment_context;
  }();
  pending_freelist_.push(*node);
  active = {};
}

void IndexWriter::FlushContext::AddToPending(ActiveSegmentContext& active) {
  std::lock_guard lock{pending_.Mutex()};
  const auto size_before = pending_segments_.size();
  IRS_ASSERT(active.segment_ != nullptr);
  pending_segments_.emplace_back(active.segment_, size_before);
  active.flush_ = this;
  active.pending_segment_offset_ = size_before;
  pending_.Add();
}

void IndexWriter::FlushContext::Reset() noexcept {
  // reset before returning to pool
  for (auto& segment : segments_) {
    // use_count here isn't used for synchronization
    if (segment.use_count() == 1) {
      segment->Reset();
    }
  }

  imports_.clear();
  cached_.clear();
  segments_.clear();
  segment_mask_.clear();

  for (auto& entry : pending_segments_) {
    if (auto& segment = entry.segment_; segment != nullptr) {
      segment->Reset();
    }
  }
  ClearPending();
  dir_->clear_refs();
}

void IndexWriter::Cleanup(FlushContext& curr, FlushContext* next) noexcept {
  for (auto& import : curr.imports_) {
    auto& candidates = import.consolidation_ctx.candidates;
    for (const auto* candidate : candidates) {
      consolidating_segments_.erase(candidate->Meta().name);
    }
  }
  for (const auto& entry : curr.cached_) {
    consolidating_segments_.erase(entry.second->Meta().name);
  }
  if (next != nullptr) {
    for (const auto& entry : next->cached_) {
      consolidating_segments_.erase(entry.second->Meta().name);
    }
  }
}

uint64_t IndexWriter::FlushContext::FlushPending(uint64_t committed_tick,
                                                 uint64_t tick) {
  // if tick is not equal uint64_max, as result of bad_alloc it's possible here
  // that not all segments which should be committed by next FlushContext
  // (fully or partially) will be moved to it.
  // I consider it's ok, because in such situation you rely on tick,
  // but you cannot assume anything about your IndexWriter::Transaction between
  // last successfully committed tick and state before you understand that
  // IndexWriter::Commit(tick) is failed in multi-threaded environment.
  // Some Transactions after tick is initially in current FlushContext.
  // Some Transactions after tick is initially in next FlushContext.
  // From outside view you cannot distinct them!
  // So even if I will make this moving deterministic it's not helpful at all.
  // Also on practice such situation is almost impossible.
  // Probably in future we can implement some out of sync logic for IndexWriter.
  // But now it's unnecessary for our usage.

  IRS_ASSERT(next_ != nullptr);
  auto& next_segments = next_->segments_;
  IRS_ASSERT(next_segments.empty());
  size_t to_next_pending_segments = 0;
  uint64_t flushed_tick = committed_tick;
  for (auto& entry : pending_segments_) {
    auto& segment = entry.segment_;
    IRS_ASSERT(segment != nullptr);
    const auto first_tick = segment->first_tick_;
    const auto last_tick = segment->last_tick_;
    if (first_tick <= tick) {
      // This assert is really paranoid, it's not required just try to detect
      // situation when we commit on tick but forgot to call RegisterFlush().
      // This assert can work only if any transaction which committed after last
      // Commit will has greater first tick than committed tick.
      IRS_ASSERT(committed_tick < first_tick);
      flushed_tick = std::max(flushed_tick, last_tick);
      segment->Flush();
      if (tick < last_tick) {
        next_segments.push_back(segment);
      }
      segments_.push_back(std::move(segment));
    } else {
      ++to_next_pending_segments;
    }
  }

  if (to_next_pending_segments != 0) {
    std::lock_guard lock{next_->pending_.Mutex()};
    for (auto& entry : pending_segments_) {
      if (auto& segment = entry.segment_; segment != nullptr) {
        IRS_ASSERT(tick < segment->first_tick_);
        auto& node = next_->pending_segments_.emplace_back(
          std::move(segment), next_->pending_segments_.size());
        next_->pending_freelist_.push(node);
      }
    }
  }

#ifdef IRESEARCH_DEBUG
  for (auto& entry : pending_segments_) {
    IRS_ASSERT(entry.segment_ == nullptr);
  }
#endif
  ClearPending();
  return flushed_tick;
}

IndexWriter::SegmentContext::SegmentContext(
  directory& dir, segment_meta_generator_t&& meta_generator,
  const SegmentWriterOptions& options)
  : dir_{dir},
    queries_{{options.resource_manager}},
    flushed_{{options.resource_manager}},
    flushed_docs_{{options.resource_manager}},
    meta_generator_{std::move(meta_generator)},
    writer_{segment_writer::make(dir_, options)} {
  IRS_ASSERT(meta_generator_);
}

void IndexWriter::SegmentContext::Flush() {
  if (!writer_->initialized() || writer_->buffered_docs() == 0) {
    flushed_queries_ = queries_.size();
    IRS_ASSERT(committed_buffered_docs_ == 0);
    return;  // Skip flushing an empty writer
  }
  IRS_ASSERT(writer_->buffered_docs() <= doc_limits::eof());

  Finally reset_writer = [&]() noexcept {
    writer_->reset();
    committed_buffered_docs_ = 0;
  };

  DocsMask docs_mask{.set{flushed_docs_.get_allocator()}};
  auto old2new = writer_->flush(writer_meta_, docs_mask);

  if (writer_meta_.meta.live_docs_count == 0) {
    return;
  }
  const auto docs_context = writer_->docs_context();
  IRS_ASSERT(writer_meta_.meta.live_docs_count <= writer_meta_.meta.docs_count);
  IRS_ASSERT(writer_meta_.meta.docs_count == docs_context.size());

  flushed_.emplace_back(std::move(writer_meta_), std::move(old2new),
                        std::move(docs_mask), flushed_docs_.size());
  try {
    flushed_docs_.insert(flushed_docs_.end(), docs_context.begin(),
                         docs_context.end());
  } catch (...) {
    flushed_.pop_back();
    throw;
  }
  flushed_queries_ = queries_.size();
  committed_flushed_docs_ += committed_buffered_docs_;
}

IndexWriter::SegmentContext::ptr IndexWriter::SegmentContext::make(
  directory& dir, segment_meta_generator_t&& meta_generator,
  const SegmentWriterOptions& segment_writer_options) {
  return std::make_unique<SegmentContext>(dir, std::move(meta_generator),
                                          segment_writer_options);
}

void IndexWriter::SegmentContext::Prepare() {
  IRS_ASSERT(!writer_->initialized());
  writer_meta_.filename.clear();
  writer_meta_.meta = meta_generator_();
  writer_->reset(writer_meta_.meta);
}

void IndexWriter::SegmentContext::Reset(bool store_flushed) noexcept {
  buffered_docs_.store(0, std::memory_order_relaxed);

  if (IRS_UNLIKELY(store_flushed)) {
    queries_.resize(flushed_queries_);
    committed_queries_ = std::min(committed_queries_, flushed_queries_);
  } else {
    queries_.clear();
    flushed_queries_ = 0;
    committed_queries_ = 0;

    flushed_.clear();
    flushed_docs_.clear();
    committed_flushed_docs_ = 0;

    // TODO(MBkkt) What about ticks in case of store_flushed?
    //  Of course it's valid but maybe we can decrease range?
    first_tick_ = writer_limits::kMaxTick;
    last_tick_ = writer_limits::kMinTick;
    has_replace_ = false;
  }
  committed_buffered_docs_ = 0;

  if (writer_->initialized()) {
    writer_->reset();  // try to reduce number of files flushed below
  }

  // TODO(MBkkt) Is it ok to release refs in case of store_flushed?
  // release refs only after clearing writer state to ensure
  // 'writer_' does not hold any files
  dir_.clear_refs();
}

void IndexWriter::SegmentContext::Rollback() noexcept {
  // rollback modification queries
  IRS_ASSERT(committed_queries_ <= queries_.size());
  queries_.resize(committed_queries_);

  // truncate uncommitted flushed tail
  const auto end = flushed_.end();
  // TODO(MBkkt) reverse find order
  auto it = std::find_if(flushed_.begin(), end, [&](const auto& flushed) {
    return committed_flushed_docs_ < flushed.GetDocsEnd();
  });
  if (it != end) {
    // because committed docs point to flushed
    IRS_ASSERT(committed_buffered_docs_ == 0);
    const auto docs_end = it->GetDocsEnd();
    if (it->SetCommitted(committed_flushed_docs_)) {
      flushed_.erase(it + 1, end);
      IRS_ASSERT(committed_flushed_docs_ < docs_end);
      flushed_docs_.resize(docs_end);
      committed_flushed_docs_ = docs_end;
    } else {
      flushed_.erase(it, end);
      flushed_docs_.resize(committed_flushed_docs_);
    }
  }

  // rollback inserts located inside the writer
  auto& writer = *writer_;
  if (committed_buffered_docs_ == 0) {
    writer.reset();
    return;
  }
  const auto buffered_docs = writer.buffered_docs();
  for (auto doc_id = buffered_docs - 1 + doc_limits::min(),
            doc_id_rend = committed_buffered_docs_ - 1 + doc_limits::min();
       doc_id > doc_id_rend; --doc_id) {
    IRS_ASSERT(doc_limits::invalid() < doc_id);
    IRS_ASSERT(doc_id <= doc_limits::eof());
    writer.remove(static_cast<doc_id_t>(doc_id));
  }

  // If it will be first or last part of flushed segment in future,
  // we need valid ticks for this documents
  // TODO(MBkkt) Maybe we can just move docs_begin/end when FlushedSegment
  //  will be ready? Also what about assign last_committed_tick
  //  only for first and last value in range?
  const auto docs = writer_->docs_context();
  const auto last_committed_tick = docs[committed_buffered_docs_ - 1].tick;
  std::for_each(docs.begin() + committed_buffered_docs_, docs.end(),
                [&](auto& doc) {
                  doc.tick = last_committed_tick;
                  doc.query_id = writer_limits::kInvalidOffset;
                });

  committed_buffered_docs_ = buffered_docs;
}

void IndexWriter::SegmentContext::Commit(uint64_t queries, uint64_t last_tick) {
  IRS_ASSERT(last_tick < writer_limits::kMaxTick);
  IRS_ASSERT(queries <= last_tick);
  const auto first_tick = last_tick - queries;
  IRS_ASSERT(writer_limits::kMinTick < first_tick);

  auto update_tick = [&](auto& entry) noexcept { entry.tick += first_tick; };

  std::for_each(queries_.begin() + committed_queries_, queries_.end(),
                update_tick);
  committed_queries_ = queries_.size();

  std::for_each(flushed_docs_.begin() + committed_flushed_docs_,
                flushed_docs_.end(), update_tick);
  committed_flushed_docs_ = flushed_docs_.size();

  const auto docs = writer_->docs_context();
  std::for_each(docs.begin() + committed_buffered_docs_, docs.end(),
                update_tick);
  committed_buffered_docs_ = docs.size();

  if (first_tick_ == writer_limits::kMaxTick) {
    first_tick_ = first_tick;
  }
  IRS_ASSERT(last_tick_ <= last_tick);
  last_tick_ = last_tick;
}

IndexWriter::IndexWriter(
  ConstructToken, index_lock::ptr&& lock,
  index_file_refs::ref_t&& lock_file_ref, directory& dir, format::ptr codec,
  size_t segment_pool_size, const SegmentOptions& segment_limits,
  const Comparer* comparator, const ColumnInfoProvider& column_info,
  const FeatureInfoProvider& feature_info,
  const PayloadProvider& meta_payload_provider,
  std::shared_ptr<const DirectoryReaderImpl>&& committed_reader,
  const ResourceManagementOptions& rm)
  : feature_info_{feature_info},
    column_info_{column_info},
    meta_payload_provider_{meta_payload_provider},
    comparator_{comparator},
    codec_{std::move(codec)},
    dir_{dir},
    committed_reader_{std::move(committed_reader)},
    segment_limits_{segment_limits},
    segment_writer_pool_{segment_pool_size},
    seg_counter_{committed_reader_->Meta().index_meta.seg_counter},
    last_gen_{committed_reader_->Meta().index_meta.gen},
    writer_{codec_->get_index_meta_writer()},
    write_lock_{std::move(lock)},
    write_lock_file_ref_{std::move(lock_file_ref)},
    resource_manager_{rm} {
  IRS_ASSERT(column_info);   // ensured by 'make'
  IRS_ASSERT(feature_info);  // ensured by 'make'
  IRS_ASSERT(codec_);

  wand_scorers_ = committed_reader_->Options().scorers;
  for (auto* scorer : wand_scorers_) {
    scorer->get_features(wand_features_);
  }

  flush_context_.store(flush_context_pool_.data());

  // setup round-robin chain
  auto* ctx = flush_context_pool_.data();
  for (auto* last = ctx + flush_context_pool_.size() - 1; ctx != last; ++ctx) {
    ctx->dir_ = std::make_unique<RefTrackingDirectory>(dir);
    ctx->next_ = ctx + 1;
  }
  ctx->dir_ = std::make_unique<RefTrackingDirectory>(dir);
  ctx->next_ = flush_context_pool_.data();
}

void IndexWriter::InitMeta(IndexMeta& meta, uint64_t tick) const {
  if (meta_payload_provider_) {
    IRS_ASSERT(!meta.payload.has_value());
    auto& payload = meta.payload.emplace(bstring{});
    if (IRS_UNLIKELY(!meta_payload_provider_(tick, payload))) {
      meta.payload.reset();
    }
  }
  meta.seg_counter = CurrentSegmentId();  // Ensure counter() >= max(seg#)
  meta.gen = last_gen_;                   // Clone index metadata generation
}

void IndexWriter::Clear(uint64_t tick) {
  std::lock_guard commit_lock{commit_lock_};

  IRS_ASSERT(committed_reader_);
  if (const auto& committed_meta = committed_reader_->Meta();
      !pending_state_.Valid() && committed_meta.index_meta.segments.empty() &&
      !IsInitialCommit(committed_meta)) {
    return;  // Already empty
  }

  PendingContext to_commit{PendingBase{.tick = tick}};
  InitMeta(to_commit.meta, tick);

  to_commit.ctx = SwitchFlushContext();
  // Ensure there are no active struct update operations
  to_commit.ctx->pending_.Wait();

  Abort();  // iff Clear called between Begin and Commit
  ApplyFlush(std::move(to_commit));
  Finish();

  // Clear consolidating segments
  std::lock_guard lock{consolidation_lock_};
  consolidating_segments_.clear();
}

IndexWriter::ptr IndexWriter::Make(directory& dir, format::ptr codec,
                                   OpenMode mode,
                                   const IndexWriterOptions& options) {
  IRS_ASSERT(std::all_of(options.reader_options.scorers.begin(),
                         options.reader_options.scorers.end(),
                         [](const auto* v) { return v != nullptr; }));
  IRS_ASSERT(options.reader_options.resource_manager.cached_columns);
  IRS_ASSERT(options.reader_options.resource_manager.consolidations);
  IRS_ASSERT(options.reader_options.resource_manager.file_descriptors);
  IRS_ASSERT(options.reader_options.resource_manager.readers);
  IRS_ASSERT(options.reader_options.resource_manager.transactions);
  index_lock::ptr lock;
  index_file_refs::ref_t lock_ref;

  if (options.lock_repository) {
    // lock the directory
    lock = dir.make_lock(kWriteLockName);
    // will be created by try_lock
    lock_ref = dir.attributes().refs().add(kWriteLockName);

    if (!lock || !lock->try_lock()) {
      throw lock_obtain_failed(kWriteLockName);
    }
  }

  // read from directory or create index metadata
  DirectoryMeta meta;

  {
    auto reader = codec->get_index_meta_reader();
    const bool index_exists = reader->last_segments_file(dir, meta.filename);

    if (OM_CREATE == mode ||
        ((OM_CREATE | OM_APPEND) == mode && !index_exists)) {
      // for OM_CREATE meta must be fully recreated, meta read only to get
      // last version
      if (index_exists) {
        // Try to read. It allows us to create writer against an index that's
        // currently opened for searching
        reader->read(dir, meta.index_meta, meta.filename);

        meta.filename.clear();  // Empty index meta -> new index
        auto& index_meta = meta.index_meta;
        index_meta.payload.reset();
        index_meta.segments.clear();
      }
    } else if (!index_exists) {
      throw file_not_found{meta.filename};  // no segments file found
    } else {
      reader->read(dir, meta.index_meta, meta.filename);
    }
  }

  auto reader = [](directory& dir, format::ptr codec, DirectoryMeta&& meta,
                   const IndexReaderOptions& opts) {
    const auto& segments = meta.index_meta.segments;

    std::vector<SegmentReader> readers;
    readers.reserve(segments.size());

    for (auto& segment : segments) {
      // Segment reader holds refs to all segment files
      readers.emplace_back(dir, segment.meta, opts);
      IRS_ASSERT(readers.back());
    }

    return std::make_shared<const DirectoryReaderImpl>(
      dir, std::move(codec), opts, std::move(meta), std::move(readers));
  }(dir, codec, std::move(meta), options.reader_options);

  auto writer = std::make_shared<IndexWriter>(
    ConstructToken{}, std::move(lock), std::move(lock_ref), dir,
    std::move(codec), options.segment_pool_size, SegmentOptions{options},
    options.comparator,
    options.column_info ? options.column_info : kDefaultColumnInfo,
    options.features ? options.features : kDefaultFeatureInfo,
    options.meta_payload_provider, std::move(reader),
    options.reader_options.resource_manager);

  // Remove non-index files from directory
  directory_utils::RemoveAllUnreferenced(dir);

  return writer;
}

IndexWriter::~IndexWriter() noexcept {
  // Failure may indicate a dangling 'document' instance
  IRS_ASSERT(!segments_active_.load());
  write_lock_.reset();  // Reset write lock if any
  // Reset pending state (if any) before destroying flush contexts
  pending_state_.Reset(*this);
  flush_context_.store(nullptr);
  // Ensure all tracked segment_contexts are released before
  // segment_writer_pool_ is deallocated
  flush_context_pool_.clear();
}

uint64_t IndexWriter::BufferedDocs() const {
  uint64_t docs_in_ram = 0;
  auto ctx = GetFlushContext();
  // 'pending_used_segment_contexts_'/'pending_free_segment_contexts_'
  // may be modified
  std::lock_guard lock{ctx->pending_.Mutex()};

  for (const auto& entry : ctx->pending_segments_) {
    IRS_ASSERT(entry.segment_ != nullptr);
    // reading segment_writer::docs_count() is not thread safe
    docs_in_ram +=
      entry.segment_->buffered_docs_.load(std::memory_order_relaxed);
  }

  return docs_in_ram;
}

uint64_t IndexWriter::NextSegmentId() noexcept {
  return seg_counter_.fetch_add(1, std::memory_order_relaxed) + 1;
}

uint64_t IndexWriter::CurrentSegmentId() const noexcept {
  return seg_counter_.load(std::memory_order_relaxed);
}

ConsolidationResult IndexWriter::Consolidate(
  const ConsolidationPolicy& policy, format::ptr codec,
  const MergeWriter::FlushProgress& progress) {
  REGISTER_TIMER_DETAILED();
  if (!codec) {
    // use default codec if not specified
    codec = codec_;
  }

  Consolidation candidates;
  const auto run_id = reinterpret_cast<uintptr_t>(&candidates);

  decltype(committed_reader_) committed_reader;
  // collect a list of consolidation candidates
  {
    std::lock_guard lock{consolidation_lock_};
    // hold a reference to the last committed state to prevent files from being
    // deleted by a cleaner during the upcoming consolidation
    // use atomic_load(...) since Finish() may modify the pointer
    committed_reader =
      std::atomic_load_explicit(&committed_reader_, std::memory_order_acquire);
    IRS_ASSERT(committed_reader != nullptr);
    if (committed_reader->size() == 0) {
      // nothing to consolidate
      return {0, ConsolidationError::OK};
    }

    // FIXME TODO remove from 'consolidating_segments_' any segments in
    // 'committed_state_' or 'pending_state_' to avoid data duplication
    policy(candidates, *committed_reader, consolidating_segments_, consolidation_merge_or_cleanup_);

    // Alternate between merge and cleanup operations in consolidation
    // to achieve fairness.
    consolidation_merge_or_cleanup_ = !consolidation_merge_or_cleanup_;

    switch (candidates.size()) {
      case 0:  // nothing to consolidate
        return {0, ConsolidationError::OK};
      case 1: {
        const auto* candidate = candidates.front();
        IRS_ASSERT(candidate != nullptr);
        if (!HasRemovals(candidate->Meta())) {
          // no removals, nothing to consolidate
          return {0, ConsolidationError::OK};
        }
      }
    }

    for (const auto* candidate : candidates) {
      IRS_ASSERT(candidate != nullptr);
      // TODO(MBkkt) Make this check assert in future
      if (consolidating_segments_.contains(candidate->Meta().name)) {
        return {0, ConsolidationError::FAIL};
      }
    }

    // register for consolidation
    consolidating_segments_.reserve(consolidating_segments_.size() +
                                    candidates.size());
    for (const auto* candidate : candidates) {
      consolidating_segments_.emplace(candidate->Meta().name);
    }
  }

  // unregisterer for all registered candidates
  Finally unregister_segments = [&candidates, this]() noexcept {
    if (candidates.empty()) {
      return;
    }
    std::lock_guard lock{consolidation_lock_};
    for (const auto* candidate : candidates) {
      consolidating_segments_.erase(candidate->Meta().name);
    }
  };

  // validate candidates: no duplicates and all should be from committed reader
#ifdef IRESEARCH_DEBUG
  std::sort(candidates.begin(), candidates.end());
  IRS_ASSERT(std::unique(candidates.begin(), candidates.end()) ==
             candidates.end());
  {
    size_t found = 0;
    for (const auto& segment : *committed_reader) {
      found += static_cast<size_t>(
        std::binary_search(candidates.begin(), candidates.end(), &segment));
    }
    IRS_ASSERT(found == candidates.size());
  }
#endif

  IRS_LOG_TRACE(absl::StrCat("Starting consolidation id='", run_id, "':\n",
                             ToString(candidates)));

  // do lock-free merge

  ConsolidationResult result{candidates.size(), ConsolidationError::FAIL};

  IndexSegment consolidation_segment;
  consolidation_segment.meta.codec = codec;  // Should use new codec
  consolidation_segment.meta.version = 0;    // Reset version for new segment
  // Increment active meta
  consolidation_segment.meta.name = file_name(NextSegmentId());

  RefTrackingDirectory dir{dir_};  // Track references for new segment

  MergeWriter merger{dir, GetSegmentWriterOptions(true)};
  merger.Reset(candidates.begin(), candidates.end());

  // We do not persist segment meta since some removals may come later
  if (!merger.Flush(consolidation_segment.meta, progress)) {
    // Nothing to consolidate or consolidation failure
    return result;
  }

  auto pending_reader = SegmentReaderImpl::Open(
    dir_, consolidation_segment.meta, committed_reader->Options());

  if (!pending_reader) {
    throw index_error{
      absl::StrCat("Failed to open reader for consolidated segment '",
                   consolidation_segment.meta.name, "'")};
  }

  // Commit merge
  {
    // ensure committed_state_ segments are not modified by concurrent
    // consolidate()/commit()
    std::unique_lock lock{commit_lock_};
    const auto current_committed_reader = committed_reader_;
    IRS_ASSERT(current_committed_reader != nullptr);
    if (pending_state_.Valid()) {
      // check that we haven't added to reader cache already absent readers
      // only if we have different index meta
      if (committed_reader != current_committed_reader) {
        auto begin = current_committed_reader->begin();
        auto end = current_committed_reader->end();

        // pointers are different so check by name
        for (const auto* candidate : candidates) {
          if (end == std::find_if(
                       begin, end,
                       [candidate = std::string_view{candidate->Meta().name}](
                         const SubReader& s) {
                         // FIXME(gnusi): compare pointers?
                         return candidate == s.Meta().name;
                       })) {
            // not all candidates are valid
            IRS_LOG_DEBUG(absl::StrCat(
              "Failed to start consolidation for index generation '",
              committed_reader->Meta().index_meta.gen, "', not found segment ",
              candidate->Meta().name, " in committed state"));
            return result;
          }
        }
      }

      result.error = ConsolidationError::PENDING;

      // transaction has been started, we're somewhere in the middle

      // can modify ctx->segment_mask_ without
      // lock since have commit_lock_
      auto ctx = GetFlushContext();

      // register consolidation for the next transaction
      ctx->imports_.emplace_back(
        std::move(consolidation_segment),
        writer_limits::kMaxTick,      // skip removals, will accumulate
                                      // removals from existing candidates
        dir.GetRefs(),                // do not forget to track refs
        std::move(candidates),        // consolidation context candidates
        std::move(pending_reader),    // consolidated reader
        std::move(committed_reader),  // consolidation context meta
        std::move(merger));           // merge context

      IRS_LOG_TRACE(absl::StrCat("Consolidation id='", run_id,
                                 "' successfully finished: pending"));
    } else if (committed_reader == current_committed_reader) {
      // before new transaction was started:
      // no commits happened in since consolidation was started

      auto ctx = GetFlushContext();
      // lock due to context modification
      std::lock_guard ctx_lock{ctx->pending_.Mutex()};

      // can release commit lock, we guarded against commit by
      // locked flush context
      lock.unlock();

      auto& segment_mask = ctx->segment_mask_;

      // persist segment meta
      index_utils::FlushIndexSegment(dir, consolidation_segment);
      segment_mask.reserve(segment_mask.size() + candidates.size());
      const auto& pending_segment = ctx->imports_.emplace_back(
        std::move(consolidation_segment),
        writer_limits::kMinTick,      // removals must be applied to the
                                      // consolidated segment
        dir.GetRefs(),                // do not forget to track refs
        std::move(candidates),        // consolidation context candidates
        std::move(pending_reader),    // consolidated reader
        std::move(committed_reader),  // consolidation context meta
        *resource_manager_.consolidations);

      // filter out merged segments for the next commit
      const auto& consolidation_ctx = pending_segment.consolidation_ctx;
      const auto& consolidation_meta = pending_segment.segment.meta;

      // mask mapped candidates
      // segments from the to-be added new segment
      for (const auto* candidate : consolidation_ctx.candidates) {
        segment_mask.emplace(candidate->Meta().name);
      }

      IRS_LOG_TRACE(
        absl::StrCat("Consolidation id='", run_id,
                     "' successfully finished: Name='", consolidation_meta.name,
                     "', docs_count=", consolidation_meta.docs_count,
                     ", live_docs_count=", consolidation_meta.live_docs_count,
                     ", size=", consolidation_meta.byte_size));
    } else {
      // before new transaction was started:
      // there was a commit(s) since consolidation was started,

      auto ctx = GetFlushContext();
      // lock due to context modification
      std::lock_guard ctx_lock{ctx->pending_.Mutex()};

      // can release commit lock, we guarded against commit by
      // locked flush context
      lock.unlock();

      auto& segment_mask = ctx->segment_mask_;

      CandidatesMapping mappings;
      const auto [count, has_removals] =
        MapCandidates(mappings, candidates, *current_committed_reader);

      if (count != candidates.size()) {
        // at least one candidate is missing can't finish consolidation
        IRS_LOG_DEBUG(absl::StrCat(
          "Failed to finish consolidation id='", run_id, "' for segment '",
          consolidation_segment.meta.name, "', found only '", count,
          "' out of '", candidates.size(), "' candidates"));

        return result;
      }

      // handle removals if something changed
      if (has_removals) {
        DocumentMask docs_mask{{*resource_manager_.readers}};

        if (!MapRemovals(mappings, merger, docs_mask)) {
          // consolidated segment has docs missing from
          // current_committed_meta->segments()
          IRS_LOG_DEBUG(absl::StrCat("Failed to finish consolidation id='",
                                     run_id, "' for segment '",
                                     consolidation_segment.meta.name,
                                     "', due removed documents still present "
                                     "the consolidation candidates"));

          return result;
        }

        if (!docs_mask.empty()) {
          WriteDocumentMask(dir, consolidation_segment.meta, docs_mask, false);

          // Reopen modified reader
          pending_reader = pending_reader->ReopenDocsMask(
            dir, consolidation_segment.meta, std::move(docs_mask));
        }
      }

      // persist segment meta
      index_utils::FlushIndexSegment(dir, consolidation_segment);
      segment_mask.reserve(segment_mask.size() + candidates.size());
      const auto& pending_segment = ctx->imports_.emplace_back(
        std::move(consolidation_segment),
        writer_limits::kMinTick,      // removals must be applied to the
                                      // consolidated segment
        dir.GetRefs(),                // do not forget to track refs
        std::move(candidates),        // consolidation context candidates
        std::move(pending_reader),    // consolidated reader
        std::move(committed_reader),  // consolidation context meta
        *resource_manager_.consolidations);

      // filter out merged segments for the next commit
      const auto& consolidation_ctx = pending_segment.consolidation_ctx;
      const auto& consolidation_meta = pending_segment.segment.meta;

      // mask mapped candidates
      // segments from the to-be added new segment
      for (const auto* candidate : consolidation_ctx.candidates) {
        segment_mask.emplace(candidate->Meta().name);
      }

      // mask mapped (matched) segments
      // segments from the already finished commit
      for (const auto& segment : *current_committed_reader) {
        if (mappings.contains(segment.Meta().name)) {
          segment_mask.emplace(segment.Meta().name);
        }
      }

      IRS_LOG_TRACE(absl::StrCat(
        "Consolidation id='", run_id, "' successfully finished:\nName='",
        consolidation_meta.name,
        "', docs_count=", consolidation_meta.docs_count,
        ", live_docs_count=", consolidation_meta.live_docs_count,
        ", size=", consolidation_meta.byte_size));
    }
  }

  result.error = ConsolidationError::OK;
  return result;
}

bool IndexWriter::Import(const IndexReader& reader,
                         format::ptr codec /*= nullptr*/,
                         const MergeWriter::FlushProgress& progress /*= {}*/) {
  if (!reader.live_docs_count()) {
    return true;  // Skip empty readers since no documents to import
  }

  if (!codec) {
    codec = codec_;
  }

  const auto options = [&] {
    const auto committed_reader =
      std::atomic_load_explicit(&committed_reader_, std::memory_order_acquire);
    IRS_ASSERT(committed_reader != nullptr);
    return committed_reader->Options();
  }();

  RefTrackingDirectory dir{dir_};  // Track references

  IndexSegment segment;
  segment.meta.name = file_name(NextSegmentId());
  segment.meta.codec = codec;

  MergeWriter merger{dir, GetSegmentWriterOptions(true)};
  merger.Reset(reader.begin(), reader.end());

  if (!merger.Flush(segment.meta, progress)) {
    return false;  // Import failure (no files created, nothing to clean up)
  }

  auto imported_reader = SegmentReaderImpl::Open(dir_, segment.meta, options);

  if (!imported_reader) {
    throw index_error{absl::StrCat(
      "Failed to open reader for imported segment '", segment.meta.name, "'")};
  }

  index_utils::FlushIndexSegment(dir, segment);

  auto refs = dir.GetRefs();

  auto flush = GetFlushContext();
  // lock due to context modification
  std::lock_guard lock{flush->pending_.Mutex()};

  // IMPORTANT NOTE!
  // Will be committed in the upcoming Commit
  // even if tick is greater than Commit tick
  // TODO(MBkkt) Can be fixed: needs to add overload with external tick and
  // moving not suited import segments to the next FlushContext in PrepareFlush
  flush->imports_.emplace_back(
    std::move(segment), tick_.load(std::memory_order_relaxed), std::move(refs),
    std::move(imported_reader),
    resource_manager_);  // do not forget to track refs

  return true;
}

IndexWriter::FlushContextPtr IndexWriter::GetFlushContext() const noexcept {
  auto* ctx = flush_context_.load(std::memory_order_relaxed);
  for (;;) {
    std::shared_lock lock{ctx->context_mutex_, std::try_to_lock};
    if (auto* new_ctx = flush_context_.load(std::memory_order_relaxed);
        !lock || ctx != new_ctx) {
      ctx = new_ctx;
      continue;
    }
    lock.release();
    return {ctx, [](FlushContext* ctx) noexcept {
              ctx->context_mutex_.unlock_shared();
            }};
  }
}

IndexWriter::FlushContextPtr IndexWriter::SwitchFlushContext() noexcept {
  auto* ctx = flush_context_.load(std::memory_order_relaxed);
  for (;;) {
    std::unique_lock lock{ctx->context_mutex_};
    if (!flush_context_.compare_exchange_strong(ctx, ctx->next_,
                                                std::memory_order_relaxed)) {
      continue;
    }
    lock.release();
    return {ctx, [](FlushContext* ctx) noexcept {
              ctx->Reset();  // reset context and make ready for reuse
              ctx->context_mutex_.unlock();
            }};
  }
}

IndexWriter::ActiveSegmentContext IndexWriter::GetSegmentContext() try {
  // TODO(MBkkt) rewrite this when will be written parallel Commit
  //  Few ideas about rewriting:
  //  1. We should use all available memory
  //  2. Flush should be async, waiting Flush only if we don't have other choice
  //  3. segment_memory/count_max should be removed in their current state
  // increment counter to acquire reservation,
  // if another thread tries to reserve last context then it'll be over limit
  const auto segments_active =
    segments_active_.fetch_add(1, std::memory_order_relaxed) + 1;

  // no free segment_context available and maximum number of segments reached
  // must return to caller so as to unlock/relock flush_context before retrying
  // to get a new segment so as to avoid a deadlock due to a read-write-read
  // situation for FlushContext::context_mutex_ with threads trying to lock
  // FlushContext::context_mutex_ to return their segment_context
  if (const auto segment_count_max =
        segment_limits_.segment_count_max.load(std::memory_order_relaxed);
      segment_count_max < segments_active) {
    segments_active_.fetch_sub(1, std::memory_order_relaxed);
    return {};
  }

  {
    auto flush = GetFlushContext();
    auto* freelist_node = flush->pending_freelist_.pop();
    if (freelist_node != nullptr) {
      flush->pending_.Add();
      return {static_cast<PendingSegmentContext*>(freelist_node)->segment_,
              segments_active_, flush.get(), freelist_node->value};
    }
  }

  const auto options = GetSegmentWriterOptions(false);

  // should allocate a new segment_context from the pool
  std::shared_ptr<SegmentContext> segment_ctx = segment_writer_pool_.emplace(
    dir_,
    [this] {
      SegmentMeta meta{.codec = codec_};
      meta.name = file_name(NextSegmentId());
      return meta;
    },
    options);

  // recreate writer if it reserved more memory than allowed by current limits
  if (auto segment_memory_max = segment_limits_.segment_memory_max.load();
      segment_memory_max < segment_ctx->writer_->memory_reserved()) {
    segment_ctx->writer_ = segment_writer::make(segment_ctx->dir_, options);
  }

  return {segment_ctx, segments_active_};
} catch (...) {
  segments_active_.fetch_sub(1, std::memory_order_relaxed);
  throw;
}

SegmentWriterOptions IndexWriter::GetSegmentWriterOptions(
  bool consolidation) const noexcept {
  return {
    .column_info = column_info_,
    .feature_info = feature_info_,
    .scorers_features = wand_features_,
    .scorers = wand_scorers_,
    .comparator = comparator_,
    .resource_manager = consolidation ? *resource_manager_.consolidations
                                      : *resource_manager_.transactions,
  };
}

IndexWriter::PendingContext IndexWriter::PrepareFlush(const CommitInfo& info) {
  REGISTER_TIMER_DETAILED();

  const auto tick = info.tick;
  IRS_ASSERT(writer_limits::kMinTick < tick);
  IRS_ASSERT(committed_tick_ <= tick);
  IRS_ASSERT(tick <= writer_limits::kMaxTick);

  // noexcept block: I'm not sure is it really necessary or not
  auto ctx = SwitchFlushContext();
  // ensure there are no active struct update operations
  ctx->pending_.Wait();
  // Stage 0
  // wait for any outstanding segments to settle to ensure that any rollbacks
  // are properly tracked in 'modification_queries_'
  const auto flushed_tick = ctx->FlushPending(committed_tick_, tick);

  std::unique_lock cleanup_lock{consolidation_lock_, std::defer_lock};
  Finally cleanup = [&]() noexcept {
    if (ctx == nullptr) {
      return;
    }
    if (!cleanup_lock.owns_lock()) {
      cleanup_lock.lock();
    }
    Cleanup(*ctx);
  };

  const auto& progress =
    (info.progress != nullptr ? info.progress : kNoProgress);

  IndexMeta pending_meta;
  std::vector<PartialSync> partial_sync;
  std::vector<SegmentReader> readers;

  auto& dir = *ctx->dir_;
  const auto& committed_reader = *committed_reader_;
  const auto& committed_meta = committed_reader.Meta();
  auto reader_options = committed_reader.Options();
  // We never need to read doc_mask here
  reader_options.doc_mask = false;

  // If there is no index we shall initialize it
  bool modified = IsInitialCommit(committed_meta);

  auto apply_queries = [&](SegmentContext& segment, const auto& func) {
    // TODO(MBkkt) binary search for begin?
    for (auto& query : segment.queries_) {
      if (query.tick <= committed_tick_) {
        continue;  // skip queries from previous Commit
      }
      if (tick < query.tick) {
        break;  // skip queries from next Commit
      }
      func(query);
    }
  };
  auto apply_all_queries = [&](const auto& func) {
    for (auto& segment : ctx->segments_) {
      IRS_ASSERT(segment != nullptr);
      apply_queries(*segment, func);
    }
  };

  // Stage 1
  // update document_mask for existing (i.e. sealed) segments
  auto& segment_mask = ctx->segment_mask_;
  segment_mask.reserve(segment_mask.size() + ctx->cached_.size());
  for (const auto& entry : ctx->cached_) {
    segment_mask.emplace(entry.second->Meta().name);
  }

  size_t current_segment_index = 0;
  const size_t committed_reader_size = committed_reader.size();

  readers.reserve(committed_reader_size);
  pending_meta.segments.reserve(committed_reader_size);

  for (DocumentMask deleted_docs{{*resource_manager_.transactions}};
       const auto& existing_segment : committed_reader.GetReaders()) {
    auto& index_segment =
      committed_meta.index_meta.segments[current_segment_index];
    progress("Stage 1: Apply removals to the existing segments",
             current_segment_index++, committed_reader_size);

    // skip already masked segments
    if (segment_mask.contains(existing_segment->Meta().name)) {
      continue;
    }

    // We don't want to call clear here because even for empty map it costs O(n)
    IRS_ASSERT(deleted_docs.empty());

    // mask documents matching filters from segment_contexts
    // (i.e. from new operations)
    apply_all_queries([&](QueryContext& query) {
      // FIXME(gnusi): optimize PK queries
      RemoveFromExistingSegment(deleted_docs, query, existing_segment);
    });

    // Write docs_mask if masks added
    if (const size_t num_removals = deleted_docs.size(); num_removals) {
      // If all docs are masked then mask segment
      if (existing_segment.live_docs_count() == num_removals) {
        deleted_docs.clear();
        // It's important to mask empty segment to rollback
        // the affected consolidations

        segment_mask.emplace(existing_segment->Meta().name);
        modified = true;
        continue;
      }

      // Append removals
      IRS_ASSERT(existing_segment.docs_mask());
      auto docs_mask = *existing_segment.docs_mask();
      docs_mask.merge(deleted_docs);
      deleted_docs.clear();

      IndexSegment segment{.meta = index_segment.meta};

      const auto mask_file_index =
        WriteDocumentMask(dir, segment.meta, docs_mask);
      index_utils::FlushIndexSegment(dir, segment);  // Write with new mask
      partial_sync.emplace_back(readers.size(), mask_file_index);

      auto new_segment = existing_segment.GetImpl()->ReopenDocsMask(
        dir, segment.meta, std::move(docs_mask));
      readers.emplace_back(std::move(new_segment));
      pending_meta.segments.emplace_back(std::move(segment));
    } else {
      readers.emplace_back(existing_segment.GetImpl());
      pending_meta.segments.emplace_back(index_segment);
    }
  }

  // Stage 2
  // Add pending complete segments registered by import or consolidation

  // Number of candidates that have been registered for pending consolidation
  size_t current_imports_index = 0;
  size_t import_candidates_count = 0;
  size_t partial_sync_threshold = readers.size();

  for (auto& import : ctx->imports_) {
    progress("Stage 2: Handling consolidated/imported segments",
             current_imports_index++, ctx->imports_.size());

    IRS_ASSERT(import.reader);  // Ensured by Consolidation/Import
    auto& meta = import.segment.meta;
    auto& import_reader = import.reader;
    auto import_docs_mask = *import_reader->docs_mask();  // Intentionally copy

    bool docs_mask_modified = false;

    const ConsolidationView candidates{import.consolidation_ctx.candidates};

    const auto pending_consolidation =
      static_cast<bool>(import.consolidation_ctx.merger);

    if (pending_consolidation) {
      // Pending consolidation request
      CandidatesMapping mappings;
      const auto [count, has_removals] =
        MapCandidates(mappings, candidates, readers);

      if (count != candidates.size()) {
        // At least one candidate is missing in pending meta can't finish
        // consolidation
        IRS_LOG_DEBUG(absl::StrCat(
          "Failed to finish merge for segment '", meta.name, "', found only '",
          count, "' out of '", candidates.size(), "' candidates"));

        continue;  // Skip this particular consolidation
      }

      // Mask mapped candidates segments from the to-be added new segment
      for (const auto& mapping : mappings) {
        const auto* reader = mapping.second.old.segment;
        IRS_ASSERT(reader);
        segment_mask.emplace(reader->Meta().name);
      }

      // Mask mapped (matched) segments from the currently ongoing commit
      for (const auto& segment : readers) {
        if (mappings.contains(segment.Meta().name)) {
          // Important to store the address of implementation
          segment_mask.emplace(segment.Meta().name);
        }
      }

      // Have some changes, apply removals
      if (has_removals) {
        const auto success = MapRemovals(
          mappings, import.consolidation_ctx.merger, import_docs_mask);

        if (!success) {
          // Consolidated segment has docs missing from 'segments'
          IRS_LOG_WARN(absl::StrCat("Failed to finish merge for segment '",
                                    meta.name,
                                    "', due to removed documents still present "
                                    "the consolidation candidates"));

          continue;  // Skip this particular consolidation
        }

        // We're done with removals for pending consolidation
        // they have been already applied to candidates above
        // and successfully remapped to consolidated segment
        docs_mask_modified |= true;
      }

      // We've seen at least 1 successfully applied
      // pending consolidation request
      import_candidates_count += candidates.size();
    } else {
      // During consolidation doc_mask could be already populated even for just
      // merged segment. Pending already imported/consolidated segment, apply
      // removals mask documents matching filters from segment_contexts
      // (i.e. from new operations)
      apply_all_queries([&](QueryContext& query) {
        // skip queries which not affect this
        if (import.tick <= query.tick) {
          // FIXME(gnusi): optimize PK queries
          docs_mask_modified |=
            RemoveFromImportedSegment(import_docs_mask, query, *import_reader);
        }
      });
    }

    // Skip empty segments
    if (meta.docs_count <= import_docs_mask.size()) {
      IRS_ASSERT(meta.docs_count == import_docs_mask.size());
      modified = true;  // FIXME(gnusi): looks strange
      continue;
    }

    // Write non-empty document mask
    if (docs_mask_modified) {
      WriteDocumentMask(dir, meta, import_docs_mask, !pending_consolidation);

      // Reopen modified reader
      import_reader =
        import_reader->ReopenDocsMask(dir, meta, std::move(import_docs_mask));
    }

    // Persist segment meta
    if (docs_mask_modified || pending_consolidation) {
      index_utils::FlushIndexSegment(dir, import.segment);
    }

    readers.emplace_back(std::move(import_reader));
    pending_meta.segments.emplace_back(std::move(import.segment));
  }

  // For pending consolidation we need to filter out consolidation
  // candidates after applying them
  if (import_candidates_count != 0) {
    IRS_ASSERT(import_candidates_count <= readers.size());
    const size_t count = readers.size() - import_candidates_count;
    std::vector<SegmentReader> tmp_readers;
    tmp_readers.reserve(count);
    IndexMeta tmp_meta;
    tmp_meta.segments.reserve(count);
    std::vector<PartialSync> tmp_partial_sync;

    auto partial_sync_begin = partial_sync.begin();
    for (size_t i = 0; i < partial_sync_threshold; ++i) {
      if (auto& segment = readers[i];
          !segment_mask.contains(segment->Meta().name)) {
        partial_sync_begin =
          std::find_if(partial_sync_begin, partial_sync.end(),
                       [i](const auto& v) { return i == v.segment_index; });
        if (partial_sync_begin != partial_sync.end()) {
          tmp_partial_sync.emplace_back(tmp_readers.size(),
                                        partial_sync_begin->file_index);
        }
        tmp_readers.emplace_back(std::move(segment));
        tmp_meta.segments.emplace_back(std::move(pending_meta.segments[i]));
      }
    }
    const auto tmp_partial_sync_threshold = tmp_readers.size();

    tmp_readers.insert(
      tmp_readers.end(),
      std::make_move_iterator(readers.begin() + partial_sync_threshold),
      std::make_move_iterator(readers.end()));
    tmp_meta.segments.insert(
      tmp_meta.segments.end(),
      std::make_move_iterator(pending_meta.segments.begin() +
                              partial_sync_threshold),
      std::make_move_iterator(pending_meta.segments.end()));

    partial_sync_threshold = tmp_partial_sync_threshold;
    partial_sync = std::move(tmp_partial_sync);
    readers = std::move(tmp_readers);
    pending_meta = std::move(tmp_meta);
  }

  auto& curr_cached = ctx->cached_;
  auto& next_cached = ctx->next_->cached_;

  IRS_ASSERT(pending_meta.segments.size() == readers.size());
  if (info.reopen_columnstore) {
    auto it = pending_meta.segments.begin();
    for (auto& reader : readers) {
      auto impl =
        reader.GetImpl()->ReopenColumnStore(dir, it->meta, reader_options);
      reader = SegmentReader{std::move(impl)};
      ++it;
    }
    curr_cached.clear();
    next_cached.clear();
  }

  // Stage 3
  // create new segments
  {
    // count total number of segments once
    size_t total_flushed_segments = 0;
    size_t current_flushed_segments = 0;
    for (const auto& segment : ctx->segments_) {
      IRS_ASSERT(segment != nullptr);
      // TODO(MBkkt) precise count?
      total_flushed_segments += segment->flushed_.size();
    }

    std::vector<FlushedSegmentContext> segment_ctxs;
    // TODO(MBkkt) reserve
    // process all segments that have been seen by the current flush_context
    for (const auto& segment : ctx->segments_) {
      IRS_ASSERT(segment != nullptr);

      // was updated after flush
      IRS_ASSERT(segment->committed_buffered_docs_ == 0);
      IRS_ASSERT(segment->committed_flushed_docs_ ==
                 segment->flushed_docs_.size());
      // process individually each flushed SegmentMeta from the SegmentContext
      for (auto& flushed : segment->flushed_) {
        IRS_ASSERT(flushed.GetDocsBegin() < flushed.GetDocsEnd());
        const auto flushed_first_tick =
          segment->flushed_docs_[flushed.GetDocsBegin()].tick;
        const auto flushed_last_tick =
          segment->flushed_docs_[flushed.GetDocsEnd() - 1].tick;
        IRS_ASSERT(flushed_first_tick <= flushed_last_tick);

        if (flushed_last_tick <= committed_tick_) {
          continue;  // skip flushed from previous Commit
        }
        if (tick < flushed_first_tick) {
          break;  // skip flushed from next Commit
        }
        progress("Stage 3: Creating new/reopen old segments",
                 current_flushed_segments++, total_flushed_segments);

        IRS_ASSERT(flushed.meta.live_docs_count != 0);
        IRS_ASSERT(flushed.meta.live_docs_count <= flushed.meta.docs_count);

        std::shared_ptr<const SegmentReaderImpl> reader;
        if (auto it = curr_cached.find(&flushed); it != curr_cached.end()) {
          IRS_ASSERT(it->second != nullptr);
          // We don't support case when segment is committed partially more than
          // one time. Because it's useless and ineffective.
          IRS_ASSERT(flushed_last_tick <= tick);
          // reuse existing reader with initial meta and docs_mask
          reader = it->second->ReopenDocsMask(
            dir, flushed.meta, DocumentMask{*resource_manager_.readers});
        } else {
          reader = SegmentReaderImpl::Open(dir, flushed.meta, reader_options);
        }

        if (!reader) {
          throw index_error{absl::StrCat(
            "while adding document mask modified records to "
            "flush_segment_context of segment '",
            flushed.meta.name, "', error: failed to open segment")};
        }

        if (tick < flushed_last_tick) {
          next_cached[&flushed] = reader;
        }

        auto& segment_ctx = segment_ctxs.emplace_back(
          std::move(reader), *segment, flushed, resource_manager_);

        // mask documents matching filters from all flushed segment_contexts
        // (i.e. from new operations)
        apply_all_queries([&](QueryContext& query) {
          // skip queries which not affect this FlushedSegment
          if (flushed_first_tick <= query.tick) {
            // FIXME(gnusi): optimize PK queries
            segment_ctx.Remove(query);
          }
        });
      }
    }

    // write docs_mask if !empty(), if all docs are masked then remove segment
    // altogether
    size_t current_segment_ctxs = 0;
    for (auto& segment_ctx : segment_ctxs) {
      // note: from the code, we are still a part of 'Stage 3',
      // but we need to report something different here, i.e. 'Stage 4'
      progress("Stage 4: Applying removals for new segments",
               current_segment_ctxs++, segment_ctxs.size());

      if (segment_ctx.segment.has_replace_) {
        segment_ctx.MaskUnusedReplace(committed_tick_, tick);
      }
      DocumentMask document_mask{{*resource_manager_.readers}};
      IndexSegment new_segment;
      if (segment_ctx.MakeDocumentMask(tick, document_mask, new_segment)) {
        modified |= segment_ctx.flushed.was_flush;
        continue;
      }
      IRS_ASSERT(segment_ctx.flushed.meta.version == new_segment.meta.version);
      const bool need_flush =
        segment_ctx.flushed.was_flush || !document_mask.empty();
      segment_ctx.flushed.was_flush = true;
      if (need_flush) {  // TODO(MBkkt) should be 1
        new_segment.meta.version += 2;
        segment_ctx.flushed.meta.version += 2;
      }
      if (!document_mask.empty()) {
        WriteDocumentMask(dir, new_segment.meta, document_mask, false);
      }
      index_utils::FlushIndexSegment(dir, new_segment);
      if (need_flush) {
        segment_ctx.reader = segment_ctx.reader->ReopenDocsMask(
          dir, new_segment.meta, std::move(document_mask));
      }
      readers.emplace_back(std::move(segment_ctx.reader));
      pending_meta.segments.emplace_back(std::move(new_segment));
    }
  }

#ifdef IRESEARCH_DEBUG
  {
    std::vector<std::string_view> filenames;
    filenames.reserve(pending_meta.segments.size());
    for (const auto& meta : pending_meta.segments) {
      // cppcheck-suppress useStlAlgorithm
      filenames.emplace_back(meta.filename);
    }
    std::sort(filenames.begin(), filenames.end());
    auto it = std::unique(filenames.begin(), filenames.end());
    IRS_ASSERT(it == filenames.end());
  }
#endif

  // TODO(MBkkt) In general looks useful to iterate here over all segments which
  //  partially committed, and free query memory which already was applied.
  //  But when I start thinking about rollback stuff it looks almost impossible

  auto files_to_sync =
    GetFilesToSync(pending_meta.segments, partial_sync, partial_sync_threshold);

  modified |= !files_to_sync.empty();

  // only flush a new index version upon a new index or a metadata change
  if (!modified) {
    IRS_ASSERT(readers.size() == committed_reader_size);
    if (info.reopen_columnstore) {
      auto new_reader = std::make_shared<const DirectoryReaderImpl>(
        committed_reader.Dir(), committed_reader.Codec(),
        committed_reader.Options(), DirectoryMeta{committed_reader.Meta()},
        std::move(readers));
      std::atomic_store_explicit(&committed_reader_, std::move(new_reader),
                                 std::memory_order_release);
    }
    return {};
  }

  InitMeta(pending_meta, tick == writer_limits::kMaxTick ? flushed_tick : tick);

  if (!next_cached.empty()) {
    cleanup_lock.lock();
    consolidating_segments_.reserve(consolidating_segments_.size() +
                                    next_cached.size());
    for (const auto& entry : next_cached) {
      consolidating_segments_.emplace(entry.second->Meta().name);
    }
    cleanup_lock.unlock();
  }

  return {
    PendingBase{
      .ctx = std::move(ctx),  // Retain flush context reference
      .tick = tick == writer_limits::kMaxTick ? committed_tick_ : tick},
    std::move(pending_meta),  // Retain meta pending flush
    std::move(readers),
    std::move(files_to_sync),
  };
}

void IndexWriter::ApplyFlush(PendingContext&& context) {
  IRS_ASSERT(!pending_state_.Valid());
  IRS_ASSERT(context.ctx);
  IRS_ASSERT(context.ctx->dir_ != nullptr);

  RefTrackingDirectory& dir = *context.ctx->dir_;

  std::string index_meta_file;
  DirectoryMeta to_commit{.index_meta = std::move(context.meta)};

  // Execute 1st phase of index meta transaction
  if (!writer_->prepare(dir, to_commit.index_meta, to_commit.filename,
                        index_meta_file)) {
    throw illegal_state{absl::StrCat(
      "Failed to write index metadata for segment '", index_meta_file, "'.")};
  }

  // The 1st phase of the transaction successfully finished here,
  // ensure we rollback changes if something goes wrong afterwards
  Finally update_generation = [this,
                               new_gen = to_commit.index_meta.gen]() noexcept {
    if (IRS_UNLIKELY(!pending_state_.Valid())) {
      writer_->rollback();  // Rollback failed transaction
    }

    // Ensure writer's generation is updated
    last_gen_ = new_gen;
  };

  context.files_to_sync.emplace_back(to_commit.filename);

  if (!dir.sync(context.files_to_sync)) {
    throw io_error{absl::StrCat("Failed to sync files for segment '",
                                index_meta_file, "'.")};
  }

  // Update file name so that directory reader holds a reference
  to_commit.filename = std::move(index_meta_file);
  // Assemble directory reader
  pending_state_.commit = std::make_shared<const DirectoryReaderImpl>(
    dir, codec_, committed_reader_->Options(), std::move(to_commit),
    std::move(context.readers));
  IRS_ASSERT(context.ctx);
  static_cast<PendingBase&>(pending_state_) = std::move(context);
  IRS_ASSERT(pending_state_.Valid());
}

bool IndexWriter::Start(const CommitInfo& info) {
  IRS_ASSERT(!commit_lock_.try_lock());  // already locked

  REGISTER_TIMER_DETAILED();

  if (pending_state_.Valid()) {
    // Begin has been already called without corresponding call to commit
    return false;
  }

  auto to_commit = PrepareFlush(info);

  if (to_commit.Empty()) {
    // Nothing to commit, no transaction started
    committed_tick_ =
      info.tick == writer_limits::kMaxTick ? committed_tick_ : info.tick;
    return false;
  }
  Finally cleanup = [&]() noexcept {
    if (!to_commit.Empty()) {
      std::ignore = to_commit.StartReset(*this);
    }
  };

  // TODO(MBkkt) error here means we don't remove cached from consolidating
  ApplyFlush(std::move(to_commit));

  return true;
}

void IndexWriter::Finish() {
  IRS_ASSERT(!commit_lock_.try_lock());  // Already locked

  REGISTER_TIMER_DETAILED();

  if (!pending_state_.Valid()) {
    return;
  }

  Finally cleanup = [&]() noexcept {
    Abort();  // after FinishReset it's noop
  };

  if (IRS_UNLIKELY(!writer_->commit())) {
    throw illegal_state{"Failed to commit index metadata."};
  }

  // noexcept part!
  auto lock = pending_state_.StartReset(*this, true);
  IRS_ASSERT(pending_state_.tick != writer_limits::kMaxTick);
  committed_tick_ = pending_state_.tick;
  // after this line transaction is successful (only noexcept operations below)
  std::atomic_store_explicit(&committed_reader_,
                             std::move(pending_state_.commit),
                             std::memory_order_release);
  pending_state_.FinishReset();
}

void IndexWriter::Abort() noexcept {
  IRS_ASSERT(!commit_lock_.try_lock());  // already locked

  if (!pending_state_.Valid()) {
    return;  // There is no open transaction
  }

  writer_->rollback();
  pending_state_.Reset(*this);
}

}  // namespace irs
