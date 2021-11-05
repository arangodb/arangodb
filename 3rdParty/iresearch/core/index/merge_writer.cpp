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

#include <deque>

#include <absl/container/flat_hash_map.h>

#include "merge_writer.hpp"
#include "analysis/token_attributes.hpp"
#include "index/comparer.hpp"
#include "index/field_meta.hpp"
#include "index/heap_iterator.hpp"
#include "index/index_meta.hpp"
#include "index/norm.hpp"
#include "index/segment_reader.hpp"
#include "utils/directory_utils.hpp"
#include "utils/log.hpp"
#include "utils/lz4compression.hpp"
#include "utils/memory.hpp"
#include "utils/type_limits.hpp"
#include "utils/version_utils.hpp"
#include "store/store_utils.hpp"

#include <array>

#include <boost/iterator/filter_iterator.hpp>

namespace {

using namespace irs;

bool is_subset_of(const feature_map_t& lhs, const feature_map_t& rhs) noexcept {
  for (auto& entry : lhs) {
    if (!rhs.count(entry.first)) {
      return false;
    }
  }
  return true;
}

void accumulate_features(feature_set_t& accum, const feature_map_t& features) {
  for (auto& entry : features) {
    accum.emplace(entry.first);
  }
}

// mapping of old doc_id to new doc_id (reader doc_ids are sequential 0 based)
// masked doc_ids have value of MASKED_DOC_ID
using doc_id_map_t = std::vector<doc_id_t>;

// document mapping function
using doc_map_f = std::function<doc_id_t(doc_id_t)>;

using field_meta_map_t = absl::flat_hash_map<string_ref, const field_meta*>;

class noop_directory : public directory {
 public:
  static noop_directory& instance() {
    static noop_directory INSTANCE;
    return INSTANCE;
  }

  virtual directory_attributes& attributes() noexcept override {
    return attrs_;
  }

  virtual index_output::ptr create(const std::string&) noexcept override {
    return nullptr;
  }

  virtual bool exists(bool&, const std::string&) const noexcept override {
    return false;
  }

  virtual bool length(uint64_t&, const std::string&) const noexcept override {
    return false;
  }

  virtual index_lock::ptr make_lock(const std::string&) noexcept override {
    return nullptr;
  }

  virtual bool mtime(std::time_t&, const std::string&) const noexcept override {
    return false;
  }

  virtual index_input::ptr open(
      const std::string&, IOAdvice) const noexcept override {
    return nullptr;
  }

  virtual bool remove(const std::string&) noexcept override {
    return false;
  }

  virtual bool rename(
    const std::string&, const std::string&
  ) noexcept override {
    return false;
  }

  virtual bool sync(const std::string&) noexcept override {
    return false;
  }

  virtual bool visit(const directory::visitor_f&) const override {
    return false;
  }

 private:
  noop_directory() = default;

  directory_attributes attrs_{0, nullptr};
}; // noop_directory

class progress_tracker {
 public:
  explicit progress_tracker(
      const merge_writer::flush_progress_t& progress,
      size_t count) noexcept
    : progress_(&progress),
      count_(count) {
    assert(progress);
  }

  bool operator()() {
    if (hits_++ >= count_) {
      hits_ = 0;
      valid_ = (*progress_)();
    }

    return valid_;
  }

  explicit operator bool() const noexcept {
    return valid_;
  }

  void reset() noexcept {
    hits_ = 0;
    valid_ = true;
  }

 private:
  const merge_writer::flush_progress_t* progress_;
  const size_t count_; // call progress callback each `count_` hits
  size_t hits_{ 0 }; // current number of hits
  bool valid_{ true };
}; // progress_tracker

//////////////////////////////////////////////////////////////////////////////
/// @struct compound_doc_iterator
/// @brief iterator over doc_ids for a term over all readers
//////////////////////////////////////////////////////////////////////////////
struct compound_doc_iterator : public doc_iterator {
  typedef std::pair<doc_iterator::ptr, const doc_map_f*> doc_iterator_t;
  typedef std::vector<doc_iterator_t> iterators_t;

  static constexpr const size_t PROGRESS_STEP_DOCS = size_t(1) << 14;

  explicit compound_doc_iterator(
      const merge_writer::flush_progress_t& progress) noexcept
    : progress(progress, PROGRESS_STEP_DOCS) {
  }

  template<typename Func>
  bool reset(Func func) {
    if (!func(iterators)) {
      return false;
    }

    current_id = doc_limits::invalid();
    current_itr = 0;

    return true;
  }

  bool aborted() const noexcept {
    return !static_cast<bool>(progress);
  }

  virtual attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    return irs::type<attribute_provider_change>::id() == type
      ? &attribute_change
      : nullptr;
  }

  virtual bool next() override;

  virtual doc_id_t seek(doc_id_t target) override {
    irs::seek(*this, target);
    return value();
  }

  virtual doc_id_t value() const noexcept override {
    return current_id;
  }

  attribute_provider_change attribute_change;
  std::vector<doc_iterator_t> iterators;
  doc_id_t current_id{ doc_limits::invalid() };
  size_t current_itr{ 0 };
  progress_tracker progress;
}; // compound_doc_iterator

bool compound_doc_iterator::next() {
  progress();

  if (aborted()) {
    current_id = doc_limits::eof();
    iterators.clear();
    return false;
  }

  for (bool notify = !doc_limits::valid(current_id);
       current_itr < iterators.size();
       notify = true, ++current_itr) {
    auto& itr_entry = iterators[current_itr];
    auto& itr = itr_entry.first;
    auto& id_map = itr_entry.second;

    if (!itr) {
      continue;
    }

    if (notify) {
      attribute_change(*itr);
    }

    while (itr->next()) {
      current_id = (*id_map)(itr->value());

      if (doc_limits::eof(current_id)) {
        continue; // masked doc_id
      }

      return true;
    }

    itr.reset();
  }

  current_id = doc_limits::eof();

  return false;
}

//////////////////////////////////////////////////////////////////////////////
/// @struct sorting_compound_doc_iterator
/// @brief iterator over sorted doc_ids for a term over all readers
//////////////////////////////////////////////////////////////////////////////
class sorting_compound_doc_iterator : public doc_iterator {
 public:
  explicit sorting_compound_doc_iterator(
      compound_doc_iterator& doc_it
  ) noexcept
    : doc_it_(&doc_it),
      heap_it_(min_heap_context(doc_it.iterators)) {
  }

  template<typename Func>
  bool reset(Func func) {
    if (!doc_it_->reset(func)) {
      return false;
    }

    heap_it_.reset(doc_it_->iterators.size());
    lead_ = nullptr;

    return true;
  }

  virtual attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    return doc_it_->get_mutable(type);
  }

  virtual bool next() override;

  virtual doc_id_t seek(doc_id_t target) override {
    irs::seek(*this, target);
    return value();
  }

  virtual doc_id_t value() const noexcept override {
    return doc_it_->current_id;
  }

 private:
  class min_heap_context {
   public:
    explicit min_heap_context(compound_doc_iterator::iterators_t& itrs) noexcept
      : itrs_(itrs) {
    }

    // advance
    bool operator()(const size_t i) const {
      assert(i < itrs_.get().size());
      auto& doc_it = itrs_.get()[i];
      auto const& map = *doc_it.second;
      while (doc_it.first->next()) {
        if (!doc_limits::eof(map(doc_it.first->value()))) {
          return true;
        }
      }
      return false;
    }

    // compare
    bool operator()(const size_t lhs, const size_t rhs) const {
      return remap(lhs) > remap(rhs);
    }

   private:
    doc_id_t remap(const size_t i) const {
      assert(i < itrs_.get().size());
      auto& doc_it = itrs_.get()[i];
      return (*doc_it.second)(doc_it.first->value());
    }

    std::reference_wrapper<compound_doc_iterator::iterators_t> itrs_;
  }; // min_heap_context

  compound_doc_iterator* doc_it_;
  external_heap_iterator<min_heap_context> heap_it_;
  compound_doc_iterator::doc_iterator_t* lead_{};
}; // sorting_compound_doc_iterator

bool sorting_compound_doc_iterator::next() {
  auto& iterators = doc_it_->iterators;
  auto& current_id = doc_it_->current_id;

  doc_it_->progress();

  if (doc_it_->aborted()) {
    current_id = doc_limits::eof();
    iterators.clear();
    return false;
  }

  while (heap_it_.next()) {
    auto& new_lead = iterators[heap_it_.value()];
    auto& it = new_lead.first;
    auto& doc_map = *new_lead.second;

    if (&new_lead != lead_) {
      // update attributes
      doc_it_->attribute_change(*it);
      lead_ = &new_lead;
    }

    current_id = doc_map(it->value());

    if (doc_limits::eof(current_id)) {
      continue;
    }

    return true;
  }

  current_id = doc_limits::eof();

  return false;
}

//////////////////////////////////////////////////////////////////////////////
/// @struct compound_iterator
//////////////////////////////////////////////////////////////////////////////
template<typename Iterator>
class compound_iterator {
 public:
  typedef typename std::remove_reference<decltype(Iterator()->value())>::type value_type;

  size_t size() const { return iterators_.size(); }

  void add(const sub_reader& reader,
           const doc_map_f& doc_map) {
    iterator_mask_.emplace_back(iterators_.size());
    iterators_.emplace_back(reader.columns(), reader, doc_map);
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

  const value_type& operator*() const {
    static value_type empty;
    return current_value_ ? *current_value_ : empty;
  }

  bool next() {
    // advance all used iterators
    for (auto id : iterator_mask_) {
      auto& it = iterators_[id].it;
      if (it && !it->next()) {
        it = nullptr;
      }
    }

    iterator_mask_.clear(); // reset for next pass

    for (size_t i = 0, size = iterators_.size(); i < size; ++i) {
      auto& it = iterators_[i].it;
      if (!it) {
        continue; // empty iterator
      }

      const auto& value = it->value();
      const string_ref key = value.name;

      if (!iterator_mask_.empty() && current_key_ < key) {
        continue; // empty field or value too large
      }

      // found a smaller field
      if (iterator_mask_.empty() || key < current_key_) {
        iterator_mask_.clear();
        current_key_ = key;
        current_value_ = &value;
      }

      assert(value == *current_value_); // validated by caller
      iterator_mask_.push_back(i);
    }

    if (!iterator_mask_.empty()) {
      return true;
    }

    current_key_ = string_ref::NIL;

    return false;
  }

 private:
  struct iterator_t : util::noncopyable {
    iterator_t(
        Iterator&& it,
        const sub_reader& reader,
        const doc_map_f& doc_map)
      : it(std::move(it)),
        reader(&reader), 
        doc_map(&doc_map) {
      }

    iterator_t(iterator_t&&) = default;
    iterator_t& operator=(iterator_t&&) = delete;

    Iterator it;
    const sub_reader* reader;
    const doc_map_f* doc_map;
  };

  static_assert(std::is_nothrow_move_constructible_v<iterator_t>);

  const value_type* current_value_{};
  string_ref current_key_;
  std::vector<size_t> iterator_mask_; // valid iterators for current step 
  std::vector<iterator_t> iterators_; // all segment iterators
}; // compound_iterator

//////////////////////////////////////////////////////////////////////////////
/// @class compound_term_iterator
/// @brief iterator over documents for a term over all readers
//////////////////////////////////////////////////////////////////////////////
class compound_term_iterator : public term_iterator {
 public:
  static constexpr const size_t PROGRESS_STEP_TERMS = size_t(1) << 7;

  explicit compound_term_iterator(
      const merge_writer::flush_progress_t& progress,
      const comparer* comparator)
    : doc_itr_(progress),
      psorting_doc_itr_(nullptr == comparator ? nullptr : &sorting_doc_itr_),
      progress_(progress, PROGRESS_STEP_TERMS) {
  }

  bool aborted() const {
    return !static_cast<bool>(progress_) || doc_itr_.aborted();
  }

  void reset(const field_meta& meta) noexcept {
    meta_ = &meta;
    term_iterator_mask_.clear();
    term_iterators_.clear();
    current_term_ = bytes_ref::NIL;
  }

  const field_meta& meta() const noexcept { return *meta_; }
  void add(const term_reader& reader, const doc_map_f& doc_map);
  virtual attribute* get_mutable(irs::type_info::type_id) noexcept override {
    // no way to merge attributes for the same term spread over multiple iterators
    // would require API change for attributes
    assert(false);
    return nullptr;
  }
  virtual bool next() override;
  virtual doc_iterator::ptr postings(IndexFeatures features) const override;
  virtual void read() override {
    for (auto& itr_id: term_iterator_mask_) {
      if (term_iterators_[itr_id].first) {
        term_iterators_[itr_id].first->read();
      }
    }
  }
  virtual const bytes_ref& value() const override {
    return current_term_;
  }
 private:
  struct term_iterator_t {
    seek_term_iterator::ptr first;
    const doc_map_f* second;

    term_iterator_t(
        seek_term_iterator::ptr&& term_itr,
        const doc_map_f* doc_map)
      : first(std::move(term_itr)), second(doc_map) {
    }

    // GCC 8.1.0/8.2.0 optimized code requires an *explicit* noexcept non-inlined
    // move constructor implementation, otherwise the move constructor is fully
    // optimized out (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=87665)
    GCC8_12_OPTIMIZED_WORKAROUND(__attribute__((noinline)))
    term_iterator_t(term_iterator_t&& other) noexcept
      : first(std::move(other.first)), second(std::move(other.second)) {
    }
  };

  compound_term_iterator(const compound_term_iterator&) = delete; // due to references
  compound_term_iterator& operator=(const compound_term_iterator&) = delete; // due to references

  bytes_ref current_term_;
  const field_meta* meta_{};
  std::vector<size_t> term_iterator_mask_; // valid iterators for current term
  std::vector<term_iterator_t> term_iterators_; // all term iterators
  mutable compound_doc_iterator doc_itr_;
  mutable sorting_compound_doc_iterator sorting_doc_itr_{ doc_itr_ };
  sorting_compound_doc_iterator* psorting_doc_itr_;
  progress_tracker progress_;
}; // compound_term_iterator

void compound_term_iterator::add(
    const term_reader& reader,
    const doc_map_f& doc_id_map) {
  term_iterator_mask_.emplace_back(term_iterators_.size()); // mark as used to trigger next()
  term_iterators_.emplace_back(reader.iterator(SeekMode::NORMAL), &doc_id_map);
}

bool compound_term_iterator::next() {
  progress_();

  if (aborted()) {
    term_iterators_.clear();
    term_iterator_mask_.clear();
    return false;
  }

  // advance all used iterators
  for (auto& itr_id: term_iterator_mask_) {
    auto& it = term_iterators_[itr_id].first;
    if (it && !it->next()) {
      it.reset();
    }
  }

  term_iterator_mask_.clear(); // reset for next pass

  for (size_t i = 0, count = term_iterators_.size(); i < count; ++i) {
    auto& term_itr = term_iterators_[i];

    if (!term_itr.first
        || (!term_iterator_mask_.empty() && term_itr.first->value() > current_term_)) {
      continue; // empty iterator or value too large
    }

    // found a smaller term
    if (term_iterator_mask_.empty() || term_itr.first->value() < current_term_) {
      term_iterator_mask_.clear();
      current_term_ = term_itr.first->value();
    }

    term_iterator_mask_.emplace_back(i);
  }

  if (!term_iterator_mask_.empty()) {
    return true;
  }

  current_term_ = bytes_ref::NIL;

  return false;
}

doc_iterator::ptr compound_term_iterator::postings(IndexFeatures /*features*/) const {
  auto add_iterators = [this](compound_doc_iterator::iterators_t& itrs) {
    itrs.clear();
    itrs.reserve(term_iterator_mask_.size());

    for (auto& itr_id : term_iterator_mask_) {
      auto& term_itr = term_iterators_[itr_id];

      itrs.emplace_back(term_itr.first->postings(meta().index_features), term_itr.second);
      assert(itrs.back().first);
    }

    return true;
  };

  doc_iterator* doc_itr = &doc_itr_;

  if (psorting_doc_itr_) {
    sorting_doc_itr_.reset(add_iterators);
    if (doc_itr_.iterators.size() > 1) {
      doc_itr = psorting_doc_itr_;
    }
  } else {
    doc_itr_.reset(add_iterators);
  }

  return memory::to_managed<doc_iterator, false>(doc_itr);
}

//////////////////////////////////////////////////////////////////////////////
/// @struct compound_field_iterator
/// @brief iterator over field_ids over all readers
//////////////////////////////////////////////////////////////////////////////
class compound_field_iterator : public basic_term_reader {
 public:
  static constexpr const size_t PROGRESS_STEP_FIELDS = size_t(1);

  explicit compound_field_iterator(
      const merge_writer::flush_progress_t& progress,
      const comparer* comparator = nullptr)
    : term_itr_(progress, comparator),
      progress_(progress, PROGRESS_STEP_FIELDS) {
  }

  void add(const sub_reader& reader, const doc_map_f& doc_id_map);
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

  virtual const field_meta& meta() const noexcept override {
    assert(current_meta_);
    return *current_meta_;
  }

  virtual const bytes_ref& (min)() const noexcept override {
    return *min_;
  }

  virtual const bytes_ref& (max)() const noexcept override {
    return *max_;
  }

  virtual attribute* get_mutable(irs::type_info::type_id) noexcept override {
    return nullptr;
  }

  virtual term_iterator::ptr iterator() const override;

  bool aborted() const {
    return !static_cast<bool>(progress_) || term_itr_.aborted();
  }

 private:
  struct field_iterator_t : util::noncopyable {
    field_iterator_t(
        field_iterator::ptr&& itr,
        const sub_reader& reader,
        const doc_map_f& doc_map)
      : itr(std::move(itr)),
        reader(&reader),
        doc_map(&doc_map) {
    }

    field_iterator_t(field_iterator_t&&) = default;
    field_iterator_t& operator=(field_iterator_t&&) = delete;

    field_iterator::ptr itr;
    const sub_reader* reader;
    const doc_map_f* doc_map;
  };

  static_assert(std::is_nothrow_move_constructible_v<field_iterator_t>);

  struct term_iterator_t {
    size_t itr_id;
    const field_meta* meta;
    const term_reader* reader;
  };

  string_ref current_field_;
  const field_meta* current_meta_{ &field_meta::EMPTY };
  const bytes_ref* min_{ &bytes_ref::NIL };
  const bytes_ref* max_{ &bytes_ref::NIL };
  std::vector<term_iterator_t> field_iterator_mask_; // valid iterators for current field
  std::vector<field_iterator_t> field_iterators_; // all segment iterators
  mutable compound_term_iterator term_itr_;
  progress_tracker progress_;
}; // compound_field_iterator

typedef compound_iterator<column_iterator::ptr> compound_column_meta_iterator_t;

void compound_field_iterator::add(
    const sub_reader& reader,
    const doc_map_f& doc_id_map) {
  field_iterator_mask_.emplace_back(term_iterator_t{
    field_iterators_.size(),
    nullptr,
    nullptr
  }); // mark as used to trigger next()

  field_iterators_.emplace_back(
    reader.fields(),
    reader, 
    doc_id_map
  );
}

bool compound_field_iterator::next() {
  progress_();

  if (aborted()) {
    field_iterators_.clear();
    field_iterator_mask_.clear();
    current_field_ = string_ref::NIL;
    max_ = min_ = &bytes_ref::NIL;
    return false;
  }

  // advance all used iterators
  for (auto& entry : field_iterator_mask_) {
    auto& it = field_iterators_[entry.itr_id];
    if (it.itr && !it.itr->next()) {
      it.itr = nullptr;
    }
  }

  // reset for next pass
  field_iterator_mask_.clear();
  max_ = min_ = &bytes_ref::NIL;

  for (size_t i = 0, count = field_iterators_.size(); i < count; ++i) {
    auto& field_itr = field_iterators_[i];

    if (!field_itr.itr) {
      continue; // empty iterator
    }

    const auto& field_meta = field_itr.itr->value().meta();
    const auto* field_terms = field_itr.reader->field(field_meta.name);
    const string_ref field_id = field_meta.name;

    if (!field_terms  || 
        (!field_iterator_mask_.empty() && field_id > current_field_)) {
      continue; // empty field or value too large
    }

    // found a smaller field
    if (field_iterator_mask_.empty() || field_id < current_field_) {
      field_iterator_mask_.clear();
      current_field_ = field_id;
      current_meta_ = &field_meta;
    }

    assert(is_subset_of(field_meta.features, meta().features)); // validated by caller
    assert(field_meta.index_features <= meta().index_features); // validated by caller
    field_iterator_mask_.emplace_back(term_iterator_t{i, &field_meta, field_terms});

    // update min and max terms
    min_ = &std::min(*min_, field_terms->min());
    max_ = &std::max(*max_, field_terms->max());
  }

  if (!field_iterator_mask_.empty()) {
    return true;
  }

  current_field_ = string_ref::NIL;

  return false;
}

term_iterator::ptr compound_field_iterator::iterator() const {
  term_itr_.reset(meta());

  for (auto& segment : field_iterator_mask_) {
    term_itr_.add(*(segment.reader), *(field_iterators_[segment.itr_id].doc_map));
  }

  return memory::to_managed<term_iterator, false>(&term_itr_);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief computes fields_type
//////////////////////////////////////////////////////////////////////////////
bool compute_field_meta(
    field_meta_map_t& field_meta_map,
    IndexFeatures& index_features,
    feature_set_t& fields_features,
    const sub_reader& reader) {
  REGISTER_TIMER_DETAILED();

  for (auto it = reader.fields(); it->next();) {
    const auto& field_meta = it->value().meta();
    const auto [field_meta_it, is_new] = field_meta_map.emplace(field_meta.name, &field_meta);

    // validate field_meta equivalence
    if (!is_new &&
        (!is_subset_of(field_meta.index_features, field_meta_it->second->index_features) ||
        !is_subset_of(field_meta.features, field_meta_it->second->features))) {
      return false; // field_meta is not equal, so cannot merge segments
    }

    accumulate_features(fields_features, field_meta.features);
    index_features |= field_meta.index_features;
  }

  return true;
}

//////////////////////////////////////////////////////////////////////////////
/// @struct columnstore
/// @brief Helper class responsible for writing a data from different sources
///        into single columnstore
//////////////////////////////////////////////////////////////////////////////
class columnstore {
 public:
  static constexpr const size_t PROGRESS_STEP_COLUMN = size_t(1) << 13;

  columnstore(
      columnstore_writer::ptr&& writer,
      const merge_writer::flush_progress_t& progress)
    : progress_(progress, PROGRESS_STEP_COLUMN),
      writer_(std::move(writer)) {
  }

  columnstore(
      directory& dir,
      const segment_meta& meta,
      const merge_writer::flush_progress_t& progress)
    : progress_(progress, PROGRESS_STEP_COLUMN) {
    auto writer = meta.codec->get_columnstore_writer(true);
    writer->prepare(dir, meta);

    writer_ = std::move(writer);
  }

  // inserts live values from the specified 'column' and 'reader' into column
  bool insert(
      const sub_reader& reader,
      field_id column,
      const doc_map_f& doc_map) {
    const auto* column_reader = reader.column_reader(column);

    if (!column_reader) {
      // nothing to do
      return true;
    }

    return column_reader->visit(
      [this, &doc_map](doc_id_t doc, const bytes_ref& in) {
        if (!progress_()) {
          // stop was requsted
          return false;
        }

        const auto mapped_doc = doc_map(doc);
        if (doc_limits::eof(mapped_doc)) {
          // skip deleted document
          return true;
        }

        empty_ = false;

        auto& out = column_.second(mapped_doc);
        out.write_bytes(in.c_str(), in.size());
        return true;
    });
  }

  // inserts live values from the specified 'iterator' into column
  bool insert(doc_iterator& it) {
    const payload* payload = nullptr;

    auto* callback = irs::get<attribute_provider_change>(it);

    if (callback) {
      callback->subscribe([&payload](const attribute_provider& attrs) {
        payload = irs::get<irs::payload>(attrs);
      });
    } else {
      payload = irs::get<irs::payload>(it);
    }

    while (it.next()) {
      if (!progress_()) {
        // stop was requsted
        return false;
      }

      auto& out = column_.second(it.value());

      if (payload) {
        out.write_bytes(payload->value.c_str(), payload->value.size());
      }

      empty_ = false;
    }

    return true;
  }

  void reset(const column_info& info) {
    if (!empty_ || info_ != info) {
      info_ = info;
      column_ = writer_->push_column(info);
      empty_ = true;
    }
  }

  // returs 
  //   'true' if object has been initialized sucessfully, 
  //   'false' otherwise
  operator bool() const noexcept { return static_cast<bool>(writer_); }

  // returns 'true' if no data have been written to columnstore
  bool empty() const noexcept { return empty_; }

  // @return was anything actually flushed
  bool flush(const flush_state& state) { return writer_->commit(state); }

  // returns current column identifier
  field_id id() const noexcept { return column_.first; }

 private:
  progress_tracker progress_;
  columnstore_writer::ptr writer_;
  columnstore_writer::column_t column_{};
  column_info info_{{}, {}, false};
  bool empty_{ false };
}; // columnstore

//////////////////////////////////////////////////////////////////////////////
/// @struct sorting_compound_column_iterator
//////////////////////////////////////////////////////////////////////////////
class sorting_compound_column_iterator : util::noncopyable {
 public:
  typedef std::pair<doc_iterator::ptr, const payload*> iterator_t;
  typedef std::vector<iterator_t> iterators_t;

  static bool emplace_back(iterators_t& itrs, const sub_reader& segment) {
    if (!segment.sort()) {
      // sort column is not present, give up
      return false;
    }

    auto it = segment.mask(segment.sort()->iterator());

    if (!it) {
      return false;
    }

    const payload* payload = irs::get<irs::payload>(*it);

    if (!payload) {
      return false;
    }

    itrs.emplace_back(std::move(it), payload);

    return true;
  }

  explicit sorting_compound_column_iterator(const comparer& comparator)
    : heap_it_(min_heap_context(itrs_, comparator)) {
  }

  void reset(iterators_t&& itrs) {
    heap_it_.reset(itrs.size());
    itrs_ = std::move(itrs);
  }

  bool next() {
    return heap_it_.next();
  }

  std::pair<size_t, const iterator_t*> value() const noexcept {
    return std::make_pair(heap_it_.value(), &itrs_[heap_it_.value()]);
  }

 private:
  class min_heap_context {
   public:
    explicit min_heap_context(
        std::vector<iterator_t>& itrs,
        const comparer& less) noexcept
      : itrs_(&itrs), less_(&less) {
    }

    // advance
    bool operator()(const size_t i) const {
      assert(i < itrs_->size());
      return (*itrs_)[i].first->next();
    }

    // compare
    bool operator()(const size_t lhs, const size_t rhs) const {
      assert(lhs < itrs_->size());
      assert(rhs < itrs_->size());
      const auto& lhs_value = (*itrs_)[lhs].second->value;
      const auto& rhs_value = (*itrs_)[rhs].second->value;
      return (*less_)(rhs_value, lhs_value);
    }

   private:
    std::vector<iterator_t>* itrs_;
    const comparer* less_;
  }; // min_heap_context

  std::vector<iterator_t> itrs_;
  external_heap_iterator<min_heap_context> heap_it_;
}; // sorting_compound_column_iterator

//////////////////////////////////////////////////////////////////////////////
/// @brief write columnstore
//////////////////////////////////////////////////////////////////////////////
template<typename CompoundIterator>
bool write_columns(
    columnstore& cs,
    CompoundIterator& columns,
    directory& dir,
    const column_info_provider_t& column_info,
    const segment_meta& meta,
    compound_column_meta_iterator_t& column_meta_itr,
    const merge_writer::flush_progress_t& progress) {
  REGISTER_TIMER_DETAILED();
  assert(cs);
  assert(progress);

  auto add_iterators = [&column_meta_itr](compound_doc_iterator::iterators_t& itrs) {
    auto add_iterators = [&itrs](
        const sub_reader& segment,
        const doc_map_f& doc_map,
        const column_meta& column) {
      auto* reader = segment.column_reader(column.id);

      if (!reader) {
        return false;
      }

      itrs.emplace_back(reader->iterator(), &doc_map);
      return true;
    };

    itrs.clear();
    return column_meta_itr.visit(add_iterators);
  };

  auto column_meta_writer = meta.codec->get_column_meta_writer();
  column_meta_writer->prepare(dir, meta);

  while (column_meta_itr.next()) {
    const auto& column_name = (*column_meta_itr).name;
    cs.reset(column_info(column_name));

    // visit matched columns from merging segments and
    // write all survived values to the new segment
    if (!progress() || !columns.reset(add_iterators)) {
      return false; // failed to visit all values
    }

    if (!cs.insert(columns)) {
      return false; // failed to insert all values
    }

    if (!cs.empty()) {
      column_meta_writer->write(column_name, cs.id());
    }
  }

  column_meta_writer->flush();

  return true;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief write columnstore
//////////////////////////////////////////////////////////////////////////////
bool write_columns(
    columnstore& cs,
    directory& dir,
    const column_info_provider_t& column_info,
    const segment_meta& meta,
    compound_column_meta_iterator_t& column_itr,
    const merge_writer::flush_progress_t& progress) {
  REGISTER_TIMER_DETAILED();
  assert(cs);
  assert(progress);

  auto visitor = [&cs](
      const sub_reader& segment,
      const doc_map_f& doc_map,
      const column_meta& column) {
    return cs.insert(segment, column.id, doc_map);
  };

  auto cmw = meta.codec->get_column_meta_writer();

  cmw->prepare(dir, meta);

  while (column_itr.next()) {
    const auto& column_name = (*column_itr).name;
    cs.reset(column_info(column_name));

    // visit matched columns from merging segments and
    // write all survived values to the new segment 
    if (!progress() || !column_itr.visit(visitor)) {
      return false; // failed to visit all values
    }

    if (!cs.empty()) {
      cmw->write(column_name, cs.id());
    } 
  }

  cmw->flush();

  return true;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief write field term data
//////////////////////////////////////////////////////////////////////////////
bool write_fields(
    columnstore& cs,
    const flush_state& flush_state,
    const segment_meta& meta,
    const feature_column_info_provider_t& column_info,
    compound_field_iterator& field_itr,
    const merge_writer::flush_progress_t& progress) {
  REGISTER_TIMER_DETAILED();
  assert(cs);

  auto field_writer = meta.codec->get_field_writer(true);
  field_writer->prepare(flush_state);

  irs::type_info::type_id feature{};

  auto merge_features = [&cs, &feature] (
      const sub_reader& segment,
      const doc_map_f& doc_map,
      const field_meta& field) {
    const auto column = field.features.find(feature);

    // merge field norms if present
    if (column != field.features.end() &&
        field_limits::valid(column->second) &&
        !cs.insert(segment, column->second, doc_map)) {
      return false;
    }

    return true;
  };

  feature_map_t features;

  while (field_itr.next()) {
    features.clear();

    auto& field_meta = field_itr.meta();

    auto begin = field_meta.features.begin();
    auto end = field_meta.features.end();

    for (; begin != end; ++begin) {
      std::tie(feature, std::ignore) = *begin;

      cs.reset(column_info(feature));

      // remap merge features
      if (!progress() || !field_itr.visit(merge_features)) {
        return false;
      }

      features[feature] = cs.empty()
        ? field_limits::invalid() : cs.id();
    }

    // write field terms
    auto terms = field_itr.iterator();

    field_writer->write(
      field_meta.name,
      field_meta.index_features,
      features,
      *terms);
  }

  field_writer->end();
  field_writer.reset();

  return !field_itr.aborted();
}

//////////////////////////////////////////////////////////////////////////////
/// @brief write field term data
//////////////////////////////////////////////////////////////////////////////
template<typename CompoundIterator>
bool write_fields(
    columnstore& cs,
    CompoundIterator& feature_itr,
    const flush_state& flush_state,
    const segment_meta& meta,
    const feature_column_info_provider_t& column_info,
    compound_field_iterator& field_itr,
    const merge_writer::flush_progress_t& progress) {
  REGISTER_TIMER_DETAILED();
  assert(cs);

  auto field_writer = meta.codec->get_field_writer(true);
  field_writer->prepare(flush_state);

  irs::type_info::type_id feature{};

  auto add_iterators = [&field_itr, &feature](compound_doc_iterator::iterators_t& itrs) {
    auto add_iterators = [&itrs, &feature](
        const sub_reader& segment,
        const doc_map_f& doc_map,
        const field_meta& field) {
      const auto column = field.features.find(feature);

      if (column == field.features.end() ||
          !field_limits::valid(column->second)) {
        // field has no feature
        return true;
      }

      auto* reader = segment.column_reader(column->second);

      if (!reader) {
        return false;
      }

      itrs.emplace_back(reader->iterator(), &doc_map);
      return true;
    };

    itrs.clear();
    return field_itr.visit(add_iterators);
  };

  feature_map_t features;

  while (field_itr.next()) {
    features.clear();
    auto& field_meta = field_itr.meta();

    auto begin = field_meta.features.begin();
    auto end = field_meta.features.end();

    for (; begin != end; ++begin) {
      std::tie(feature, std::ignore) = *begin;

      cs.reset(column_info(feature));

      // remap merge norms
      if (!progress() || !feature_itr.reset(add_iterators)) {
        return false;
      }

      if (!cs.insert(feature_itr)) {
        return false; // failed to insert all values
      }

      features[feature] = cs.empty()
        ? field_limits::invalid()
        : cs.id();
    }

    // write field terms
    auto terms = field_itr.iterator();

    field_writer->write(
      field_meta.name,
      field_meta.index_features,
      field_meta.features,
      *terms);
  }

  field_writer->end();
  field_writer.reset();

  return !field_itr.aborted();
}

//////////////////////////////////////////////////////////////////////////////
/// @brief computes doc_id_map and docs_count
//////////////////////////////////////////////////////////////////////////////
doc_id_t compute_doc_ids(
    doc_id_map_t& doc_id_map,
    const sub_reader& reader,
    doc_id_t next_id) noexcept {
  REGISTER_TIMER_DETAILED();
  // assume not a lot of space wasted if doc_limits::min() > 0
  try {
    doc_id_map.resize(
      reader.docs_count() + doc_limits::min(),
      doc_limits::eof());
  } catch (...) {
    IR_FRMT_ERROR(
      "Failed to resize merge_writer::doc_id_map to accommodate element: " IR_UINT64_T_SPECIFIER,
      reader.docs_count() + doc_limits::min());

    return doc_limits::invalid();
  }

  for (auto docs_itr = reader.docs_iterator(); docs_itr->next(); ++next_id) {
    auto src_doc_id = docs_itr->value();

    assert(src_doc_id >= doc_limits::min());
    assert(src_doc_id < reader.docs_count() + doc_limits::min());
    doc_id_map[src_doc_id] = next_id; // set to next valid doc_id
  }

  return next_id;
}

const merge_writer::flush_progress_t PROGRESS_NOOP = [](){ return true; };

} // LOCAL

namespace iresearch {

merge_writer::reader_ctx::reader_ctx(sub_reader::ptr reader) noexcept
  : reader(reader),
    doc_map([](doc_id_t) noexcept { return doc_limits::eof(); }) {
  assert(reader);
}

merge_writer::merge_writer() noexcept
  : dir_(noop_directory::instance()),
    column_info_(nullptr),
    feature_column_info_(nullptr),
    comparator_(nullptr) {
}

merge_writer::operator bool() const noexcept {
  return &dir_ != &noop_directory::instance();
}

bool merge_writer::flush(
    tracking_directory& dir,
    index_meta::index_segment_t& segment,
    const flush_progress_t& progress) {
  REGISTER_TIMER_DETAILED();
  assert(progress);
  assert(!comparator_);
  assert(column_info_ && *column_info_);
  assert(feature_column_info_ && *feature_column_info_);

  field_meta_map_t field_meta_map;
  compound_field_iterator fields_itr(progress);
  compound_column_meta_iterator_t columns_meta_itr;
  feature_set_t fields_features;
  IndexFeatures index_features{IndexFeatures::NONE};

  doc_id_t base_id = doc_limits::min(); // next valid doc_id

  // collect field meta and field term data
  for (auto& reader_ctx : readers_) {
    assert(reader_ctx.reader);
    auto& reader = *reader_ctx.reader;
    const auto docs_count = reader.docs_count();

    if (reader.live_docs_count() == docs_count) { // segment has no deletes
      const auto reader_base = base_id - doc_limits::min();
      base_id += docs_count;

      reader_ctx.doc_map = [reader_base](doc_id_t doc) noexcept {
        return reader_base + doc;
      };
    } else { // segment has some deleted docs
      auto& doc_id_map = reader_ctx.doc_id_map;
      base_id = compute_doc_ids(doc_id_map , reader, base_id);

      reader_ctx.doc_map = [&doc_id_map](doc_id_t doc) noexcept {
        return doc >= doc_id_map.size()
          ? doc_limits::eof()
          : doc_id_map[doc];
      };
    }

    if (!doc_limits::valid(base_id)) {
      return false; // failed to compute next doc_id
    }

    if (!compute_field_meta(field_meta_map, index_features,
                            fields_features, reader)) {
      return false;
    }

    fields_itr.add(reader, reader_ctx.doc_map);
    columns_meta_itr.add(reader, reader_ctx.doc_map);
  }

  segment.meta.docs_count = base_id - doc_limits::min(); // total number of doc_ids
  segment.meta.live_docs_count = segment.meta.docs_count; // all merged documents are live

  if (!progress()) {
    return false; // progress callback requested termination
  }

  //...........................................................................
  // write merged segment data
  //...........................................................................
  REGISTER_TIMER_DETAILED();
  columnstore cs(dir, segment.meta, progress);

  if (!cs) {
    return false; // flush failure
  }

  if (!progress()) {
    return false; // progress callback requested termination
  }

  // write columns
  if (!write_columns(cs, dir, *column_info_, segment.meta,
                     columns_meta_itr, progress)) {
    return false; // flush failure
  }

  if (!progress()) {
    return false; // progress callback requested termination
  }

  flush_state state;
  state.dir = &dir;
  state.doc_count = segment.meta.docs_count;
  state.features = &fields_features;
  state.index_features = index_features;
  state.name = segment.meta.name;

  // write field meta and field term data
  if (!write_fields(cs, state, segment.meta, *feature_column_info_,
                    fields_itr, progress)) {
    return false; // flush failure
  }

  if (!progress()) {
    return false; // progress callback requested termination
  }

  segment.meta.column_store = cs.flush(state);

  return true;
}

bool merge_writer::flush_sorted(
    tracking_directory& dir,
    index_meta::index_segment_t& segment,
    const flush_progress_t& progress) {
  REGISTER_TIMER_DETAILED();
  assert(progress);
  assert(comparator_);
  assert(column_info_ && *column_info_);
  assert(feature_column_info_ && *feature_column_info_);

  field_meta_map_t field_meta_map;
  compound_column_meta_iterator_t columns_meta_itr;
  compound_field_iterator fields_itr(progress, comparator_);
  feature_set_t fields_features;
  IndexFeatures index_features{IndexFeatures::NONE};

  sorting_compound_column_iterator::iterators_t itrs;
  itrs.reserve(readers_.size());

  //...........................................................................
  // init doc map for each reader
  //...........................................................................

  segment.meta.docs_count = 0;

  for (auto& reader_ctx : readers_) {
    assert(reader_ctx.reader);
    auto& reader = *reader_ctx.reader;

    if (reader.docs_count() > doc_limits::eof() - doc_limits::min()) {
      // can't merge segment holding more than 'doc_limits::eof()-1' docs
      return false;
    }

    if (!sorting_compound_column_iterator::emplace_back(itrs, reader)) {
      // sort column is not present, give up
      return false;
    }

    if (!compute_field_meta(field_meta_map, index_features, fields_features, reader)) {
      return false;
    }

    fields_itr.add(reader, reader_ctx.doc_map);
    columns_meta_itr.add(reader, reader_ctx.doc_map);

    // count total number of documents in consolidated segment
    if (!math::sum_check_overflow(segment.meta.docs_count,
                                  reader.live_docs_count(),
                                  segment.meta.docs_count)) {
      return false;
    }

    // prepare doc maps
    auto& doc_id_map = reader_ctx.doc_id_map;

    try {
      // assume not a lot of space wasted if doc_limits::min() > 0
      doc_id_map.resize(
        reader.docs_count() + doc_limits::min(),
        doc_limits::eof());
    } catch (...) {
      IR_FRMT_ERROR(
        "Failed to resize merge_writer::doc_id_map to accommodate element: " IR_UINT64_T_SPECIFIER,
        reader.docs_count() + doc_limits::min());

      return false;
    }

    reader_ctx.doc_map = [&doc_id_map](doc_id_t doc) noexcept {
      return doc >= doc_id_map.size()
        ? doc_limits::eof()
        : doc_id_map[doc];
    };
  }

  if (segment.meta.docs_count >= doc_limits::eof()) {
    // can't merge segments holding more than 'doc_limits::eof()-1' docs
    return false;
  }

  if (!progress()) {
    return false; // progress callback requested termination
  }

  //...........................................................................
  // write new sorted column and fill doc maps for each reader
  //...........................................................................

  sorting_compound_column_iterator columns_it(*comparator_);
  columns_it.reset(std::move(itrs));

  auto writer = segment.meta.codec->get_columnstore_writer(true);
  writer->prepare(dir, segment.meta);

  // get column info for sorted column
  const auto info = (*column_info_)(string_ref::NIL);
  auto column = writer->push_column(info);

  doc_id_t next_id = doc_limits::min();
  while (columns_it.next()) {
    const auto value = columns_it.value();
    assert(value.second);
    auto& it = *value.second;
    assert(it.second);
    auto& payload = it.second->value;

    // fill doc id map
    readers_[value.first].doc_id_map[it.first->value()] = next_id;

    // write value into new column
    auto& stream = column.second(next_id);
    stream.write_bytes(payload.c_str(), payload.size());

    ++next_id;

    if (!progress()) {
      return false; // progress callback requested termination
    }
  }

#ifdef IRESEARCH_DEBUG
  struct ne_eof {
    bool operator()(doc_id_t doc) const noexcept {
      return !doc_limits::eof(doc);
    }
  };

  // ensure doc ids for each segment are sorted
  for (auto& reader : readers_) {
    auto& doc_map = reader.doc_id_map;
    assert(doc_map.size() >= doc_limits::min());
    assert(
      std::is_sorted(
        boost::make_filter_iterator(ne_eof(), doc_map.begin(), doc_map.end()),
        boost::make_filter_iterator(ne_eof(), doc_map.end(), doc_map.end())));
    UNUSED(doc_map);
  }
#endif

  columnstore cs(std::move(writer), progress);
  compound_doc_iterator doc_it(progress); // reuse iterator
  sorting_compound_doc_iterator sorting_doc_it(doc_it); // reuse iterator

  if (!cs) {
    return false; // flush failure
  }

  if (!progress()) {
    return false; // progress callback requested termination
  }

  // write columns
  if (!write_columns(cs, sorting_doc_it, dir, *column_info_,
                     segment.meta, columns_meta_itr, progress)) {
    return false; // flush failure
  }

  if (!progress()) {
    return false; // progress callback requested termination
  }

  flush_state state;
  state.dir = &dir;
  state.doc_count = segment.meta.docs_count;
  state.index_features = index_features;
  state.features = &fields_features;
  state.name = segment.meta.name;

  // write field meta and field term data
  if (!write_fields(cs, sorting_doc_it, state, segment.meta,
                    *feature_column_info_, fields_itr, progress)) {
    return false; // flush failure
  }

  if (!progress()) {
    return false; // progress callback requested termination
  }

  segment.meta.column_store = cs.flush(state); // flush columnstore
  segment.meta.sort = column.first; // set sort column identifier
  segment.meta.live_docs_count = segment.meta.docs_count; // all merged documents are live

  return true;
}

bool merge_writer::flush(
    index_meta::index_segment_t& segment,
    const flush_progress_t& progress /*= {}*/) {
  REGISTER_TIMER_DETAILED();
  assert(segment.meta.codec); // must be set outside

  bool result = false; // overall flush result

  auto segment_invalidator = irs::make_finally([&result, &segment]() noexcept {
    if (result) {
      // all good
      return;
    }

    // invalidate segment
    segment.filename.clear();
    auto& meta = segment.meta;
    meta.name.clear();
    meta.files.clear();
    meta.column_store = false;
    meta.docs_count = 0;
    meta.live_docs_count = 0;
    meta.size = 0;
    meta.version = 0;
  });

  const auto& progress_callback = progress ? progress : PROGRESS_NOOP;

  tracking_directory track_dir(dir_); // track writer created files

  result = comparator_
    ? flush_sorted(track_dir, segment, progress_callback)
    : flush(track_dir, segment, progress_callback);

  track_dir.flush_tracked(segment.meta.files);

  return result;
}

} // ROOT
