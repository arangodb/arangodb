////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_MULTITERM_FILTER_H
#define IRESEARCH_MULTITERM_FILTER_H

#include "filter.hpp"
#include "index/index_reader.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @brief base class for implementations selecting which terms get included
///        while collecting index statistics
//////////////////////////////////////////////////////////////////////////////
class term_selector {
 public:
  term_selector(const index_reader& index, const order::prepared& order)
    : collector_(order.prepare_stats()), index_(index) {
  }

  virtual ~term_selector() {}

  virtual void build(attribute_store& attrs) = 0;

  virtual void insert(
    const sub_reader& segment,
    const term_reader& field,
    const term_iterator& term
  ) = 0;

 protected:
  const order::prepared::stats collector_;
  const index_reader& index_;
};

//////////////////////////////////////////////////////////////////////////////
/// @brief class for selecting all terms for inclusion into index statistics
//////////////////////////////////////////////////////////////////////////////
class all_term_selector: public term_selector {
 public:
  all_term_selector(const index_reader& index, const order::prepared& order);

  virtual void build(attribute_store& attrs) override;

  virtual void insert(
    const sub_reader& segment,
    const term_reader& field,
    const term_iterator& term
  ) override;
};

//////////////////////////////////////////////////////////////////////////////
/// @brief class for selecting terms from fields with the highest term count
///        for inclusion included into index statistics
//////////////////////////////////////////////////////////////////////////////
class limited_term_selector_by_field_size: public term_selector {
 public:
  limited_term_selector_by_field_size(
    const index_reader& index, const order::prepared& order, size_t limit
  );

  virtual void build(attribute_store& attrs) override;

  virtual void insert(
    const sub_reader& segment,
    const term_reader& field,
    const term_iterator& term
  ) override;

 private:
  struct state_t {
    const sub_reader& segment;
    const term_reader& field;
    seek_term_iterator::ptr term;
  };

  std::unordered_multimap<size_t, const term_reader*> candidates_; // map by significance
  size_t limit_;
  std::unordered_multimap<const term_reader*, state_t> states_;
};

//////////////////////////////////////////////////////////////////////////////
/// @brief class for selecting terms with the longest postings list for
///        inclusion into index statistics
//////////////////////////////////////////////////////////////////////////////
class limited_term_selector_by_postings_size: public term_selector {
 public:
  limited_term_selector_by_postings_size(
    const index_reader& index, const order::prepared& order, size_t limit
  );

  virtual void build(attribute_store& attrs) override;

  virtual void insert(
    const sub_reader& segment,
    const term_reader& field,
    const term_iterator& term
  ) override;

 private:
  struct state_t {
    const sub_reader& segment;
    const term_reader& field;
    seek_term_iterator::ptr term;
  };

  size_t limit_;
  std::unordered_multimap<size_t, state_t> states_; // map by significance
};

//////////////////////////////////////////////////////////////////////////////
/// @brief class for selecting terms from segments with the highest document
///        count for inclusion included into index statistics
//////////////////////////////////////////////////////////////////////////////
class limited_term_selector_by_segment_live_docs: public term_selector {
 public:
  limited_term_selector_by_segment_live_docs(
    const index_reader& index, const order::prepared& order, size_t limit
  );

  virtual void build(attribute_store& attrs) override;

  virtual void insert(
    const sub_reader& segment,
    const term_reader& field,
    const term_iterator& term
  ) override;

 private:
  struct state_t {
    const sub_reader& segment;
    const term_reader& field;
    seek_term_iterator::ptr term;
  };

  std::unordered_multimap<size_t, const sub_reader*> candidates_; // map by significance
  size_t limit_;
  std::unordered_multimap<const sub_reader*, state_t> states_;
};

//////////////////////////////////////////////////////////////////////////////
/// @brief base class for term filters e.g. by_prefix/by_range/by_term
//////////////////////////////////////////////////////////////////////////////
template<typename Selector>
class multiterm_filter: public filter {
 public:
  explicit multiterm_filter(const type_id& type) NOEXCEPT;

  const std::string& field() const {
    return field_;
  }

  virtual filter::prepared::ptr prepare(
      const index_reader& index,
      const order::prepared& order,
      boost_t boost
  ) const override {
    const irs::string_ref field_ref(field_); // avoid casting to irs::string_ref
    Selector selector(index, order);

    for (auto& segment: index) {
      const auto* field = segment.field(field_ref);

      if (!field) {
        continue; // no such field in the segment
      }

      collect_terms(selector, segment, *field);
    }

    attribute_store attrs;

    irs::boost::apply(attrs, this->boost() * boost); // apply boost
    selector.build(attrs);

    return build(std::move(attrs));
  }

 protected:
  virtual filter::prepared::ptr build(attribute_store&& attrs) = 0;

  virtual void collect_terms(
    Selector& selector,
    const sub_reader& segment,
    const term_reader& field
  ) const = 0;

  multiterm_filter& field(irs::string_ref const& field) {
    field_ = field_;

    return *this;
  }

  multiterm_filter& field(std::string&& field) {
    field_ = std::move(field_);

    return *this;
  }

 private:
  std::string field_;
}; // multiterm_filter

NS_END // ROOT

#endif