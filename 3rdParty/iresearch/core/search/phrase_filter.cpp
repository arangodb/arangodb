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
////////////////////////////////////////////////////////////////////////////////

#include "phrase_filter.hpp"

#include "index/field_meta.hpp"
#include "search/collectors.hpp"
#include "search/filter_visitor.hpp"
#include "search/phrase_iterator.hpp"

namespace {

using namespace irs;

template<typename StateType>
using phrase_state = std::vector<StateType>;

//////////////////////////////////////////////////////////////////////////////
/// @class fixed_phrase_state
/// @brief cached per reader phrase state
//////////////////////////////////////////////////////////////////////////////
struct fixed_phrase_state : util::noncopyable {
  // mimic std::pair interface
  struct term_state {
    term_state(seek_term_iterator::cookie_ptr&& first,
               boost_t /*second*/) noexcept
      : first(std::move(first)) {
    }

    seek_term_iterator::cookie_ptr first;
  };

  phrase_state<term_state> terms;
  const term_reader* reader{};
}; // fixed_phrase_state

static_assert(std::is_nothrow_move_constructible_v<fixed_phrase_state>);
static_assert(std::is_nothrow_move_assignable_v<fixed_phrase_state>);

//////////////////////////////////////////////////////////////////////////////
/// @class variadic_phrase_state
/// @brief cached per reader phrase state
//////////////////////////////////////////////////////////////////////////////
struct variadic_phrase_state : fixed_phrase_state {
  using term_state = std::pair<seek_term_iterator::cookie_ptr, boost_t>;

  std::vector<size_t> num_terms; // number of terms per phrase part
  phrase_state<term_state> terms;
  const term_reader* reader{};
  bool volatile_boost{};
}; // variadic_phrase_state

static_assert(std::is_nothrow_move_constructible_v<variadic_phrase_state>);
static_assert(std::is_nothrow_move_assignable_v<variadic_phrase_state>);

struct get_visitor {
  using result_type = field_visitor;

  result_type operator()(const by_term_options& part) const {
    return [term = bytes_ref(part.term)](
        const sub_reader& segment,
        const term_reader& field,
        filter_visitor& visitor) {
      return by_term::visit(segment, field, term, visitor);
    };
  }

  result_type operator()(const by_prefix_options& part) const {
    return [term = bytes_ref(part.term)](
        const sub_reader& segment,
        const term_reader& field,
        filter_visitor& visitor) {
      return by_prefix::visit(segment, field, term, visitor);
    };
  }

  result_type operator()(const by_wildcard_options& part) const {
    return by_wildcard::visitor(part.term);
  }

  result_type operator()(const by_edit_distance_filter_options& part) const {
    return by_edit_distance::visitor(part);
  }

  result_type operator()(const by_terms_options& part) const {
    return [terms = &part.terms](
        const sub_reader& segment,
        const term_reader& field,
        filter_visitor& visitor) {
      return by_terms::visit(segment, field, *terms, visitor);
    };
  }

  result_type operator()(const by_range_options& part) const {
    return [range = &part.range](
        const sub_reader& segment,
        const term_reader& field,
        filter_visitor& visitor) {
      return by_range::visit(segment, field, *range, visitor);
    };
  }

  template<typename T>
  result_type operator()(const T&) const {
    assert(false);
    return [](const sub_reader&, const term_reader&, filter_visitor&) { };
  }
}; // get_visitor

struct prepare : util::noncopyable {
  using result_type = filter::prepared::ptr;

  result_type operator()(const by_term_options& opts) const {
    return by_term::prepare(index, order, boost, field, opts.term);
  }

  result_type operator()(const by_prefix_options& part) const {
    return by_prefix::prepare(
      index, order, boost, field,
      part.term, part.scored_terms_limit);
  }

  result_type operator()(const by_wildcard_options& part) const {
    return by_wildcard::prepare(
      index, order, boost, field,
      part.term, part.scored_terms_limit);
  }

  result_type operator()(const by_edit_distance_filter_options& part) const {
    return by_edit_distance::prepare(
      index, order, boost, field, part.term,
      0, // collect all terms
      part.max_distance, part.provider,
      part.with_transpositions);
  }

  result_type operator()(const by_terms_options& /*part*/) const {
    return nullptr; // FIXME
  }

  result_type operator()(const by_range_options& part) const {
    return by_range::prepare(
      index, order, boost, field,
       part.range, part.scored_terms_limit);
  }

  template<typename T>
  result_type operator()(const T&) const {
    assert(false);
    return filter::prepared::empty();
  }

  prepare(const index_reader& index,
          const order::prepared& order,
          const std::string& field,
          const boost_t boost) noexcept
    : index(index), order(order),
      field(field), boost(boost) {
  }

  const index_reader& index;
  const irs::order::prepared& order;
  const string_ref field;
  const boost_t boost;
}; // prepare

}

namespace iresearch {

//////////////////////////////////////////////////////////////////////////////
/// @class phrase_term_visitor
/// @brief filter visitor for phrase queries
//////////////////////////////////////////////////////////////////////////////
template<typename PhraseStates>
class phrase_term_visitor final : public filter_visitor,
                                  private util::noncopyable {
 public:
  explicit phrase_term_visitor(PhraseStates& phrase_states) noexcept
    : phrase_states_(phrase_states) {
  }

  virtual void prepare(const sub_reader& segment,
                       const term_reader& field,
                       const seek_term_iterator& terms) noexcept override {
    segment_ = &segment;
    reader_ = &field;
    terms_ = &terms;
    found_ = true;
  }

  virtual void visit(boost_t boost) override {
    assert(terms_ && collectors_ && segment_ && reader_);

    // disallow negative boost
    boost = std::max(0.f, boost);

    if (stats_size_ <= term_offset_) {
      // variadic phrase case
      collectors_->push_back();
      assert(stats_size_ == term_offset_);
      ++stats_size_;
      volatile_boost_ |= (boost != no_boost());
    }

    collectors_->collect(*segment_, *reader_, term_offset_++, *terms_);
    phrase_states_.emplace_back(terms_->cookie(), boost);
  }

  void reset() noexcept {
    volatile_boost_ = false;
  }

  void reset(term_collectors& collectors) noexcept {
    found_ = false;
    terms_ = nullptr;
    term_offset_ = 0;
    collectors_ = &collectors;
    stats_size_ = collectors.size();
  }

  bool found() const noexcept { return found_; }

  bool volatile_boost() const noexcept { return volatile_boost_; }

 private:
  size_t term_offset_ = 0;
  size_t stats_size_ = 0;
  const sub_reader* segment_{};
  const term_reader* reader_{};
  PhraseStates& phrase_states_;
  term_collectors* collectors_ = nullptr;
  const seek_term_iterator* terms_ = nullptr;
  bool found_ = false;
  bool volatile_boost_ = false;
};

}

namespace iresearch {

//////////////////////////////////////////////////////////////////////////////
/// @class phrase_query
/// @brief prepared phrase query implementation
//////////////////////////////////////////////////////////////////////////////
template<typename State>
class phrase_query : public filter::prepared {
 public:
  using states_t = states_cache<State>;
  using positions_t = std::vector<position::value_t>;

  phrase_query(
      states_t&& states,
      positions_t&& positions,
      bstring&& stats,
      boost_t boost) noexcept
    : prepared(boost),
      states_(std::move(states)),
      positions_(std::move(positions)),
      stats_(std::move(stats)) {
  }

 protected:
  states_t states_;
  positions_t positions_;
  bstring stats_;
}; // phrase_query

class fixed_phrase_query : public phrase_query<fixed_phrase_state> {
 public:
  fixed_phrase_query(
      states_t&& states, positions_t&& positions,
      bstring&& stats, boost_t boost) noexcept
    : phrase_query<fixed_phrase_state>(
        std::move(states),  std::move(positions),
        std::move(stats), boost) {
  }

  using filter::prepared::execute;

  doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord,
      const attribute_provider* /*ctx*/) const override {
    using conjunction_t = conjunction<doc_iterator::ptr>;
    using phrase_iterator_t = phrase_iterator<
      conjunction_t,
      fixed_phrase_frequency>;

    // get phrase state for the specified reader
    auto phrase_state = states_.find(rdr);

    if (!phrase_state) {
      // invalid state
      return doc_iterator::empty();
    }

    // get features required for query & order
    auto features = ord.features() | by_phrase::required();

    conjunction_t::doc_iterators_t itrs;
    itrs.reserve(phrase_state->terms.size());

    std::vector<fixed_phrase_frequency::term_position_t> positions;
    positions.reserve(phrase_state->terms.size());

    // find term using cached state
    auto terms = phrase_state->reader->iterator();
    auto position = positions_.begin();

    for (const auto& term_state : phrase_state->terms) {
      assert(term_state.first);

      // use bytes_ref::blank here since we do not need just to "jump"
      // to cached state, and we are not interested in term value itself
      if (!terms->seek(bytes_ref::NIL, *term_state.first)) {
        return doc_iterator::empty();
      }

      auto docs = terms->postings(features); // postings

      auto* pos = irs::get_mutable<irs::position>(docs.get());

      if (!pos) {
        // positions not found
        return doc_iterator::empty();
      }

      positions.emplace_back(std::ref(*pos), *position);

      // add base iterator
      itrs.emplace_back(std::move(docs));

      ++position;
    }

    return memory::make_managed<phrase_iterator_t>(
        std::move(itrs),
        std::move(positions),
        rdr,
        *phrase_state->reader,
        stats_.c_str(),
        ord,
        boost());
  }
}; // fixed_phrase_query

class variadic_phrase_query : public phrase_query<variadic_phrase_state> {
 public:
  using conjunction_t = conjunction<doc_iterator::ptr>;

  // FIXME add proper handling of overlapped case
  template<bool VolatileBoost>
  using phrase_iterator_t = phrase_iterator<conjunction_t,
                                            variadic_phrase_frequency<VolatileBoost>>;

  variadic_phrase_query(
      states_t&& states, positions_t&& positions,
      bstring&& stats, boost_t boost) noexcept
    : phrase_query<variadic_phrase_state>(
        std::move(states), std::move(positions),
        std::move(stats), boost) {
  }

  using filter::prepared::execute;

  doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord,
      const attribute_provider* /*ctx*/) const override {
    using adapter_t = variadic_phrase_adapter;
    using disjunction_t = disjunction<doc_iterator::ptr, adapter_t, true>;
    using compound_doc_iterator_t = irs::compound_doc_iterator<adapter_t>;

    // get phrase state for the specified reader
    auto phrase_state = states_.find(rdr);

    if (!phrase_state) {
      // invalid state
      return doc_iterator::empty();
    }

    // get features required for query & order
    const auto features = ord.features() | by_phrase::required();

    conjunction_t::doc_iterators_t conj_itrs;
    conj_itrs.reserve(phrase_state->terms.size());

    const auto phrase_size = phrase_state->num_terms.size();

    std::vector<variadic_term_position> positions;
    positions.resize(phrase_size);

    // find term using cached state
    auto terms = phrase_state->reader->iterator();
    auto position = positions_.begin();

    auto term_state = phrase_state->terms.begin();
    for (size_t i = 0; i < phrase_size; ++i) {
      const auto num_terms = phrase_state->num_terms[i];
      auto& pos = positions[i];
      pos.second = *position;

      disjunction_t::doc_iterators_t disj_itrs;
      disj_itrs.reserve(num_terms);
      for (const auto end = term_state + num_terms; term_state != end; ++term_state) {
        assert(term_state->first);
        // use bytes_ref::NIL here since we do not need just to "jump"
        // to cached state, and we are not interested in term value itself
        if (!terms->seek(bytes_ref::NIL, *term_state->first)) {
          continue;
        }

        disjunction_t::adapter docs(terms->postings(features),
                                    term_state->second);

        if (!docs.position) {
          // positions not found
          continue;
        }

        disj_itrs.emplace_back(std::move(docs));
      }

      if (disj_itrs.empty()) {
        return doc_iterator::empty();
      }

      auto disj = make_disjunction<disjunction_t>(std::move(disj_itrs));
      #ifdef IRESEARCH_DEBUG
        pos.first = dynamic_cast<compound_doc_iterator_t*>(disj.get());
        assert(pos.first);
      #else
        pos.first = static_cast<compound_doc_iterator_t*>(disj.get());
      #endif
      conj_itrs.emplace_back(std::move(disj));
      ++position;
    }
    assert(term_state == phrase_state->terms.end());

    if (phrase_state->volatile_boost) {
      return memory::make_managed<phrase_iterator_t<true>>(
        std::move(conj_itrs),
        std::move(positions),
        rdr,
        *phrase_state->reader,
        stats_.c_str(),
        ord,
        boost());
    }

    return memory::make_managed<phrase_iterator_t<false>>(
      std::move(conj_itrs),
      std::move(positions),
      rdr,
      *phrase_state->reader,
      stats_.c_str(),
      ord,
      boost());
  }
}; // variadic_phrase_query

// -----------------------------------------------------------------------------
// --SECTION--                                          by_phrase implementation
// -----------------------------------------------------------------------------

/* static */ const flags& by_phrase::required() {
  static const flags req{ irs::type<frequency>::get(), irs::type<position>::get() };
  return req;
}

DEFINE_FACTORY_DEFAULT(by_phrase)

filter::prepared::ptr by_phrase::prepare(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost,
    const attribute_provider* /*ctx*/) const {
  if (field().empty() || options().empty()) {
    // empty field or phrase
    return filter::prepared::empty();
  }

  if (1 == options().size()) {
    auto query = std::visit(
      ::prepare{index, ord, field(), this->boost()*boost},
      options().begin()->second);

    if (query) {
      return query;
    }
  }

  // prepare phrase stats (collector for each term)
  if (options().simple()) {
    return fixed_prepare_collect(index, ord, boost);
  }

  return variadic_prepare_collect(index, ord, boost);
}

filter::prepared::ptr by_phrase::fixed_prepare_collect(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost) const {
  const auto phrase_size = options().size();
  const auto is_ord_empty = ord.empty();

  // stats collectors
  field_collectors field_stats(ord);
  term_collectors term_stats(ord, phrase_size);

  // per segment phrase states
  fixed_phrase_query::states_t phrase_states(index.size());

  // per segment phrase terms
  phrase_state<fixed_phrase_state::term_state> phrase_terms;
  phrase_terms.reserve(phrase_size);

  // iterate over the segments
  const string_ref field = this->field();

  phrase_term_visitor<decltype(phrase_terms)> ptv(phrase_terms);

  for (const auto& segment : index) {
    // get term dictionary for field
    const auto* reader = segment.field(field);

    if (!reader) {
      continue;
    }

    // check required features
    if (!by_phrase::required().is_subset_of(reader->meta().features)) {
      continue;
    }

    field_stats.collect(segment, *reader); // collect field statistics once per segment
    ptv.reset(term_stats);

    for (const auto& word : options()) {
      assert(std::get_if<by_term_options>(&word.second));
      by_term::visit(segment, *reader, std::get<by_term_options>(word.second).term, ptv);
      if (!ptv.found()) {
        if (is_ord_empty) {
          break;
        }
        // continue here because we should collect
        // stats for other terms in phrase
      }
    }

    // we have not found all needed terms
    if (phrase_terms.size() != phrase_size) {
      phrase_terms.clear();
      continue;
    }

    auto& state = phrase_states.insert(segment);
    state.terms = std::move(phrase_terms);
    state.reader = reader;

    phrase_terms.reserve(phrase_size);
  }

  // offset of the first term in a phrase
  assert(!options().empty());
  const size_t base_offset = options().begin()->first;

  // finish stats
  bstring stats(ord.stats_size(), 0); // aggregated phrase stats
  auto* stats_buf = const_cast<byte_type*>(stats.data());

  fixed_phrase_query::positions_t positions(phrase_size);
  auto pos_itr = positions.begin();

  size_t term_idx = 0;
  for (const auto& term : options()) {
    *pos_itr = position::value_t(term.first - base_offset);
    term_stats.finish(stats_buf, term_idx, field_stats, index);
    ++pos_itr;
    ++term_idx;
  }

  return memory::make_managed<fixed_phrase_query>(
    std::move(phrase_states),
    std::move(positions),
    std::move(stats),
    this->boost() * boost);
}

filter::prepared::ptr by_phrase::variadic_prepare_collect(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost) const {
  const auto phrase_size = options().size();

  // stats collectors
  field_collectors field_stats(ord);

  std::vector<field_visitor> phrase_part_visitors;
  phrase_part_visitors.reserve(phrase_size);
  std::vector<term_collectors> phrase_part_stats;
  phrase_part_stats.reserve(phrase_size);
  for (const auto& word : options()) {
    phrase_part_stats.emplace_back(ord, 0);
    phrase_part_visitors.emplace_back(std::visit(get_visitor{}, word.second));
  }

  // per segment phrase states
  variadic_phrase_query::states_t phrase_states(index.size());

  // per segment phrase terms
  std::vector<size_t> num_terms(phrase_size); // number of terms per part
  phrase_state<variadic_phrase_state::term_state> phrase_terms;
  phrase_terms.reserve(phrase_size); // reserve space for at least 1 term per part

  // iterate over the segments
  const string_ref field = this->field();
  const auto is_ord_empty = ord.empty();

  phrase_term_visitor<decltype(phrase_terms)> ptv(phrase_terms);

  for (const auto& segment : index) {
    // get term dictionary for field
    const auto* reader = segment.field(field);

    if (!reader) {
      continue;
    }

    // check required features
    if (!by_phrase::required().is_subset_of(reader->meta().features)) {
      continue;
    }

    ptv.reset(); // reset boost volaitility mark
    field_stats.collect(segment, *reader); // collect field statistics once per segment

    size_t part_offset = 0;
    size_t found_words_count = 0;

    for (const auto& visitor: phrase_part_visitors) {
      const auto terms_count = phrase_terms.size();

      ptv.reset(phrase_part_stats[part_offset]);
      visitor(segment, *reader, ptv);
      if (!ptv.found()) {
        if (is_ord_empty) {
          break;
        }
        // continue here because we should collect
        // stats for other terms in phrase
      } else {
        ++found_words_count;
      }

      // number of terms per phrase part
      num_terms[part_offset] = phrase_terms.size() - terms_count;
      ++part_offset;
    }

    // we have not found all needed terms
    if (found_words_count != phrase_size) {
      phrase_terms.clear();
      continue;
    }

    auto& state = phrase_states.insert(segment);
    state.terms = std::move(phrase_terms);
    state.num_terms = std::move(num_terms);
    state.reader = reader;
    state.volatile_boost = !is_ord_empty && ptv.volatile_boost();
    assert(phrase_size == state.num_terms.size());

    phrase_terms.reserve(phrase_size);
    num_terms.resize(phrase_size); // reserve space for at least 1 term per part
  }

  // offset of the first term in a phrase
  assert(!options().empty());
  const size_t base_offset = options().begin()->first;

  // finish stats
  assert(phrase_size == phrase_part_stats.size());
  bstring stats(ord.stats_size(), 0); // aggregated phrase stats
  auto* stats_buf = const_cast<byte_type*>(stats.data());
  auto collector = phrase_part_stats.begin();

  variadic_phrase_query::positions_t positions(phrase_size);
  auto pos_itr = positions.begin();

  for (const auto& term : options()) {
    *pos_itr = position::value_t(term.first - base_offset);
    for (size_t term_idx = 0, size = collector->size(); term_idx < size; ++term_idx) {
      collector->finish(stats_buf, term_idx, field_stats, index);
    }
    ++pos_itr;
    ++collector;
  }

  return memory::make_managed<variadic_phrase_query>(
    std::move(phrase_states),
    std::move(positions),
    std::move(stats),
    this->boost() * boost);
}

} // ROOT
