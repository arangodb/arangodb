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

#include "merge_writer.hpp"

#include "utils/assert.hpp"
#include "utils/string.hpp"

#if defined(IRESEARCH_DEBUG) && !defined(__clang__)
#include <ranges>
#endif

#include "analysis/token_attributes.hpp"
#include "index/buffered_column.hpp"
#include "index/buffered_column_iterator.hpp"
#include "index/comparer.hpp"
#include "index/field_meta.hpp"
#include "index/heap_iterator.hpp"
#include "index/index_meta.hpp"
#include "index/norm.hpp"
#include "index/segment_reader.hpp"
#include "store/store_utils.hpp"
#include "utils/directory_utils.hpp"
#include "utils/log.hpp"
#include "utils/memory.hpp"
#include "utils/timer_utils.hpp"
#include "utils/type_limits.hpp"

#include <absl/container/flat_hash_map.h>
#include <absl/strings/internal/resize_uninitialized.h>

namespace irs {
namespace {

bool IsSubsetOf(const feature_map_t& lhs, const feature_map_t& rhs) noexcept {
  for (auto& entry : lhs) {
    if (!rhs.count(entry.first)) {
      return false;
    }
  }
  return true;
}

void AccumulateFeatures(feature_set_t& accum, const feature_map_t& features) {
  for (auto& entry : features) {
    accum.emplace(entry.first);
  }
}

// mapping of old doc_id to new doc_id (reader doc_ids are sequential 0 based)
// masked doc_ids have value of MASKED_DOC_ID
using doc_id_map_t = ManagedVector<doc_id_t>;

// document mapping function
using doc_map_f = std::function<doc_id_t(doc_id_t)>;

using field_meta_map_t =
  absl::flat_hash_map<std::string_view, const field_meta*>;

class NoopDirectory : public directory {
 public:
  static NoopDirectory& instance() {
    static NoopDirectory INSTANCE;
    return INSTANCE;
  }

  directory_attributes& attributes() noexcept final { return attrs_; }

  index_output::ptr create(std::string_view) noexcept final { return nullptr; }

  bool exists(bool&, std::string_view) const noexcept final { return false; }

  bool length(uint64_t&, std::string_view) const noexcept final {
    return false;
  }

  index_lock::ptr make_lock(std::string_view) noexcept final { return nullptr; }

  bool mtime(std::time_t&, std::string_view) const noexcept final {
    return false;
  }

  index_input::ptr open(std::string_view, IOAdvice) const noexcept final {
    return nullptr;
  }

  bool remove(std::string_view) noexcept final { return false; }

  bool rename(std::string_view, std::string_view) noexcept final {
    return false;
  }

  bool sync(std::span<const std::string_view>) noexcept final { return false; }

  bool visit(const directory::visitor_f&) const final { return false; }

 private:
  NoopDirectory() = default;

  directory_attributes attrs_;
};

class ProgressTracker {
 public:
  explicit ProgressTracker(const MergeWriter::FlushProgress& progress,
                           size_t count) noexcept
    : progress_(&progress), count_(count) {
    IRS_ASSERT(progress);
  }

  bool operator()() {
    if (hits_++ >= count_) {
      hits_ = 0;
      valid_ = (*progress_)();
    }

    return valid_;
  }

  explicit operator bool() const noexcept { return valid_; }

 private:
  const MergeWriter::FlushProgress* progress_;
  const size_t count_;  // call progress callback each `count_` hits
  size_t hits_{0};      // current number of hits
  bool valid_{true};
};

class RemappingDocIterator : public doc_iterator {
 public:
  RemappingDocIterator(doc_iterator::ptr&& it, const doc_map_f& mapper) noexcept
    : it_{std::move(it)}, mapper_{&mapper}, src_{irs::get<document>(*it_)} {
    IRS_ASSERT(it_ && src_);
  }

  bool next() final;

  doc_id_t value() const noexcept final { return doc_.value; }

  doc_id_t seek(doc_id_t target) final {
    irs::seek(*this, target);
    return value();
  }

  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return irs::type<irs::document>::id() == type ? &doc_
                                                  : it_->get_mutable(type);
  }

 private:
  doc_iterator::ptr it_;
  const doc_map_f* mapper_;
  const irs::document* src_;
  irs::document doc_;
};

bool RemappingDocIterator::next() {
  while (it_->next()) {
    doc_.value = (*mapper_)(src_->value);

    if (doc_limits::eof(doc_.value)) {
      continue;  // masked doc_id
    }

    return true;
  }

  return false;
}

// Iterator over doc_ids for a term over all readers
class CompoundDocIterator : public doc_iterator {
 public:
  using doc_iterator_t =
    std::pair<doc_iterator::ptr, std::reference_wrapper<const doc_map_f>>;
  using iterators_t = std::vector<doc_iterator_t>;

  static constexpr auto kProgressStepDocs = size_t{1} << size_t{14};

  explicit CompoundDocIterator(
    const MergeWriter::FlushProgress& progress) noexcept
    : progress_(progress, kProgressStepDocs) {}

  template<typename Func>
  bool reset(Func&& func) {
    if (!func(iterators_)) {
      return false;
    }

    doc_.value = doc_limits::invalid();
    current_itr_ = 0;

    return true;
  }

  size_t size() const noexcept { return iterators_.size(); }

  bool aborted() const noexcept { return !static_cast<bool>(progress_); }

  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    if (irs::type<irs::document>::id() == type) {
      return &doc_;
    }

    return irs::type<attribute_provider_change>::id() == type
             ? &attribute_change_
             : nullptr;
  }

  bool next() final;

  doc_id_t seek(doc_id_t target) final {
    irs::seek(*this, target);
    return value();
  }

  doc_id_t value() const noexcept final { return doc_.value; }

 private:
  friend class SortingCompoundDocIterator;

  attribute_provider_change attribute_change_;
  std::vector<doc_iterator_t> iterators_;
  size_t current_itr_{0};
  ProgressTracker progress_;
  document doc_;
};

bool CompoundDocIterator::next() {
  progress_();

  if (aborted()) {
    doc_.value = doc_limits::eof();
    iterators_.clear();
    return false;
  }

  for (bool notify = !doc_limits::valid(doc_.value);
       current_itr_ < iterators_.size(); notify = true, ++current_itr_) {
    auto& itr_entry = iterators_[current_itr_];
    auto& itr = itr_entry.first;
    auto& id_map = itr_entry.second.get();

    if (!itr) {
      continue;
    }

    if (notify) {
      attribute_change_(*itr);
    }

    while (itr->next()) {
      doc_.value = id_map(itr->value());

      if (doc_limits::eof(doc_.value)) {
        continue;  // masked doc_id
      }

      return true;
    }

    itr.reset();
  }

  doc_.value = doc_limits::eof();

  return false;
}

// Iterator over sorted doc_ids for a term over all readers
class SortingCompoundDocIterator : public doc_iterator {
 public:
  explicit SortingCompoundDocIterator(CompoundDocIterator& doc_it) noexcept
    : doc_it_{&doc_it} {}

  template<typename Func>
  bool reset(Func&& func) {
    if (!doc_it_->reset(std::forward<Func>(func))) {
      return false;
    }

    merge_it_.Reset(doc_it_->iterators_);
    lead_ = nullptr;

    return true;
  }

  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return doc_it_->get_mutable(type);
  }

  bool next() final;

  doc_id_t seek(doc_id_t target) final {
    irs::seek(*this, target);
    return value();
  }

  doc_id_t value() const noexcept final { return doc_it_->value(); }

 private:
  class Context {
   public:
    using Value = CompoundDocIterator::doc_iterator_t;

    // advance
    bool operator()(const Value& value) const {
      const auto& map = value.second.get();
      while (value.first->next()) {
        if (!doc_limits::eof(map(value.first->value()))) {
          return true;
        }
      }
      return false;
    }

    // compare
    bool operator()(const Value& lhs, const Value& rhs) const {
      return lhs.second.get()(lhs.first->value()) <
             rhs.second.get()(rhs.first->value());
    }
  };

  CompoundDocIterator* doc_it_;
  ExternalMergeIterator<Context> merge_it_;
  CompoundDocIterator::doc_iterator_t* lead_{};
};

bool SortingCompoundDocIterator::next() {
  doc_it_->progress_();

  auto& current_id = doc_it_->doc_.value;
  if (doc_it_->aborted()) {
    current_id = doc_limits::eof();
    doc_it_->iterators_.clear();
    return false;
  }

  while (merge_it_.Next()) {
    auto& new_lead = merge_it_.Lead();
    auto& it = new_lead.first;
    auto& doc_map = new_lead.second.get();

    if (&new_lead != lead_) {
      // update attributes
      doc_it_->attribute_change_(*it);
      lead_ = &new_lead;
    }

    current_id = doc_map(it->value());
    if (!doc_limits::eof(current_id)) {
      return true;
    }
  }

  current_id = doc_limits::eof();
  return false;
}

class DocIteratorContainer {
 public:
  explicit DocIteratorContainer(size_t size) { itrs_.reserve(size); }

  auto begin() { return std::begin(itrs_); }
  auto end() { return std::end(itrs_); }

  template<typename Func>
  bool reset(Func&& func) {
    return func(itrs_);
  }

 private:
  std::vector<RemappingDocIterator> itrs_;
};

class CompoundColumnIterator final {
 public:
  explicit CompoundColumnIterator(size_t size) {
    iterators_.reserve(size);
    iterator_mask_.reserve(size);
  }

  void add(const SubReader& reader, const doc_map_f& doc_map) {
    auto it = reader.columns();
    IRS_ASSERT(it);

    if (IRS_LIKELY(it)) {
      iterator_mask_.emplace_back(iterators_.size());
      iterators_.emplace_back(std::move(it), reader, doc_map);
    }
  }

  // visit matched iterators
  template<typename Visitor>
  bool visit(const Visitor& visitor) const {
    for (auto id : iterator_mask_) {
      auto& it = iterators_[id];
      if (!visitor(*it.reader, *it.doc_map, it.it->value())) {
        return false;
      }
    }
    return true;
  }

  const column_reader& value() const {
    if (IRS_LIKELY(current_value_)) {
      return *current_value_;
    }

    return column_iterator::empty()->value();
  }

  bool next() {
    // advance all used iterators
    for (auto id : iterator_mask_) {
      auto& it = iterators_[id].it;

      if (it) {
        // Skip annonymous columns
        bool exhausted;
        do {
          exhausted = !it->next();
        } while (!exhausted && IsNull(it->value().name()));

        if (exhausted) {
          it = nullptr;
        }
      }
    }

    iterator_mask_.clear();  // reset for next pass

    for (size_t i = 0, size = iterators_.size(); i < size; ++i) {
      auto& it = iterators_[i].it;
      if (!it) {
        continue;  // empty iterator
      }

      const auto& value = it->value();
      const std::string_view key = value.name();
      IRS_ASSERT(!IsNull(key));

      if (!iterator_mask_.empty() && current_key_ < key) {
        continue;  // empty field or value too large
      }

      // found a smaller field
      if (iterator_mask_.empty() || key < current_key_) {
        iterator_mask_.clear();
        current_key_ = key;
        current_value_ = &value;
      }

      IRS_ASSERT(value.name() ==
                 current_value_->name());  // validated by caller
      iterator_mask_.push_back(i);
    }

    if (!iterator_mask_.empty()) {
      return true;
    }

    current_key_ = {};

    return false;
  }

 private:
  struct Iterator : util::noncopyable {
    Iterator(column_iterator::ptr&& it, const SubReader& reader,
             const doc_map_f& doc_map)
      : it(std::move(it)), reader(&reader), doc_map(&doc_map) {}

    Iterator(Iterator&&) = default;
    Iterator& operator=(Iterator&&) = delete;

    column_iterator::ptr it;
    const SubReader* reader;
    const doc_map_f* doc_map;
  };

  static_assert(std::is_nothrow_move_constructible_v<Iterator>);

  const column_reader* current_value_{};
  std::string_view current_key_;
  std::vector<size_t> iterator_mask_;  // valid iterators for current step
  std::vector<Iterator> iterators_;    // all segment iterators
};

// Iterator over documents for a term over all readers
class CompoundTermIterator : public term_iterator {
 public:
  CompoundTermIterator(const CompoundTermIterator&) = delete;
  CompoundTermIterator& operator=(const CompoundTermIterator&) = delete;

  static constexpr const size_t kProgressStepTerms = size_t(1) << 7;

  explicit CompoundTermIterator(const MergeWriter::FlushProgress& progress,
                                const Comparer* comparator)
    : doc_itr_{progress},
      has_comparer_{nullptr != comparator},
      progress_{progress, kProgressStepTerms} {}

  bool aborted() const {
    return !static_cast<bool>(progress_) || doc_itr_.aborted();
  }

  void reset(const field_meta& meta) noexcept {
    current_term_ = {};
    meta_ = &meta;
    term_iterator_mask_.clear();
    term_iterators_.clear();
    min_term_.clear();
    max_term_.clear();
    has_min_term_ = false;
  }

  const field_meta& meta() const noexcept { return *meta_; }
  void add(const term_reader& reader, const doc_map_f& doc_map);
  attribute* get_mutable(irs::type_info::type_id) noexcept final {
    // no way to merge attributes for the same term spread over multiple
    // iterators would require API change for attributes
    IRS_ASSERT(false);
    return nullptr;
  }
  bool next() final;
  doc_iterator::ptr postings(IndexFeatures features) const final;
  void read() final {
    for (const auto itr_id : term_iterator_mask_) {
      auto& it = term_iterators_[itr_id].first;
      IRS_ASSERT(it);
      it->read();
    }
  }

  bytes_view value() const final {
    if (IRS_UNLIKELY(!has_min_term_)) {
      has_min_term_ = true;
      min_term_ = current_term_;
    }
    IRS_ASSERT(max_term_ <= current_term_);
    max_term_ = current_term_;
    return current_term_;
  }

  bytes_view MinTerm() const noexcept { return min_term_; }
  bytes_view MaxTerm() const noexcept { return max_term_; }

 private:
  struct TermIterator {
    seek_term_iterator::ptr first;
    const doc_map_f* second;

    TermIterator(seek_term_iterator::ptr&& term_itr, const doc_map_f* doc_map)
      : first{std::move(term_itr)}, second{doc_map} {}

    TermIterator(TermIterator&& other) noexcept = default;
  };

  bytes_view current_term_;
  const field_meta* meta_{};
  std::vector<size_t> term_iterator_mask_;  // valid iterators for current term
  std::vector<TermIterator> term_iterators_;  // all term iterators
  mutable bstring min_term_;
  mutable bstring max_term_;
  mutable CompoundDocIterator doc_itr_;
  mutable SortingCompoundDocIterator sorting_doc_itr_{doc_itr_};
  bool has_comparer_;
  mutable bool has_min_term_{false};
  ProgressTracker progress_;
};

void CompoundTermIterator::add(const term_reader& reader,
                               const doc_map_f& doc_id_map) {
  auto it = reader.iterator(SeekMode::NORMAL);
  IRS_ASSERT(it);

  if (IRS_LIKELY(it)) {
    // mark as used to trigger next()
    term_iterator_mask_.emplace_back(term_iterators_.size());
    term_iterators_.emplace_back(std::move(it), &doc_id_map);
  }
}

bool CompoundTermIterator::next() {
  progress_();

  if (aborted()) {
    term_iterators_.clear();
    term_iterator_mask_.clear();
    return false;
  }

  // advance all used iterators
  for (const auto itr_id : term_iterator_mask_) {
    auto& it = term_iterators_[itr_id].first;
    IRS_ASSERT(it);
    if (!it->next()) {
      it.reset();
    }
  }

  term_iterator_mask_.clear();
  current_term_ = {};

  // TODO(MBkkt) use k way merge
  for (size_t i = 0, count = term_iterators_.size(); i != count; ++i) {
    auto& it = term_iterators_[i].first;
    if (!it) {
      continue;
    }
    const bytes_view value = it->value();
    IRS_ASSERT(!IsNull(value));
    IRS_ASSERT(term_iterator_mask_.empty() == IsNull(current_term_));
    if (!IsNull(current_term_)) {
      const auto cmp = value.compare(current_term_);
      if (cmp > 0) {
        continue;
      }
      if (cmp < 0) {
        term_iterator_mask_.clear();
      }
    }
    current_term_ = value;
    term_iterator_mask_.emplace_back(i);
  }

  return !IsNull(current_term_);
}

doc_iterator::ptr CompoundTermIterator::postings(
  IndexFeatures /*features*/) const {
  auto add_iterators = [this](CompoundDocIterator::iterators_t& itrs) {
    itrs.clear();
    itrs.reserve(term_iterator_mask_.size());

    for (auto& itr_id : term_iterator_mask_) {
      auto& term_itr = term_iterators_[itr_id];
      IRS_ASSERT(term_itr.first);
      auto it = term_itr.first->postings(meta().index_features);
      IRS_ASSERT(it);

      if (IRS_LIKELY(it)) {
        itrs.emplace_back(std::move(it), *term_itr.second);
      }
    }

    return true;
  };

  if (has_comparer_) {
    sorting_doc_itr_.reset(add_iterators);
    // we need to use sorting_doc_itr_ wrapper only
    if (doc_itr_.size() > 1) {
      return memory::to_managed<doc_iterator>(sorting_doc_itr_);
    }
  } else {
    doc_itr_.reset(add_iterators);
  }
  return memory::to_managed<doc_iterator>(doc_itr_);
}

// Iterator over field_ids over all readers
class CompoundFieldIterator final : public basic_term_reader {
 public:
  static constexpr const size_t kProgressStepFields = size_t(1);

  explicit CompoundFieldIterator(size_t size,
                                 const MergeWriter::FlushProgress& progress,
                                 const Comparer* comparator = nullptr)
    : term_itr_(progress, comparator),
      progress_(progress, kProgressStepFields) {
    field_iterators_.reserve(size);
    field_iterator_mask_.reserve(size);
  }

  void add(const SubReader& reader, const doc_map_f& doc_id_map);
  bool next();
  size_t size() const noexcept { return field_iterators_.size(); }

  // visit matched iterators
  template<typename Visitor>
  bool visit(const Visitor& visitor) const {
    for (auto& entry : field_iterator_mask_) {
      auto& itr = field_iterators_[entry.itr_id];
      if (!visitor(*itr.reader, *itr.doc_map, *entry.meta)) {
        return false;
      }
    }
    return true;
  }

  const field_meta& meta() const noexcept final {
    IRS_ASSERT(current_meta_);
    return *current_meta_;
  }

  bytes_view(min)() const noexcept final { return term_itr_.MinTerm(); }

  bytes_view(max)() const noexcept final { return term_itr_.MaxTerm(); }

  attribute* get_mutable(irs::type_info::type_id) noexcept final {
    return nullptr;
  }

  term_iterator::ptr iterator() const final;

  bool aborted() const {
    return !static_cast<bool>(progress_) || term_itr_.aborted();
  }

 private:
  struct FieldIterator : util::noncopyable {
    FieldIterator(field_iterator::ptr&& itr, const SubReader& reader,
                  const doc_map_f& doc_map)
      : itr(std::move(itr)), reader(&reader), doc_map(&doc_map) {}

    FieldIterator(FieldIterator&&) = default;
    FieldIterator& operator=(FieldIterator&&) = delete;

    field_iterator::ptr itr;
    const SubReader* reader;
    const doc_map_f* doc_map;
  };

  static_assert(std::is_nothrow_move_constructible_v<FieldIterator>);

  struct TermIterator {
    size_t itr_id;
    const field_meta* meta;
    const term_reader* reader;
  };

  std::string_view current_field_;
  const field_meta* current_meta_{&field_meta::kEmpty};
  // valid iterators for current field
  std::vector<TermIterator> field_iterator_mask_;
  std::vector<FieldIterator> field_iterators_;  // all segment iterators
  mutable CompoundTermIterator term_itr_;
  ProgressTracker progress_;
};

void CompoundFieldIterator::add(const SubReader& reader,
                                const doc_map_f& doc_id_map) {
  auto it = reader.fields();
  IRS_ASSERT(it);

  if (IRS_LIKELY(it)) {
    // mark as used to trigger next()
    field_iterator_mask_.emplace_back(
      TermIterator{field_iterators_.size(), nullptr, nullptr});
    field_iterators_.emplace_back(std::move(it), reader, doc_id_map);
  }
}

bool CompoundFieldIterator::next() {
  current_field_ = {};

  progress_();

  if (aborted()) {
    field_iterator_mask_.clear();
    field_iterators_.clear();
    return false;
  }

  // advance all used iterators
  for (auto& entry : field_iterator_mask_) {
    auto& it = field_iterators_[entry.itr_id].itr;
    IRS_ASSERT(it);
    if (!it->next()) {
      it.reset();
    }
  }

  // reset for next pass
  field_iterator_mask_.clear();

  // TODO(MBkkt) use k way merge
  for (size_t i = 0, count = field_iterators_.size(); i != count; ++i) {
    auto& field_itr = field_iterators_[i];

    if (!field_itr.itr) {
      continue;  // empty iterator
    }

    const auto& field_meta = field_itr.itr->value().meta();
    const auto* field_terms = field_itr.reader->field(field_meta.name);
    if (!field_terms) {
      continue;
    }
    const std::string_view field_id = field_meta.name;

    IRS_ASSERT(!IsNull(field_id));
    IRS_ASSERT(field_iterator_mask_.empty() == IsNull(current_field_));
    if (!IsNull(current_field_)) {
      const auto cmp = field_id.compare(current_field_);
      if (cmp > 0) {
        continue;
      }
      if (cmp < 0) {
        field_iterator_mask_.clear();
      }
    }
    current_field_ = field_id;
    current_meta_ = &field_meta;

    // validated by caller
    IRS_ASSERT(IsSubsetOf(field_meta.features, meta().features));
    IRS_ASSERT(field_meta.index_features <= meta().index_features);

    field_iterator_mask_.emplace_back(
      TermIterator{i, &field_meta, field_terms});
  }

  return !IsNull(current_field_);
}

term_iterator::ptr CompoundFieldIterator::iterator() const {
  term_itr_.reset(meta());

  for (const auto& segment : field_iterator_mask_) {
    term_itr_.add(*(segment.reader),
                  *(field_iterators_[segment.itr_id].doc_map));
  }

  return memory::to_managed<term_iterator>(term_itr_);
}

// Computes fields_type
bool ComputeFieldMeta(field_meta_map_t& field_meta_map,
                      IndexFeatures& index_features,
                      feature_set_t& fields_features, const SubReader& reader) {
  REGISTER_TIMER_DETAILED();

  for (auto it = reader.fields(); it->next();) {
    const auto& field_meta = it->value().meta();
    const auto [field_meta_it, is_new] =
      field_meta_map.emplace(field_meta.name, &field_meta);

    // validate field_meta equivalence
    if (!is_new &&
        (!IsSubsetOf(field_meta.index_features,
                     field_meta_it->second->index_features) ||
         !IsSubsetOf(field_meta.features, field_meta_it->second->features))) {
      return false;  // field_meta is not equal, so cannot merge segments
    }

    AccumulateFeatures(fields_features, field_meta.features);
    index_features |= field_meta.index_features;
  }

  return true;
}

// Helper class responsible for writing a data from different sources
// into single columnstore.
class Columnstore {
 public:
  static constexpr size_t kProgressStepColumn = size_t{1} << 13;

  Columnstore(columnstore_writer::ptr&& writer,
              const MergeWriter::FlushProgress& progress)
    : progress_{progress, kProgressStepColumn}, writer_{std::move(writer)} {}

  Columnstore(directory& dir, const SegmentMeta& meta,
              const MergeWriter::FlushProgress& progress, IResourceManager& rm)
    : progress_{progress, kProgressStepColumn} {
    auto writer = meta.codec->get_columnstore_writer(true, rm);
    writer->prepare(dir, meta);

    writer_ = std::move(writer);
  }

  // Inserts live values from the specified iterators into a column.
  // Returns column id of the inserted column on success,
  //  field_limits::invalid() in case if no data were inserted,
  //  empty value is case if operation was interrupted.
  template<typename Writer>
  std::optional<field_id> insert(
    DocIteratorContainer& itrs, const ColumnInfo& info,
    columnstore_writer::column_finalizer_f&& finalizer, Writer&& writer);

  // Inserts live values from the specified 'iterator' into a column.
  // Returns column id of the inserted column on success,
  //  field_limits::invalid() in case if no data were inserted,
  //  empty value is case if operation was interrupted.
  template<typename Writer>
  std::optional<field_id> insert(
    SortingCompoundDocIterator& it, const ColumnInfo& info,
    columnstore_writer::column_finalizer_f&& finalizer, Writer&& writer);

  // Returns `true` if anything was actually flushed
  bool flush(const flush_state& state) { return writer_->commit(state); }

  bool valid() const noexcept { return static_cast<bool>(writer_); }

 private:
  ProgressTracker progress_;
  columnstore_writer::ptr writer_;
};

template<typename Writer>
std::optional<field_id> Columnstore::insert(
  DocIteratorContainer& itrs, const ColumnInfo& info,
  columnstore_writer::column_finalizer_f&& finalizer, Writer&& writer) {
  auto next_iterator = [end = std::end(itrs)](auto begin) {
    return std::find_if(begin, end, [](auto& it) { return it.next(); });
  };

  auto begin = next_iterator(std::begin(itrs));

  if (begin == std::end(itrs)) {
    // Empty column
    return std::make_optional(field_limits::invalid());
  }

  auto column = writer_->push_column(info, std::move(finalizer));

  auto write_column = [&column, &writer, this](auto& it) {
    auto* payload = irs::get<irs::payload>(it);

    do {
      if (!progress_()) {
        // Stop was requested
        return false;
      }

      const auto doc = it.value();
      if (payload) {
        writer(column.out, doc, payload->value);
      } else {
        column.out.Prepare(doc);
      }
    } while (it.next());

    return true;
  };

  do {
    if (!write_column(*begin)) {
      // Stop was requested
      return std::nullopt;
    }

    begin = next_iterator(++begin);
  } while (begin != std::end(itrs));

  return std::make_optional(column.id);
}

template<typename Writer>
std::optional<field_id> Columnstore::insert(
  SortingCompoundDocIterator& it, const ColumnInfo& info,
  columnstore_writer::column_finalizer_f&& finalizer, Writer&& writer) {
  const payload* payload = nullptr;

  auto* callback = irs::get<attribute_provider_change>(it);

  if (callback) {
    callback->subscribe([&payload](const attribute_provider& attrs) {
      payload = irs::get<irs::payload>(attrs);
    });
  } else {
    payload = irs::get<irs::payload>(it);
  }

  if (it.next()) {
    auto column = writer_->push_column(info, std::move(finalizer));

    do {
      if (!progress_()) {
        // Stop was requested
        return std::nullopt;
      }

      const auto doc = it.value();

      if (payload) {
        writer(column.out, doc, payload->value);
      } else {
        column.out.Prepare(doc);
      }
    } while (it.next());

    return std::make_optional(column.id);
  } else {
    // Empty column
    return std::make_optional(field_limits::invalid());
  }
}

struct PrimarySortIteratorAdapter {
  explicit PrimarySortIteratorAdapter(doc_iterator::ptr it,
                                      doc_iterator::ptr live_docs) noexcept
    : it{std::move(it)},
      doc{irs::get<document>(*this->it)},
      payload{irs::get<irs::payload>(*this->it)},
      live_docs{std::move(live_docs)},
      live_doc{this->live_docs ? irs::get<document>(*this->live_docs)
                               : nullptr} {
    IRS_ASSERT(valid());
  }

  [[nodiscard]] bool valid() const noexcept {
    return it && doc && payload && (!live_docs || live_doc);
  }

  doc_iterator::ptr it;
  const document* doc;
  const irs::payload* payload;
  doc_iterator::ptr live_docs;
  const document* live_doc;
  doc_id_t min{};
};

class MergeContext {
 public:
  using Value = PrimarySortIteratorAdapter;

  MergeContext(const Comparer& compare) noexcept : compare_{&compare} {}

  // advance
  bool operator()(Value& value) const {
    value.min = value.doc->value + 1;
    return value.it->next();
  }

  // compare
  bool operator()(const Value& lhs, const Value& rhs) const {
    IRS_ASSERT(&lhs != &rhs);
    const bytes_view lhs_value = lhs.payload->value;
    const bytes_view rhs_value = rhs.payload->value;
    const auto r = compare_->Compare(lhs_value, rhs_value);
    return r < 0;
  }

 private:
  const Comparer* compare_;
};

template<typename Iterator>
bool WriteColumns(Columnstore& cs, Iterator& columns,
                  const ColumnInfoProvider& column_info,
                  CompoundColumnIterator& column_itr,
                  const MergeWriter::FlushProgress& progress) {
  REGISTER_TIMER_DETAILED();
  IRS_ASSERT(cs.valid());
  IRS_ASSERT(progress);

  auto add_iterators = [&column_itr](auto& itrs) {
    auto add_iterators = [&itrs](const SubReader& /*segment*/,
                                 const doc_map_f& doc_map,
                                 const irs::column_reader& column) {
      auto it = column.iterator(ColumnHint::kConsolidation);

      if (IRS_LIKELY(it && irs::get<document>(*it))) {
        itrs.emplace_back(std::move(it), doc_map);
      } else {
        IRS_ASSERT(false);
        IRS_LOG_ERROR(
          "Got an invalid iterator during consolidationg of the columnstore, "
          "skipping it");
      }
      return true;
    };

    itrs.clear();
    return column_itr.visit(add_iterators);
  };

  while (column_itr.next()) {
    // visit matched columns from merging segments and
    // write all survived values to the new segment
    if (!progress() || !columns.reset(add_iterators)) {
      return false;  // failed to visit all values
    }

    const std::string_view column_name = column_itr.value().name();

    const auto res = cs.insert(
      columns, column_info(column_name),
      [column_name](bstring&) { return column_name; },
      [](column_output& out, doc_id_t doc, bytes_view payload) {
        out.Prepare(doc);
        if (!payload.empty()) {
          out.write_bytes(payload.data(), payload.size());
        }
      });

    if (!res.has_value()) {
      return false;  // failed to insert all values
    }
  }

  return true;
}

class BufferedValues final : public column_reader, data_output {
 public:
  BufferedValues(IResourceManager& rm) : index_{{rm}}, data_{{rm}} {}
  void Clear() noexcept {
    index_.clear();
    data_.clear();
    header_.reset();
    out_ = nullptr;
    feature_writer_ = nullptr;
  }

  void Reserve(size_t count, size_t value_size) {
    index_.reserve(count);
    data_.reserve(count * value_size);
  }

  void PushBack(doc_id_t doc, bytes_view value) {
    index_.emplace_back(doc, data_.size(), value.size());
    data_.append(value);
  }

  void SetID(field_id id) noexcept { id_ = id; }

  void UpdateHeader() {
    if (feature_writer_) {
      feature_writer_->finish(header_.emplace());
    }
    if (!out_) {
      return;
    }
    auto* data = data_.data();
    for (auto v : index_) {
      out_->Prepare(v.key);
      out_->write_bytes(data + v.begin, v.size);
    }
  }

  void SetFeatureWriter(FeatureWriter& writer) noexcept {
    IRS_ASSERT(!feature_writer_);
    feature_writer_ = &writer;
    IRS_ASSERT(out_ == nullptr);
  }

  void operator()(column_output& out, doc_id_t doc, bytes_view payload) {
    IRS_ASSERT(out_ == nullptr || out_ == &out);
    out_ = &out;
    const auto begin = data_.size();
    feature_writer_->write(*this, payload);
    index_.emplace_back(doc, begin, data_.size() - begin);
  }

  // data_output
  void write_byte(byte_type b) final { data_ += b; }

  void write_bytes(const byte_type* b, size_t len) final {
    data_.append(b, len);
  }

  void write_short(int16_t value) final {
    auto* begin = EnsureSize(sizeof(uint16_t));
    irs::write<uint16_t>(begin, value);
  }

  void write_int(int32_t value) final {
    auto* begin = EnsureSize(sizeof(uint32_t));
    irs::write<uint32_t>(begin, value);
  }

  void write_long(int64_t value) final {
    auto* begin = EnsureSize(sizeof(uint64_t));
    irs::write<uint64_t>(begin, value);
  }

  void write_vint(uint32_t value) final {
    auto* begin = EnsureSize(bytes_io<uint32_t>::const_max_vsize);
    irs::vwrite<uint32_t>(begin, value);
  }

  void write_vlong(uint64_t value) final {
    auto* begin = EnsureSize(bytes_io<uint64_t>::const_max_vsize);
    irs::vwrite<uint64_t>(begin, value);
  }

  // column_reader
  field_id id() const noexcept final { return id_; }

  std::string_view name() const noexcept final { return {}; }

  bytes_view payload() const noexcept final {
    return header_.has_value() ? *header_ : bytes_view{};
  }

  doc_id_t size() const noexcept final {
    return static_cast<doc_id_t>(index_.size());
  }

  doc_iterator::ptr iterator(ColumnHint hint) const noexcept final {
    // kPrevDoc isn't supported atm
    IRS_ASSERT(ColumnHint::kNormal == (hint & ColumnHint::kPrevDoc));

    // FIXME(gnusi): can avoid allocation with the help of managed_ptr
    return memory::make_managed<BufferedColumnIterator>(index_, data_);
  }

 private:
  byte_type* EnsureSize(size_t size) {
    const auto offset = data_.size();
    absl::strings_internal::STLStringResizeUninitializedAmortized(
      &data_, offset + size);
    return data_.data() + offset;
  }

  BufferedColumn::BufferedValues index_;
  BufferedColumn::Buffer data_;
  field_id id_{field_limits::invalid()};
  std::optional<bstring> header_;
  column_output* out_{};
  FeatureWriter* feature_writer_{};
};

class BufferedColumns final : public irs::ColumnProvider {
 public:
  BufferedColumns(IResourceManager& rm) : rm_(rm) {}
  const irs::column_reader* column(field_id field) const noexcept final {
    if (IRS_UNLIKELY(!field_limits::valid(field))) {
      return nullptr;
    }

    return Find(field);
  }

  const irs::column_reader* column(std::string_view) const noexcept final {
    return nullptr;
  }

  BufferedValues& PushColumn() {
    if (auto* column = Find(field_limits::invalid()); column) {
      column->Clear();
      return *column;
    }

    return columns_.emplace_back(rm_);
  }

  void Clear() noexcept {
    for (auto& column : columns_) {
      column.SetID(field_limits::invalid());
    }
  }

 private:
  BufferedValues* Find(field_id id) const noexcept {
    for (auto& column : columns_) {
      if (column.id() == id) {
        return const_cast<BufferedValues*>(&column);
      }
    }
    return nullptr;
  }

  // SmallVector seems to be incompatible with
  // our ManagedTypedAllocator
  SmallVector<BufferedValues, 1> columns_;
  IResourceManager& rm_;
};

// Write field term data
template<typename Iterator>
bool WriteFields(Columnstore& cs, Iterator& feature_itr,
                 const irs::flush_state& flush_state, const SegmentMeta& meta,
                 const FeatureInfoProvider& column_info,
                 CompoundFieldIterator& field_itr,
                 const feature_set_t& scorers_features,
                 const MergeWriter::FlushProgress& progress,
                 IResourceManager& rm) {
  REGISTER_TIMER_DETAILED();
  IRS_ASSERT(cs.valid());

  feature_map_t features;
  irs::type_info::type_id feature{};
  std::vector<bytes_view> hdrs;
  hdrs.reserve(field_itr.size());
  doc_id_t count{};  // Total count of documents in consolidating columns

  auto add_iterators = [&](auto& itrs) {
    auto add_iterators = [&](const SubReader& segment, const doc_map_f& doc_map,
                             const field_meta& field) {
      const auto column = field.features.find(feature);

      if (column == field.features.end() ||
          !field_limits::valid(column->second)) {
        // Field has no feature
        return true;
      }

      auto* reader = segment.column(column->second);

      // Tail columns can be removed if empty.
      if (reader) {
        auto it = reader->iterator(ColumnHint::kConsolidation);
        IRS_ASSERT(it);

        if (IRS_LIKELY(it)) {
          hdrs.emplace_back(reader->payload());

          if (IRS_LIKELY(irs::get<document>(*it))) {
            count += reader->size();
            itrs.emplace_back(std::move(it), doc_map);
          } else {
            IRS_ASSERT(false);
            IRS_LOG_ERROR(
              "Failed to get document attribute from the iterator, skipping "
              "it");
          }
        }
      }

      return true;
    };

    count = 0;
    hdrs.clear();
    itrs.clear();
    return field_itr.visit(add_iterators);
  };

  auto field_writer = meta.codec->get_field_writer(true, rm);
  field_writer->prepare(flush_state);

  // Ensured by the caller
  IRS_ASSERT(flush_state.columns);
  auto& buffered_columns = const_cast<BufferedColumns&>(
    DownCast<BufferedColumns>(*flush_state.columns));

  while (field_itr.next()) {
    buffered_columns.Clear();
    features.clear();
    auto& field_meta = field_itr.meta();

    auto begin = field_meta.features.begin();
    auto end = field_meta.features.end();

    for (; begin != end; ++begin) {
      if (!progress()) {
        return false;
      }

      std::tie(feature, std::ignore) = *begin;

      if (!feature_itr.reset(add_iterators)) {
        return false;
      }

      const auto [info, factory] = column_info(feature);

      std::optional<field_id> res;
      auto feature_writer =
        factory ? (*factory)({hdrs.data(), hdrs.size()}) : FeatureWriter::ptr{};

      auto* buffered_column = scorers_features.contains(feature)
                                ? &buffered_columns.PushColumn()
                                : nullptr;

      if (buffered_column) {
        // FIXME(gnusi): We can get better estimation from column headers/stats.
        static constexpr size_t kValueSize = sizeof(uint32_t);
        buffered_column->Reserve(count, kValueSize);
      }

      if (feature_writer) {
        auto write_values = [&, &info = info]<typename T>(T&& value_writer) {
          return cs.insert(
            feature_itr, info,
            [feature_writer = std::move(feature_writer)](bstring& out) {
              feature_writer->finish(out);
              return std::string_view{};
            },
            std::forward<T>(value_writer));
        };

        if (buffered_column) {
          buffered_column->SetFeatureWriter(*feature_writer);
          res = write_values(*buffered_column);
        } else {
          res = write_values(
            [writer = feature_writer.get()](column_output& out, doc_id_t doc,
                                            bytes_view payload) {
              out.Prepare(doc);
              writer->write(out, payload);
            });
        }

      } else if (!factory) {
        // Factory has failed to instantiate a writer
        res = cs.insert(
          feature_itr, info, [](bstring&) { return std::string_view{}; },
          [buffered_column](column_output& out, doc_id_t doc,
                            bytes_view payload) {
            out.Prepare(doc);
            if (!payload.empty()) {
              out.write_bytes(payload.data(), payload.size());
              if (buffered_column) {
                buffered_column->PushBack(doc, payload);
              }
            }
          });
      }

      if (!res.has_value()) {
        return false;  // Failed to insert all values
      }

      features[feature] = *res;
      if (buffered_column) {
        buffered_column->SetID(*res);
        buffered_column->UpdateHeader();
      }
    }

    // Write field terms
    field_writer->write(field_itr, features);
  }

  field_writer->end();
  field_writer.reset();

  return !field_itr.aborted();
}

// Computes doc_id_map and docs_count
doc_id_t ComputeDocIds(doc_id_map_t& doc_id_map, const SubReader& reader,
                       doc_id_t next_id) noexcept {
  REGISTER_TIMER_DETAILED();
  // assume not a lot of space wasted if doc_limits::min() > 0
  try {
    doc_id_map.resize(reader.docs_count() + doc_limits::min(),
                      doc_limits::eof());
  } catch (...) {
    IRS_LOG_ERROR(absl::StrCat(
      "Failed to resize merge_writer::doc_id_map to accommodate element: ",
      reader.docs_count() + doc_limits::min()));

    return doc_limits::invalid();
  }

  for (auto docs_itr = reader.docs_iterator(); docs_itr->next(); ++next_id) {
    auto src_doc_id = docs_itr->value();

    IRS_ASSERT(src_doc_id >= doc_limits::min());
    IRS_ASSERT(src_doc_id < reader.docs_count() + doc_limits::min());
    doc_id_map[src_doc_id] = next_id;  // set to next valid doc_id
  }

  return next_id;
}

#if defined(IRESEARCH_DEBUG) && !defined(__clang__)
void EnsureSorted(const auto& readers) {
  for (const auto& reader : readers) {
    const auto& doc_map = reader.doc_id_map;
    IRS_ASSERT(doc_map.size() >= doc_limits::min());

    auto view = doc_map | std::views::filter([](doc_id_t doc) noexcept {
                  return !doc_limits::eof(doc);
                });

    IRS_ASSERT(std::ranges::is_sorted(view));
  }
}
#endif

const MergeWriter::FlushProgress kProgressNoop = []() { return true; };

}  // namespace

MergeWriter::ReaderCtx::ReaderCtx(const SubReader* reader,
                                  IResourceManager& rm) noexcept
  : reader{reader}, doc_id_map{{rm}}, doc_map{[](doc_id_t) noexcept {
      return doc_limits::eof();
    }} {
  IRS_ASSERT(this->reader);
}

MergeWriter::MergeWriter(IResourceManager& rm) noexcept
  : dir_(NoopDirectory::instance()), readers_{{rm}} {}

MergeWriter::operator bool() const noexcept {
  return &dir_ != &NoopDirectory::instance();
}

bool MergeWriter::FlushUnsorted(TrackingDirectory& dir, SegmentMeta& segment,
                                const FlushProgress& progress) {
  REGISTER_TIMER_DETAILED();
  IRS_ASSERT(progress);
  IRS_ASSERT(!comparator_);
  IRS_ASSERT(column_info_ && *column_info_);
  IRS_ASSERT(feature_info_ && *feature_info_);

  const size_t size = readers_.size();

  field_meta_map_t field_meta_map;
  CompoundFieldIterator fields_itr{size, progress};
  CompoundColumnIterator columns_itr{size};
  feature_set_t fields_features;
  IndexFeatures index_features{IndexFeatures::NONE};

  DocIteratorContainer remapping_itrs{size};

  doc_id_t base_id = doc_limits::min();  // next valid doc_id

  // collect field meta and field term data
  for (auto& reader_ctx : readers_) {
    // ensured by merge_writer::add(...)
    IRS_ASSERT(reader_ctx.reader);

    auto& reader = *reader_ctx.reader;
    const auto docs_count = reader.docs_count();

    if (reader.live_docs_count() == docs_count) {  // segment has no deletes
      const auto reader_base = base_id - doc_limits::min();
      IRS_ASSERT(static_cast<uint64_t>(base_id) + docs_count <
                 std::numeric_limits<doc_id_t>::max());
      base_id += static_cast<doc_id_t>(docs_count);

      reader_ctx.doc_map = [reader_base](doc_id_t doc) noexcept {
        return reader_base + doc;
      };
    } else {  // segment has some deleted docs
      auto& doc_id_map = reader_ctx.doc_id_map;
      base_id = ComputeDocIds(doc_id_map, reader, base_id);

      reader_ctx.doc_map = [&doc_id_map](doc_id_t doc) noexcept {
        return doc >= doc_id_map.size() ? doc_limits::eof() : doc_id_map[doc];
      };
    }

    if (!doc_limits::valid(base_id)) {
      return false;  // failed to compute next doc_id
    }

    if (!ComputeFieldMeta(field_meta_map, index_features, fields_features,
                          reader)) {
      return false;
    }

    fields_itr.add(reader, reader_ctx.doc_map);
    columns_itr.add(reader, reader_ctx.doc_map);
  }

  // total number of doc_ids
  segment.docs_count = base_id - doc_limits::min();
  // all merged documents are live
  segment.live_docs_count = segment.docs_count;

  if (!progress()) {
    return false;  // progress callback requested termination
  }

  // write merged segment data
  REGISTER_TIMER_DETAILED();
  Columnstore cs(dir, segment, progress,
                 readers_.get_allocator().ResourceManager());

  if (!cs.valid()) {
    return false;  // flush failure
  }

  if (!progress()) {
    return false;  // progress callback requested termination
  }

  if (!WriteColumns(cs, remapping_itrs, *column_info_, columns_itr, progress)) {
    return false;  // flush failure
  }

  if (!progress()) {
    return false;  // progress callback requested termination
  }

  BufferedColumns buffered_columns{readers_.get_allocator().ResourceManager()};

  const flush_state state{.dir = &dir,
                          .columns = &buffered_columns,
                          .features = &fields_features,
                          .name = segment.name,
                          .scorers = scorers_,
                          .doc_count = segment.docs_count,
                          .index_features = index_features};

  // Write field meta and field term data
  IRS_ASSERT(scorers_features_);
  if (!WriteFields(cs, remapping_itrs, state, segment, *feature_info_,
                   fields_itr, *scorers_features_, progress,
                   readers_.get_allocator().ResourceManager())) {
    return false;  // Flush failure
  }

  if (!progress()) {
    return false;  // Progress callback requested termination
  }

  segment.column_store = cs.flush(state);

  return true;
}

bool MergeWriter::FlushSorted(TrackingDirectory& dir, SegmentMeta& segment,
                              const FlushProgress& progress) {
  REGISTER_TIMER_DETAILED();
  IRS_ASSERT(progress);
  IRS_ASSERT(comparator_);
  IRS_ASSERT(column_info_ && *column_info_);
  IRS_ASSERT(feature_info_ && *feature_info_);

  const size_t size = readers_.size();

  field_meta_map_t field_meta_map;
  CompoundColumnIterator columns_itr{size};
  CompoundFieldIterator fields_itr{size, progress, comparator_};
  feature_set_t fields_features;
  IndexFeatures index_features{IndexFeatures::NONE};

  std::vector<PrimarySortIteratorAdapter> itrs;
  itrs.reserve(size);

  auto emplace_iterator = [&itrs](const SubReader& segment) {
    if (!segment.sort()) {
      // Sort column is not present, give up
      return false;
    }

    auto sort =
      segment.mask(segment.sort()->iterator(irs::ColumnHint::kConsolidation));

    if (IRS_UNLIKELY(!sort)) {
      return false;
    }

    doc_iterator::ptr live_docs;
    if (segment.docs_count() != segment.live_docs_count()) {
      live_docs = segment.docs_iterator();
    }

    auto& it = itrs.emplace_back(std::move(sort), std::move(live_docs));

    if (IRS_UNLIKELY(!it.valid())) {
      return false;
    }

    return true;
  };

  segment.docs_count = 0;

  // Init doc map for each reader
  for (auto& reader_ctx : readers_) {
    // Ensured by merge_writer::add(...)
    IRS_ASSERT(reader_ctx.reader);

    auto& reader = *reader_ctx.reader;

    if (reader.docs_count() > doc_limits::eof() - doc_limits::min()) {
      // Can't merge segment holding more than 'doc_limits::eof()-1' docs
      return false;
    }

    if (!emplace_iterator(reader)) {
      // Sort column is not present, give up
      return false;
    }

    if (!ComputeFieldMeta(field_meta_map, index_features, fields_features,
                          reader)) {
      return false;
    }

    fields_itr.add(reader, reader_ctx.doc_map);
    columns_itr.add(reader, reader_ctx.doc_map);

    // Count total number of documents in consolidated segment
    if (!math::sum_check_overflow(segment.docs_count, reader.live_docs_count(),
                                  segment.docs_count)) {
      return false;
    }

    // Prepare doc maps
    auto& doc_id_map = reader_ctx.doc_id_map;

    try {
      doc_id_map.resize(reader.docs_count() + doc_limits::min(),
                        doc_limits::eof());
    } catch (...) {
      IRS_LOG_ERROR(absl::StrCat(
        "Failed to resize merge_writer::doc_id_map to accommodate element: ",
        reader.docs_count() + doc_limits::min()));

      return false;
    }

    reader_ctx.doc_map = [doc_id_map =
                            std::span{doc_id_map}](size_t doc) noexcept {
      IRS_ASSERT(doc_id_map[0] == doc_limits::eof());
      return doc_id_map[doc * static_cast<size_t>(doc < doc_id_map.size())];
    };
  }

  if (segment.docs_count >= doc_limits::eof()) {
    // Can't merge segments holding more than 'doc_limits::eof()-1' docs
    return false;
  }

  if (!progress()) {
    return false;  // Progress callback requested termination
  }

  // Write new sorted column and fill doc maps for each reader
  auto writer = segment.codec->get_columnstore_writer(
    true, readers_.get_allocator().ResourceManager());
  writer->prepare(dir, segment);

  // Get column info for sorted column
  const auto info = (*column_info_)({});
  auto [column_id, column_writer] = writer->push_column(info, {});

  doc_id_t next_id = doc_limits::min();

  auto fill_doc_map = [&](doc_id_map_t& doc_id_map,
                          PrimarySortIteratorAdapter& it, doc_id_t max) {
    if (auto min = it.min; min < max) {
      if (it.live_docs) {
        auto& live_docs = *it.live_docs;
        const auto* live_doc = it.live_doc;
        for (live_docs.seek(min); live_doc->value < max; live_docs.next()) {
          doc_id_map[live_doc->value] = next_id++;
          if (!progress()) {
            return false;
          }
        }
      } else {
        do {
          doc_id_map[min] = next_id++;
          ++min;
          if (!progress()) {
            return false;
          }
        } while (min < max);
      }
    }
    return true;
  };

  ExternalMergeIterator<MergeContext> columns_it{*comparator_};

  for (columns_it.Reset(itrs); columns_it.Next();) {
    auto& it = columns_it.Lead();
    IRS_ASSERT(it.valid());

    const auto max = it.doc->value;
    const auto index = &it - itrs.data();
    auto& doc_id_map = readers_[index].doc_id_map;

    // Fill doc id map
    if (!fill_doc_map(doc_id_map, it, max)) {
      return false;  // Progress callback requested termination
    }
    doc_id_map[max] = next_id;

    // write value into new column if present
    column_writer.Prepare(next_id);
    const auto payload = it.payload->value;
    column_writer.write_bytes(payload.data(), payload.size());

    ++next_id;

    if (!progress()) {
      return false;  // Progress callback requested termination
    }
  }

  // Handle empty values greater than the last document in sort column
  for (auto it = itrs.begin(); auto& reader : readers_) {
    if (!fill_doc_map(reader.doc_id_map, *it,
                      doc_limits::min() +
                        static_cast<doc_id_t>(reader.reader->docs_count()))) {
      return false;  // progress callback requested termination
    }
    ++it;
  }

#if defined(IRESEARCH_DEBUG) && !defined(__clang__)
  EnsureSorted(readers_);
#endif

  Columnstore cs(std::move(writer), progress);
  CompoundDocIterator doc_it(progress);
  SortingCompoundDocIterator sorting_doc_it(doc_it);

  if (!cs.valid()) {
    return false;  // Flush failure
  }

  if (!progress()) {
    return false;  // Progress callback requested termination
  }

  if (!WriteColumns(cs, sorting_doc_it, *column_info_, columns_itr, progress)) {
    return false;  // Flush failure
  }

  if (!progress()) {
    return false;  // Progress callback requested termination
  }

  BufferedColumns buffered_columns{readers_.get_allocator().ResourceManager()};

  const flush_state state{.dir = &dir,
                          .columns = &buffered_columns,
                          .features = &fields_features,
                          .name = segment.name,
                          .scorers = scorers_,
                          .doc_count = segment.docs_count,
                          .index_features = index_features};

  // Write field meta and field term data
  IRS_ASSERT(scorers_features_);
  if (!WriteFields(cs, sorting_doc_it, state, segment, *feature_info_,
                   fields_itr, *scorers_features_, progress,
                   readers_.get_allocator().ResourceManager())) {
    return false;  // flush failure
  }

  if (!progress()) {
    return false;  // Progress callback requested termination
  }

  segment.column_store = cs.flush(state);  // Flush columnstore
  segment.sort = column_id;                // Set sort column identifier
  // All merged documents are live
  segment.live_docs_count = segment.docs_count;

  return true;
}

bool MergeWriter::Flush(SegmentMeta& segment,
                        const FlushProgress& progress /*= {}*/) {
  REGISTER_TIMER_DETAILED();
  IRS_ASSERT(segment.codec);  // Must be set outside

  bool result = false;  // Flush result

  Finally segment_invalidator = [&result, &segment]() noexcept {
    if (IRS_UNLIKELY(!result)) {
      // Invalidate segment
      segment.files.clear();
      segment.column_store = false;
      static_cast<SegmentInfo&>(segment) = SegmentInfo{};
    }
  };

  const auto& progress_callback = progress ? progress : kProgressNoop;

  TrackingDirectory track_dir{dir_};  // Track writer created files

  result = comparator_ ? FlushSorted(track_dir, segment, progress_callback)
                       : FlushUnsorted(track_dir, segment, progress_callback);

  segment.files = track_dir.FlushTracked(segment.byte_size);

  return result;
}

}  // namespace irs
