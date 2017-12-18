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
#include <unordered_map>

#include "merge_writer.hpp"
#include "index/field_meta.hpp"
#include "index/index_meta.hpp"
#include "index/segment_reader.hpp"
#include "utils/directory_utils.hpp"
#include "utils/log.hpp"
#include "utils/type_limits.hpp"
#include "utils/version_utils.hpp"
#include "store/store_utils.hpp"

#include <array>

NS_LOCAL

// mapping of old doc_id to new doc_id (reader doc_ids are sequential 0 based)
// masked doc_ids have value of MASKED_DOC_ID
typedef std::vector<irs::doc_id_t> doc_id_map_t;

// mapping of old field_id to new field_id
typedef std::vector<irs::field_id> id_map_t;

typedef std::unordered_map<irs::string_ref, const irs::field_meta*> field_meta_map_t;

const irs::doc_id_t MASKED_DOC_ID = irs::integer_traits<irs::doc_id_t>::const_max; // masked doc_id (ignore)

//////////////////////////////////////////////////////////////////////////////
/// @class compound_attributes
/// @brief compound view of multiple attributes as a single object
//////////////////////////////////////////////////////////////////////////////
class compound_attributes: public irs::attribute_view {
 public:
  void add(const irs::attribute_view& attributes) {
    auto visitor = [this](
        const irs::attribute::type_id& type_id,
        const irs::attribute_view::ref<irs::attribute>::type&
    ) ->bool {
#if defined(__GNUC__) && (__GNUC__ < 5)
      // GCCs before 5 are unable to call protected
      // member of base class from lambda
      this->insert(type_id);
#else
      bool inserted;
      attribute_map::emplace(inserted, type_id);
#endif // defined(__GNUC__) && (__GNUC__ < 5)
      return true;
    };

    attributes.visit(visitor); // add
  }

  void set(const irs::attribute_view& attributes) {
    auto visitor_unset = [](
      const irs::attribute::type_id&,
      irs::attribute_view::ref<irs::attribute>::type& value
    )->bool {
      value = nullptr;
      return true;
    };
    auto visitor_update = [this](
      const irs::attribute::type_id& type_id,
      const irs::attribute_view::ref<irs::attribute>::type& value
    )->bool {
#if defined(__GNUC__) && (__GNUC__ < 5)
      // GCCs before 5 are unable to call protected
      // member of base class from lambda
      this->insert(type_id, value);
#else
      bool inserted;
      attribute_map::emplace(inserted, type_id) = value;
#endif // defined(__GNUC__) && (__GNUC__ < 5)
      return true;
    };

    visit(visitor_unset); // unset
    attributes.visit(visitor_update); // set
  }

#if defined(__GNUC__) && (__GNUC__ < 5)
 private:
  void insert(const irs::attribute::type_id& type_id) {
    bool inserted;
    attribute_map::emplace(inserted, type_id);
  }

  void insert(
      const irs::attribute::type_id& type_id,
      const irs::attribute_view::ref<irs::attribute>::type& value) {
    bool inserted;
    attribute_map::emplace(inserted, type_id) = value;
  }
#endif // defined(__GNUC__) && (__GNUC__ < 5)
}; // compound_attributes

//////////////////////////////////////////////////////////////////////////////
/// @struct compound_doc_iterator
/// @brief iterator over doc_ids for a term over all readers
//////////////////////////////////////////////////////////////////////////////
struct compound_doc_iterator : public irs::doc_iterator {
  typedef std::pair<irs::doc_iterator::ptr, const doc_id_map_t*> doc_iterator_t;

  void reset() NOEXCEPT {
    iterators.clear();
    current_id = irs::type_limits<irs::type_t::doc_id_t>::invalid();
    current_itr = 0;
  }

  void add(irs::doc_iterator::ptr&& postings, const doc_id_map_t& doc_id_map) {
    if (iterators.empty()) {
      attrs.set(postings->attributes()); // add keys and set values
    } else {
      attrs.add(postings->attributes()); // only add missing keys
    }

    iterators.emplace_back(std::move(postings), &doc_id_map);
  }

  virtual const irs::attribute_view& attributes() const NOEXCEPT override {
    return attrs;
  }

  virtual bool next() override;

  virtual irs::doc_id_t seek(irs::doc_id_t target) override {
    irs::seek(*this, target);
    return value();
  }

  virtual irs::doc_id_t value() const override {
    return current_id;
  }

  compound_attributes attrs;
  std::vector<doc_iterator_t> iterators;
  irs::doc_id_t current_id{ irs::type_limits<irs::type_t::doc_id_t>::invalid() };
  size_t current_itr{ 0 };
}; // compound_doc_iterator

bool compound_doc_iterator::next() {
  for (
    bool update_attributes = false;
    current_itr < iterators.size();
    update_attributes = true, ++current_itr
  ) {
    auto& itr_entry = iterators[current_itr];
    auto& itr = itr_entry.first;
    auto& id_map = itr_entry.second;

    if (!itr) {
      continue;
    }

    if (update_attributes) {
      attrs.set(itr->attributes());
    }

    while (itr->next()) {
      auto doc = itr->value();

      if (doc >= id_map->size()) {
        continue; // invalid doc_id
      }

      current_id = (*id_map)[doc];

      if (current_id == MASKED_DOC_ID) {
        continue; // masked doc_id
      }

      return true;
    }

    itr.reset();
  }

  current_id = MASKED_DOC_ID;
  attrs.set(irs::attribute_view::empty_instance());

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

  void add(const irs::sub_reader& reader,
           const doc_id_map_t& doc_id_map) {
    iterator_mask_.emplace_back(iterators_.size());
    iterators_.emplace_back(reader.columns(), reader, doc_id_map);
  }

  // visit matched iterators
  template<typename Visitor>
  bool visit(const Visitor& visitor) const {
    for (auto id : iterator_mask_) {
      auto& it = iterators_[id];
      if (!visitor(*it.reader, *it.doc_id_map, it.it->value())) {
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
      const irs::string_ref key = value.name;

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

    current_key_ = irs::string_ref::nil;

    return false;
  }

 private:
  struct iterator_t {
    iterator_t(
        Iterator&& it,
        const irs::sub_reader& reader,
        const doc_id_map_t& doc_id_map)
      : it(std::move(it)),
        reader(&reader), 
        doc_id_map(&doc_id_map) {
      }

    iterator_t(iterator_t&& other) NOEXCEPT
      : it(std::move(other.it)),
        reader(std::move(other.reader)),
        doc_id_map(std::move(other.doc_id_map)) {
    }

    Iterator it;
    const irs::sub_reader* reader;
    const doc_id_map_t* doc_id_map;
  };

  const value_type* current_value_{};
  irs::string_ref current_key_;
  std::vector<size_t> iterator_mask_; // valid iterators for current step 
  std::vector<iterator_t> iterators_; // all segment iterators
}; // compound_iterator

//////////////////////////////////////////////////////////////////////////////
/// @class compound_term_iterator
/// @brief iterator over documents for a term over all readers
//////////////////////////////////////////////////////////////////////////////
class compound_term_iterator : public irs::term_iterator {
 public:
  compound_term_iterator()
    : doc_itr_(std::make_shared<compound_doc_iterator>()) {
  }

  void reset(const irs::field_meta& meta) NOEXCEPT {
    meta_ = &meta;
    term_iterator_mask_.clear();
    term_iterators_.clear();
    current_term_ = irs::bytes_ref::nil;
  }

  compound_term_iterator& operator=(const compound_term_iterator&) = delete; // due to references
  const irs::field_meta& meta() const NOEXCEPT { return *meta_; }
  void add(const irs::term_reader& reader, const doc_id_map_t& doc_id_map);
  virtual const irs::attribute_view& attributes() const NOEXCEPT override {
    // no way to merge attributes for the same term spread over multiple iterators
    // would require API change for attributes
    assert(false);
    return irs::attribute_view::empty_instance();
  }
  virtual bool next() override;
  virtual irs::doc_iterator::ptr postings(const irs::flags& features) const override;
  virtual void read() override {
    for (auto& itr_id: term_iterator_mask_) {
      if (term_iterators_[itr_id].first) {
        term_iterators_[itr_id].first->read();
      }
    }
  }
  virtual const irs::bytes_ref& value() const override {
    return current_term_;
  }
 private:
  typedef std::pair<irs::seek_term_iterator::ptr, const doc_id_map_t*> term_iterator_t;

  irs::bytes_ref current_term_;
  const irs::field_meta* meta_;
  std::vector<size_t> term_iterator_mask_; // valid iterators for current term
  std::vector<term_iterator_t> term_iterators_; // all term iterators
  irs::doc_iterator::ptr doc_itr_;
}; // compound_term_iterator

void compound_term_iterator::add(
    const irs::term_reader& reader,
    const doc_id_map_t& doc_id_map) {
  term_iterator_mask_.emplace_back(term_iterators_.size()); // mark as used to trigger next()
  term_iterators_.emplace_back(reader.iterator(), &doc_id_map);
}

bool compound_term_iterator::next() {
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

  current_term_ = irs::bytes_ref::nil;

  return false;
}

irs::doc_iterator::ptr compound_term_iterator::postings(const irs::flags& /*features*/) const {
  auto& doc_itr = static_cast<compound_doc_iterator&>(*doc_itr_);

  doc_itr.reset();

  for (auto& itr_id : term_iterator_mask_) {
    auto& term_itr = term_iterators_[itr_id];

    doc_itr.add(term_itr.first->postings(meta().features), *(term_itr.second));
  }

  return doc_itr_;
}

//////////////////////////////////////////////////////////////////////////////
/// @struct compound_field_iterator
/// @brief iterator over field_ids over all readers
//////////////////////////////////////////////////////////////////////////////
class compound_field_iterator : public irs::basic_term_reader {
 public:
  void add(const irs::sub_reader& reader, const doc_id_map_t& doc_id_map);
  bool next();
  size_t size() const { return field_iterators_.size(); }

  // visit matched iterators
  template<typename Visitor>
  bool visit(const Visitor& visitor) const {
    for (auto& entry : field_iterator_mask_) {
      auto& itr = field_iterators_[entry.itr_id];
      if (!visitor(*itr.reader, *itr.doc_id_map, *entry.meta)) {
        return false;
      }
    }
    return true;
  }

  virtual const irs::field_meta& meta() const NOEXCEPT override {
    assert(current_meta_);
    return *current_meta_;
  }

  virtual const irs::bytes_ref& (min)() const NOEXCEPT override {
    return *min_;
  }

  virtual const irs::bytes_ref& (max)() const NOEXCEPT override {
    return *max_;
  }

  virtual const irs::attribute_view& attributes() const NOEXCEPT override {
    return irs::attribute_view::empty_instance();
  }

  virtual irs::term_iterator::ptr iterator() const override;

 private:
  struct field_iterator_t {
    field_iterator_t(
        irs::field_iterator::ptr&& v_itr,
        const irs::sub_reader& v_reader,
        const doc_id_map_t& v_doc_id_map)
      : itr(std::move(v_itr)),
        reader(&v_reader), 
        doc_id_map(&v_doc_id_map) {
    }
    field_iterator_t(field_iterator_t&& other) NOEXCEPT
      : itr(std::move(other.itr)),
        reader(std::move(other.reader)),
        doc_id_map(std::move(other.doc_id_map)) {
    }

    irs::field_iterator::ptr itr;
    const irs::sub_reader* reader;
    const doc_id_map_t* doc_id_map;
  };
  struct term_iterator_t {
    size_t itr_id;
    const irs::field_meta* meta;
    const irs::term_reader* reader;
  };
  irs::string_ref current_field_;
  const irs::field_meta* current_meta_{ &irs::field_meta::EMPTY };
  const irs::bytes_ref* min_{ &irs::bytes_ref::nil };
  const irs::bytes_ref* max_{ &irs::bytes_ref::nil };
  std::vector<term_iterator_t> field_iterator_mask_; // valid iterators for current field
  std::vector<field_iterator_t> field_iterators_; // all segment iterators
  mutable compound_term_iterator term_itr_;
}; // compound_field_iterator

typedef compound_iterator<irs::column_iterator::ptr> compound_column_iterator_t;

void compound_field_iterator::add(
    const irs::sub_reader& reader,
    const doc_id_map_t& doc_id_map) {   
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
  // advance all used iterators
  for (auto& entry : field_iterator_mask_) {
    auto& it = field_iterators_[entry.itr_id];
    if (it.itr && !it.itr->next()) {
      it.itr = nullptr;
    }
  }

  // reset for next pass
  field_iterator_mask_.clear();
  max_ = min_ = &irs::bytes_ref::nil;

  for (size_t i = 0, count = field_iterators_.size(); i < count; ++i) {
    auto& field_itr = field_iterators_[i];

    if (!field_itr.itr) {
      continue; // empty iterator
    }

    const auto& field_meta = field_itr.itr->value().meta();
    const auto* field_terms = field_itr.reader->field(field_meta.name);
    const irs::string_ref field_id = field_meta.name;

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

    assert(field_meta.features.is_subset_of(meta().features)); // validated by caller
    field_iterator_mask_.emplace_back(term_iterator_t{i, &field_meta, field_terms});

    // update min and max terms
    min_ = &std::min(*min_, field_terms->min());
    max_ = &std::max(*max_, field_terms->max());
  }

  if (!field_iterator_mask_.empty()) {
    return true;
  }

  current_field_ = irs::string_ref::nil;

  return false;
}

irs::term_iterator::ptr compound_field_iterator::iterator() const {
  term_itr_.reset(meta());

  for (auto& segment : field_iterator_mask_) {
    term_itr_.add(*(segment.reader), *(field_iterators_[segment.itr_id].doc_id_map));
  }

  return irs::memory::make_managed<irs::term_iterator, false>(&term_itr_);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief computes doc_id_map and docs_count
//////////////////////////////////////////////////////////////////////////////
irs::doc_id_t compute_doc_ids(
  doc_id_map_t& doc_id_map,
  const irs::sub_reader& reader,
  irs::doc_id_t next_id
) NOEXCEPT {
  // assume not a lot of space wasted if type_limits<type_t::doc_id_t>::min() > 0
  try {
    doc_id_map.resize(reader.docs_count() + irs::type_limits<irs::type_t::doc_id_t>::min(), MASKED_DOC_ID);
  } catch (...) {
    IR_FRMT_ERROR(
      "Failed to resize merge_writer::doc_id_map to accommodate element: " IR_UINT64_T_SPECIFIER,
      reader.docs_count() + irs::type_limits<irs::type_t::doc_id_t>::min()
    );
    return irs::type_limits<irs::type_t::doc_id_t>::invalid();
  }

  for (auto docs_itr = reader.docs_iterator(); docs_itr->next(); ++next_id) {
    auto src_doc_id = docs_itr->value();

    assert(src_doc_id >= irs::type_limits<irs::type_t::doc_id_t>::min());
    assert(src_doc_id < reader.docs_count() + irs::type_limits<irs::type_t::doc_id_t>::min());
    doc_id_map[src_doc_id] = next_id; // set to next valid doc_id
  }

  return next_id;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief computes fields_type and fields_count
//////////////////////////////////////////////////////////////////////////////
bool compute_field_meta(
    field_meta_map_t& field_meta_map,
    irs::flags& fields_features,
    const irs::sub_reader& reader) {
  for (auto it = reader.fields(); it->next();) {
    const auto& field_meta = it->value().meta();
    auto field_meta_map_itr = field_meta_map.emplace(field_meta.name, &field_meta);
    auto* tracked_field_meta = field_meta_map_itr.first->second;

    // validate field_meta equivalence
    if (!field_meta_map_itr.second &&
        !field_meta.features.is_subset_of(tracked_field_meta->features)) {
      return false; // field_meta is not equal, so cannot merge segments
    }

    fields_features |= field_meta.features;
  }

  return true;
}

//////////////////////////////////////////////////////////////////////////////
/// @struct columnstore
/// @brief Helper class responsible for writing a data from different sources
///        into single columnstore
///
///        Using by
///         'write' function to merge field norms values from different segments
///         'write_columns' function to merge columns values from different segmnets
//////////////////////////////////////////////////////////////////////////////
class columnstore {
 public:
  columnstore(irs::directory& dir, const irs::segment_meta& meta) {
    auto writer = meta.codec->get_columnstore_writer();

    if (!writer->prepare(dir, meta)) {
      return; // flush failure
    }

    writer_ = std::move(writer);
  }

  // inserts live values from the specified 'column' and 'reader' into column
  bool insert(
      const irs::sub_reader& reader,
      irs::field_id column,
      const doc_id_map_t& doc_id_map) {
    const auto* column_reader = reader.column_reader(column);

    if (!column_reader) {
      // nothing to do
      return true;
    }

    return column_reader->visit(
      [this, &doc_id_map](irs::doc_id_t doc, const irs::bytes_ref& in) {
        const auto mapped_doc = doc_id_map[doc];
        if (MASKED_DOC_ID == mapped_doc) {
          // skip deleted document
          return true;
        }

        empty_ = false;

        auto& out = column_.second(mapped_doc);
        out.write_bytes(in.c_str(), in.size());
        return true;
    });
  }

  void reset() {
    if (!empty_) {
      column_ = writer_->push_column();
      empty_ = true;
    }
  }

  // returs 
  //   'true' if object has been initialized sucessfully, 
  //   'false' otherwise
  operator bool() const { return static_cast<bool>(writer_); }

  // returns 'true' if no data has been written to columnstore
  bool empty() const { return empty_; }

  // @return was anything actually flushed
  bool flush() { return writer_->flush(); }

  // returns current column identifier
  irs::field_id id() const { return column_.first; }

 private:
  irs::columnstore_writer::ptr writer_;
  irs::columnstore_writer::column_t column_{};
  bool empty_{ false };
}; // columnstore

bool write_columns(
    columnstore& cs,
    irs::directory& dir,
    const irs::segment_meta& meta,
    compound_column_iterator_t& column_itr
) {
  assert(cs);

  auto visitor = [&cs](
      const irs::sub_reader& segment,
      const doc_id_map_t& doc_id_map,
      const irs::column_meta& column) {
    return cs.insert(segment, column.id, doc_id_map);
  };

  auto cmw = meta.codec->get_column_meta_writer();

  if (!cmw->prepare(dir, meta)) {
    // failed to prepare writer
    return false;
  }

  while (column_itr.next()) {
    cs.reset();  

    // visit matched columns from merging segments and
    // write all survived values to the new segment 
    column_itr.visit(visitor); 

    if (!cs.empty()) {
      cmw->write((*column_itr).name, cs.id());
    } 
  }
  cmw->flush();

  return true;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief write field term data
//////////////////////////////////////////////////////////////////////////////
bool write(
    columnstore& cs,
    irs::directory& dir,
    const irs::segment_meta& meta,
    compound_field_iterator& field_itr,
    const field_meta_map_t& field_meta_map,
    const irs::flags& fields_features
) {
  REGISTER_TIMER_DETAILED();
  assert(cs);

  irs::flush_state flush_state;
  flush_state.dir = &dir;
  flush_state.doc_count = meta.docs_count;
  flush_state.fields_count = field_meta_map.size();
  flush_state.features = &fields_features;
  flush_state.name = meta.name;
  flush_state.ver = IRESEARCH_VERSION;

  auto fw = meta.codec->get_field_writer(true);
  fw->prepare(flush_state);

  auto merge_norms = [&cs] (
      const irs::sub_reader& segment,
      const doc_id_map_t& doc_id_map,
      const irs::field_meta& field) {
    // merge field norms if present
    if (irs::type_limits<irs::type_t::field_id_t>::valid(field.norm)) {
      cs.insert(segment, field.norm, doc_id_map);
    }

    return true;
  };

  while (field_itr.next()) {
    cs.reset();

    auto& field_meta = field_itr.meta();
    auto& field_features = field_meta.features;

    // remap merge norms
    field_itr.visit(merge_norms); 
   
    // write field terms
    auto terms = field_itr.iterator();

    fw->write(
      field_meta.name,
      cs.empty() ? irs::type_limits<irs::type_t::field_id_t>::invalid() : cs.id(),
      field_features,
      *terms
    );
  }

  fw->end();

  fw.reset();

  return true;
}

NS_END // LOCAL

NS_ROOT

merge_writer::merge_writer(directory& dir, const string_ref& name) NOEXCEPT
  : dir_(dir), name_(name) {
}

void merge_writer::add(const sub_reader& reader) {
  readers_.emplace_back(&reader);
}

bool merge_writer::flush(std::string& filename, segment_meta& meta) {
  REGISTER_TIMER_DETAILED();
  // reader with map of old doc_id to new doc_id
  typedef std::pair<const irs::sub_reader*, doc_id_map_t> reader_t;

  std::unordered_map<irs::string_ref, const irs::field_meta*> field_metas;
  compound_field_iterator fields_itr;
  compound_column_iterator_t columns_itr;
  irs::flags fields_features;
  doc_id_t next_id = type_limits<type_t::doc_id_t>::min(); // next valid doc_id
  std::deque<reader_t> readers; // a container that does not copy when growing (iterators store pointers)

  // collect field meta and field term data
  for (auto& reader: readers_) {
    readers.emplace_back(reader, doc_id_map_t());

    if (!compute_field_meta(field_metas, fields_features, *reader)) {
      return false;
    }

    auto& doc_id_map = readers.back().second;

    next_id = compute_doc_ids(doc_id_map, *reader, next_id);

    if (!irs::type_limits<irs::type_t::doc_id_t>::valid(next_id)) {
      return false; // failed to compute next doc_id
    }

    fields_itr.add(*reader, doc_id_map);
    columns_itr.add(*reader, doc_id_map);
  }

  meta.docs_count = next_id - type_limits<type_t::doc_id_t>::min(); // total number of doc_ids

  //...........................................................................
  // write merged segment data
  //...........................................................................

  tracking_directory track_dir(dir_); // track writer created files
  columnstore cs(track_dir, meta);

  if (!cs) {
    return false; // flush failure
  }

  // write columns
  if (!write_columns(cs, track_dir, meta, columns_itr)) {
    return false; // flush failure
  }

  // write field meta and field term data
  if (!write(cs, track_dir, meta, fields_itr, field_metas, fields_features)) {
    return false; // flush failure
  }

  meta.column_store = cs.flush();

  // ...........................................................................
  // write segment meta
  // ...........................................................................
  if (!track_dir.swap_tracked(meta.files)) {
    IR_FRMT_ERROR("Failed to swap list of tracked files in: %s", __FUNCTION__);
    return false;
  }

  auto writer = meta.codec->get_segment_meta_writer();

  writer->write(dir_, meta);
  filename = writer->filename(meta);

  // ...........................................................................
  // finish/cleanup
  // ...........................................................................
  readers_.clear();

  return true;
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
