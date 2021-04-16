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

#ifndef IRESEARCH_ASSERT_FORMAT_H
#define IRESEARCH_ASSERT_FORMAT_H

#include <set>

#include "doc_generator.hpp"
#include "index/field_meta.hpp"
#include "index/comparer.hpp"
#include "formats/formats.hpp"

namespace tests {

struct position {
  position(uint32_t pos, uint32_t start,
           uint32_t end, const irs::bytes_ref& pay);

  bool operator<(const position& rhs) const {
    return pos < rhs.pos;
  }

  uint32_t pos;
  uint32_t start;
  uint32_t end;
  irs::bstring payload;
};

class posting {
 public:
  posting(irs::doc_id_t id);
  posting(irs::doc_id_t id, std::set<position>&& positions)
    : positions_(std::move(positions)), id_(id) {
  }
  posting(posting&& rhs) noexcept
    : positions_(std::move(rhs.positions_)),
      id_(rhs.id_) {
  }
  posting& operator=(posting&& rhs) noexcept {
    if (this != &rhs) {
      positions_ = std::move(rhs.positions_);
      id_ = rhs.id_;
    }
    return *this;
  }

  void add(uint32_t pos, uint32_t offs_start, const irs::attribute_provider& attrs);

  bool operator<(const posting& rhs) const {
    return id_ < rhs.id_;
  }

  const std::set<position>& positions() const { return positions_; }
  irs::doc_id_t id() const { return id_; }
  size_t size() const { return positions_.size(); }

 private:
  friend struct term;

  std::set<position> positions_;
  irs::doc_id_t id_;
};

struct term {
  term(const irs::bytes_ref& data);

  posting& add(irs::doc_id_t id);

  bool operator<(const term& rhs) const;

  uint64_t docs_count() const { return postings.size(); }

  void sort(const std::map<irs::doc_id_t, irs::doc_id_t>& docs) {
    std::set<posting> resorted_postings;

    for (auto& posting : postings) {
      resorted_postings.emplace(
        docs.at(posting.id_),
        std::move(const_cast<tests::posting&>(posting).positions_)
      );
    }

    postings = std::move(resorted_postings);
  }

  std::set<posting> postings;
  irs::bstring value;
};

class field : public irs::field_meta {
 public:
  field(const irs::string_ref& name, const irs::flags& features);
  field(field&& rhs) noexcept;
  field& operator=(field&& rhs) noexcept;

  term& add(const irs::bytes_ref& term);
  term* find(const irs::bytes_ref& term);
  size_t remove(const irs::bytes_ref& t);
  void sort(const std::map<irs::doc_id_t, irs::doc_id_t>& docs) {
    for (auto& term : terms) {
      const_cast<tests::term&>(term).sort(docs);
    }
  }

  std::set<term> terms;
  std::unordered_set<irs::doc_id_t> docs;
  uint32_t pos;
  uint32_t offs;
};

class index_segment: irs::util::noncopyable {
 public:
  typedef std::map<irs::string_ref, field> field_map_t;
  typedef field_map_t::const_iterator iterator;

  index_segment();
  index_segment(index_segment&& rhs) noexcept;
  index_segment& operator=(index_segment&& rhs) noexcept;
   
  size_t doc_count() const { return count_; }
  size_t size() const { return fields_.size(); }

  const irs::document_mask& doc_mask() const { return doc_mask_; }
  const field_map_t& fields() const { return fields_; }

  bool find(const irs::string_ref& name, const irs::bytes_ref& term) {
    field* fld = find( name );
    return fld && fld->find(term);
  }

  const field* find(size_t id) const {
    return id_to_field_.at(id);
  }

  field* find(const irs::string_ref& name) {
    auto it = fields_.find( name );
    return it == fields_.end()?nullptr:&it->second;
  }

  const field* find(const irs::string_ref& name) const {
    auto it = fields_.find( name );
    return it == fields_.end()?nullptr:&it->second;
  }

  template<typename Iterator>
  void add(Iterator begin, Iterator end, ifield::ptr sorted = nullptr) {
    // reset field per-document state
    for (auto it = begin; it != end; ++it) {
      auto* field_data = find((*it).name());

      if (!field_data) {
        continue;
      }

      field_data->pos = 0;
      field_data->offs = 0;
    }

    for (; begin != end; ++begin) {
      add(*begin);
    }

    if (sorted) {
      add_sorted(*sorted);
    }

    ++count_;
  }

  void sort(const irs::comparer& comparator) {
    if (sort_.empty()) {
      return;
    }

    std::sort(
      sort_.begin(), sort_.end(),
      [&comparator](
          const std::pair<irs::bstring, irs::doc_id_t>& lhs,
          const std::pair<irs::bstring, irs::doc_id_t>& rhs) {
        return comparator(lhs.first, rhs.first);
    });

    irs::doc_id_t new_doc_id = irs::doc_limits::min();
    std::map<irs::doc_id_t, irs::doc_id_t> order;
    for (auto& entry : sort_) {
      order[entry.second] = new_doc_id++;
    }

    for (auto& field : fields_) {
      field.second.sort(order);
    }
  }

  void clear() {
    fields_.clear();
    count_ = 0;
  }

 private:
  void add(const ifield& field);
  void add_sorted(const ifield& field);

  std::vector<std::pair<irs::bstring, irs::doc_id_t>> sort_;
  std::vector<const field*> id_to_field_;
  field_map_t fields_;
  size_t count_;
  irs::document_mask doc_mask_;
};

class term_reader : public irs::term_reader {
 public:
  term_reader(const tests::field& data)
    : data_(data),
      min_(data_.terms.begin()->value),
      max_(data_.terms.rbegin()->value) {
    if (meta().features.check<irs::frequency>()) {
      for (auto& term : data.terms) {
        for (auto& p : term.postings) {
          freq_.value += p.positions().size();
        }
      }
      pfreq_ = &freq_;
    }
  }

  virtual irs::seek_term_iterator::ptr iterator() const override;
  virtual irs::seek_term_iterator::ptr iterator(irs::automaton_table_matcher& a) const override;
  virtual const irs::field_meta& meta() const override { return data_; }
  virtual size_t size() const override { return data_.terms.size(); }
  virtual uint64_t docs_count() const override { return data_.docs.size(); }
  virtual const irs::bytes_ref& (min)() const override { return min_; }
  virtual const irs::bytes_ref& (max)() const override { return max_; }
  virtual size_t bit_union(
    const cookie_provider& provider,
    size_t* bitset) const override;
  virtual irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    if (irs::type<irs::frequency>::id() == type) {
      return pfreq_;
    }
    return nullptr;
  }

 private:
  const tests::field& data_;
  irs::frequency freq_;
  irs::frequency* pfreq_{};
  irs::bytes_ref min_;
  irs::bytes_ref max_;
};

struct index_meta_writer: public irs::index_meta_writer {
  virtual std::string filename(
    const irs::index_meta& meta
  ) const override;
  virtual bool prepare(
    irs::directory& dir,
    irs::index_meta& meta
  ) override;
  virtual bool commit() override;
  virtual void rollback() noexcept override;
};

struct index_meta_reader : public irs::index_meta_reader {
  virtual bool last_segments_file(
    const irs::directory& dir, std::string& out
  ) const override;
  virtual void read(
    const irs::directory& dir,
    irs::index_meta& meta,
    const irs::string_ref& filename = irs::string_ref::NIL
  ) override;
};

struct segment_meta_writer : public irs::segment_meta_writer {
  virtual void write(
    irs::directory& dir,
    std::string& filename,
    const irs::segment_meta& meta
  ) override;
};

struct segment_meta_reader : public irs::segment_meta_reader {
  virtual void read(
    const irs::directory& dir,
    irs::segment_meta& meta,
    const irs::string_ref& filename = irs::string_ref::NIL
  ) override;
};

class document_mask_writer: public irs::document_mask_writer {
 public:
  document_mask_writer(const index_segment& data);
  virtual std::string filename(
    const irs::segment_meta& meta
  ) const override;

  void write(
    irs::directory& dir,
    const irs::segment_meta& meta,
    const irs::document_mask& docs_mask
  ) override;

 private:
  const index_segment& data_;
};

class field_reader : public irs::field_reader {
 public:
  field_reader( const index_segment& data );
  field_reader(field_reader&& other) noexcept;

  virtual void prepare(const irs::directory& dir, const irs::segment_meta& meta, const irs::document_mask& mask) override;
  virtual const irs::term_reader* field(const irs::string_ref& field) const override;
  virtual irs::field_iterator::ptr iterator() const override;
  virtual size_t size() const override;
  
  const index_segment& data() const {
    return data_;
  }

 private:
  std::vector<irs::term_reader::ptr> readers_;
  const index_segment& data_;
};

class field_writer : public irs::field_writer {
 public:
  field_writer(const index_segment& data, const irs::flags& features = irs::flags());

  /* returns features which should be checked
   * in "write" method */
  irs::flags features() const { return features_; }

  /* sets features which should be checked
   * in "write" method */
  void features(const irs::flags& features) { features_ = features; }

  virtual void prepare(const irs::flush_state& state) override;
  virtual void write(const std::string& name, irs::field_id norm, const irs::flags& expected_field, irs::term_iterator& actual_term) override;
  virtual void end() override;

 private:
  field_reader readers_;
  irs::flags features_;
};

class format : public irs::format {
 public:
  DECLARE_FACTORY();
  format();
  format(const index_segment& data);

  virtual irs::index_meta_writer::ptr get_index_meta_writer() const override;
  virtual irs::index_meta_reader::ptr get_index_meta_reader() const override;

  virtual irs::segment_meta_writer::ptr get_segment_meta_writer() const override;
  virtual irs::segment_meta_reader::ptr get_segment_meta_reader() const override;

  virtual document_mask_writer::ptr get_document_mask_writer() const override;
  virtual irs::document_mask_reader::ptr get_document_mask_reader() const override;

  virtual irs::field_writer::ptr get_field_writer(bool volatile_attributes) const override;
  virtual irs::field_reader::ptr get_field_reader() const override;

  virtual irs::column_meta_writer::ptr get_column_meta_writer() const override;
  virtual irs::column_meta_reader::ptr get_column_meta_reader() const override;

  virtual irs::columnstore_writer::ptr get_columnstore_writer() const override;
  virtual irs::columnstore_reader::ptr get_columnstore_reader() const override;

 private:
  static const index_segment DEFAULT_SEGMENT;
  const index_segment& data_;
};

typedef std::vector<index_segment> index_t;

void assert_term(
  const irs::term_iterator& expected_term,
  const irs::term_iterator& actual_term,
  const irs::flags& features);

void assert_terms_next(
  const irs::term_reader& expected_term_reader,
  const irs::term_reader& actual_term_reader,
  irs::automaton_table_matcher* acceptor,
  const irs::flags& features);

void assert_terms_seek(
  const irs::term_reader& expected_term_reader,
  const irs::term_reader& actual_term_reader,
  const irs::flags& features,
  irs::automaton_table_matcher* acceptor,
  size_t lookahead = 10); // number of steps to iterate after the seek

void assert_index(
  const index_t& expected_index,
  const irs::index_reader& actual_index,
  const irs::flags& features,
  size_t skip = 0, // do not validate the first 'skip' segments
  irs::automaton_table_matcher* matcher = nullptr
);

void assert_index(
  const irs::directory& dir,
  irs::format::ptr codec,
  const index_t& index,
  const irs::flags& features,
  size_t skip = 0, // no not validate the first 'skip' segments
  irs::automaton_table_matcher* matcher = nullptr
);

} // tests

#endif
