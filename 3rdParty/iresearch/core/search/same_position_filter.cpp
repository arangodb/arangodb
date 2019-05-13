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
#include "same_position_filter.hpp"
#include "term_query.hpp"
#include "conjunction.hpp"

#include "index/field_meta.hpp"

#include "analysis/token_attributes.hpp"
#include "utils/misc.hpp"

#include <boost/functional/hash.hpp>

NS_ROOT

class same_position_iterator final : public conjunction {
 public:
  typedef std::vector<position::ref> positions_t;

  same_position_iterator(
      conjunction::doc_iterators_t&& itrs,
      const order::prepared& ord,
      positions_t&& pos)
    : conjunction(std::move(itrs), ord),
      pos_(std::move(pos)) {
    assert(!pos_.empty());
  }

#if defined(_MSC_VER)
  #pragma warning(disable : 4706)
#elif defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wparentheses"
#endif

  virtual bool next() override {
    bool next = false;
    while (true == (next = conjunction::next()) && !find_same_position()) {}
    return next;
  }

#if defined(_MSC_VER)
  #pragma warning(default : 4706)
#elif defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

  virtual doc_id_t seek(doc_id_t target) override {
    const auto doc = conjunction::seek(target);

    if (doc_limits::eof(doc) || find_same_position()) {
      return doc; 
    }

    next();
    return this->value();
  }

 private:
  bool find_same_position() {
    auto target = pos_limits::min();

    for (auto begin = pos_.begin(), end = pos_.end(); begin != end;) {
      position& pos = *begin;

      if (target != pos.seek(target)) {
        target = pos.value();
        if (pos_limits::eof(target)) {
          return false;
        }
        begin = pos_.begin();
      } else {
        ++begin;
      }
    }

    return true;
  }

  positions_t pos_;
}; // same_position_iterator

// per segment terms state
typedef std::vector<reader_term_state> terms_states_t;

class same_position_query final : public filter::prepared {
 public:
  typedef states_cache<terms_states_t> states_t;
  typedef std::vector<attribute_store> stats_t;

  DECLARE_SHARED_PTR(same_position_query);

  explicit same_position_query(states_t&& states, stats_t&& stats)
    : states_(std::move(states)), stats_(std::move(stats)) {
  }

  using filter::prepared::execute;

  virtual doc_iterator::ptr execute(
      const sub_reader& segment,
      const order::prepared& ord,
      const attribute_view& /*ctx*/) const override {
    // get query state for the specified reader
    auto query_state = states_.find(segment);
    if (!query_state) {
      // invalid state 
      return doc_iterator::empty();
    }

    // get features required for query & order
    auto features = ord.features() | by_same_position::features();

    same_position_iterator::doc_iterators_t itrs;
    itrs.reserve(query_state->size());

    same_position_iterator::positions_t positions;
    positions.reserve(itrs.size());

    auto term_stats = stats_.begin();
    for (auto& term_state : *query_state) {
      auto term = term_state.reader->iterator();

      // use bytes_ref::blank here since we do not need just to "jump"
      // to cached state, and we are not interested in term value itself */
      if (!term->seek(bytes_ref::NIL, *term_state.cookie)) {
        return doc_iterator::empty();
      }

      // get postings
      auto docs = term->postings(features);

      // get needed postings attributes
      auto& pos = docs->attributes().get<position>();
      if (!pos) {
        // positions not found
        return doc_iterator::empty();
      }
      positions.emplace_back(std::ref(*pos));

      // add base iterator
      itrs.emplace_back(doc_iterator::make<basic_doc_iterator>(
        segment,
        *term_state.reader,
        *term_stats,
        std::move(docs), 
        ord, 
        term_state.estimation
      ));

      ++term_stats;
    }

    return make_conjunction<same_position_iterator>(
      std::move(itrs), ord, std::move(positions)
    );
  }

 private:
  states_t states_;
  stats_t stats_;
}; // same_position_query

DEFINE_FILTER_TYPE(by_same_position)
DEFINE_FACTORY_DEFAULT(by_same_position)

/* static */ const flags& by_same_position::features() {
  static flags features{ frequency::type(), position::type() };
  return features;
}

by_same_position::by_same_position() 
  : filter(by_same_position::type()) {
}

bool by_same_position::equals(const filter& rhs) const NOEXCEPT {
  const auto& trhs = static_cast<const by_same_position&>(rhs);
  return filter::equals(rhs) && terms_ == trhs.terms_;
}

size_t by_same_position::hash() const NOEXCEPT {
  size_t seed = 0;
  ::boost::hash_combine(seed, filter::hash());
  for (auto& term : terms_) {
    ::boost::hash_combine(seed, term.first);
    ::boost::hash_combine(seed, term.second);
  }
  return seed;
}
  
by_same_position& by_same_position::push_back(
    const std::string& field, 
    const bstring& term) {
  terms_.emplace_back(field, term);
  return *this;
}

by_same_position& by_same_position::push_back(
    const std::string& field, 
    bstring&& term) {
  terms_.emplace_back(field, std::move(term));
  return *this;
}

by_same_position& by_same_position::push_back(
    std::string&& field,
    const bstring& term) {
  terms_.emplace_back(std::move(field), term);
  return *this;
}

by_same_position& by_same_position::push_back(
    std::string&& field,
    bstring&& term) {
  terms_.emplace_back(std::move(field), std::move(term));
  return *this;
}

filter::prepared::ptr by_same_position::prepare(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost,
    const attribute_view& /*ctx*/) const {
  if (terms_.empty()) {
    // empty field or phrase
    return filter::prepared::empty();
  }

  // per segment query state
  same_position_query::states_t query_states(index.size());

  // per segment terms states
  terms_states_t term_states;
  term_states.reserve(terms_.size());

  // prepare phrase stats (collector for each term)
  std::vector<order::prepared::collectors> term_stats;
  term_stats.reserve(terms_.size());

  for(auto size = terms_.size(); size; --size) {
    term_stats.emplace_back(ord.prepare_collectors(1)); // 1 term per attribute_store because a range is treated as a disjunction
  }

  for (const auto& segment : index) {
    auto term_itr = term_stats.begin();

    for (const auto& branch : terms_) {
      auto next_stats = irs::make_finally([&term_itr]()->void{ ++term_itr; });

      // get term dictionary for field
      const term_reader* field = segment.field(branch.first);
      if (!field) {
        continue;
      }

      // check required features
      if (!features().is_subset_of(field->meta().features)) {
        continue;
      }

      term_itr->collect(segment, *field); // collect field statistics once per segment

      // find terms
      seek_term_iterator::ptr term = field->iterator();
      // get term metadata
      auto& meta = term->attributes().get<term_meta>();

      if (!term->seek(branch.second)) {
        if (ord.empty()) {
          break;
        } else {
          // continue here because we should collect 
          // stats for other terms in phrase
          continue;
        }
      }

      term->read(); // read term attributes
      term_itr->collect(segment, *field, 0, term->attributes()); // collect statistics, 0 because only 1 term
      term_states.emplace_back();

      auto& state = term_states.back();

      state.cookie = term->cookie();
      state.estimation = meta ? meta->docs_count : cost::MAX;
      state.reader = field;
    }

    if (term_states.size() != terms_.size()) {
      // we have not found all needed terms
      term_states.clear();
      continue;
    }

    auto& state = query_states.insert(segment);
    state = std::move(term_states);

    term_states.reserve(terms_.size());
  }

  // finish stats
  same_position_query::stats_t stats(terms_.size());
  auto stat_itr = stats.begin();
  auto term_itr = term_stats.begin();
  assert(term_stats.size() == terms_.size()); // initialized above

  for (size_t i = 0, size = terms_.size(); i < size; ++i) {
    term_itr->finish(*stat_itr, index);
    ++stat_itr;
    ++term_itr;
  }

  auto q = memory::make_shared<same_position_query>(
    std::move(query_states),
    std::move(stats)
  );

  // apply boost
  irs::boost::apply(q->attributes(), this->boost() * boost);

  return q;
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
