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

#include "same_position_filter.hpp"

#include <boost/functional/hash.hpp>

#include "shared.hpp"
#include "term_query.hpp"
#include "collectors.hpp"
#include "conjunction.hpp"
#include "index/field_meta.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/misc.hpp"

namespace {

using namespace irs;

using conjunction_t = conjunction<irs::doc_iterator::ptr> ;

class same_position_iterator final : public conjunction_t {
 public:
  typedef std::vector<position::ref> positions_t;

  same_position_iterator(
      conjunction_t::doc_iterators_t&& itrs,
      const order::prepared& ord,
      positions_t&& pos)
    : conjunction_t(std::move(itrs), ord),
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
    while (true == (next = conjunction_t::next()) && !find_same_position()) {}
    return next;
  }

#if defined(_MSC_VER)
  #pragma warning(default : 4706)
#elif defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

  virtual doc_id_t seek(doc_id_t target) override {
    const auto doc = conjunction_t::seek(target);

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

class same_position_query final : public filter::prepared {
 public:
  typedef std::vector<term_query::term_state> terms_states_t;
  typedef states_cache<terms_states_t> states_t;
  typedef std::vector<bstring> stats_t;

  explicit same_position_query(
      states_t&& states,
      stats_t&& stats,
      boost_t boost)
    : prepared(boost),
      states_(std::move(states)),
      stats_(std::move(stats)) {
  }

  using filter::prepared::execute;

  virtual doc_iterator::ptr execute(
      const sub_reader& segment,
      const order::prepared& ord,
      const attribute_provider* /*ctx*/) const override {
    // get query state for the specified reader
    auto query_state = states_.find(segment);
    if (!query_state) {
      // invalid state
      return doc_iterator::empty();
    }

    // get features required for query & order
    auto features = ord.features() | by_same_position::required();

    same_position_iterator::doc_iterators_t itrs;
    itrs.reserve(query_state->size());

    same_position_iterator::positions_t positions;
    positions.reserve(itrs.size());

    const bool no_score = ord.empty();
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
      auto* pos = irs::get_mutable<position>(docs.get());

      if (!pos) {
        // positions not found
        return doc_iterator::empty();
      }

      positions.emplace_back(std::ref(*pos));

      if (!no_score) {
        auto* score = irs::get_mutable<irs::score>(docs.get());

        if (score) {
          order::prepared::scorers scorers(
            ord, segment, *term_state.reader,
            term_stats->c_str(), score->realloc(ord),
            *docs, boost());

          irs::reset(*score, std::move(scorers));
        }
      }

      // add iterator
      itrs.emplace_back(std::move(docs));

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

}

namespace iresearch {

DEFINE_FACTORY_DEFAULT(by_same_position)

/* static */ const flags& by_same_position::required() {
  static const flags features{ irs::type<frequency>::get(), irs::type<position>::get() };
  return features;
}

filter::prepared::ptr by_same_position::prepare(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost,
    const attribute_provider* /*ctx*/) const {
  auto& terms = options().terms;
  const auto size = terms.size();

  if (0 == size) {
    // empty field or phrase
    return filter::prepared::empty();
  }

  // per segment query state
  same_position_query::states_t query_states(index);

  // per segment terms states
  same_position_query::states_t::state_type term_states;
  term_states.reserve(size);

  //////////////////////////////////////////////////////////////////////////////
  /// !!! FIXME !!!
  /// that's completely wrong, we have to collect stats for each field
  /// instead of aggregating them using a single collector
  //////////////////////////////////////////////////////////////////////////////
  field_collectors field_stats(ord);

  // prepare phrase stats (collector for each term)
  term_collectors term_stats(ord, size);

  for (const auto& segment : index) {
    size_t term_idx = 0;

    for (const auto& branch : terms) {
      auto next_stats = irs::make_finally([&term_idx](){ ++term_idx; });

      // get term dictionary for field
      const term_reader* field = segment.field(branch.first);
      if (!field) {
        continue;
      }

      // check required features
      if (!required().is_subset_of(field->meta().features)) {
        continue;
      }

      field_stats.collect(segment, *field); // collect field statistics once per segment

      // find terms
      seek_term_iterator::ptr term = field->iterator();

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
      term_stats.collect(segment, *field, term_idx, *term);
      term_states.emplace_back();

      auto& state = term_states.back();

      state.cookie = term->cookie();
      state.reader = field;
    }

    if (term_states.size() != terms.size()) {
      // we have not found all needed terms
      term_states.clear();
      continue;
    }

    auto& state = query_states.insert(segment);
    state = std::move(term_states);

    term_states.reserve(terms.size());
  }

  // finish stats
  size_t term_idx = 0;
  same_position_query::stats_t stats(size);
  for (auto& stat : stats) {
    stat.resize(ord.stats_size());
    auto* stats_buf = const_cast<byte_type*>(stat.data());
    term_stats.finish(stats_buf, term_idx++, field_stats, index);
  }

  return memory::make_managed<same_position_query>(
    std::move(query_states),
    std::move(stats),
    this->boost() * boost);
}

} // ROOT
