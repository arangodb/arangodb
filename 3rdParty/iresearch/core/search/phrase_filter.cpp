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

#include <boost/functional/hash.hpp>

#include "phrase_filter.hpp"
#include "phrase_iterator.hpp"
#include "term_query.hpp"
#include "wildcard_filter.hpp"
#include "index/field_meta.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class phrase_query
/// @brief prepared phrase query implementation
//////////////////////////////////////////////////////////////////////////////
template<template<typename...> class T>
class phrase_query : public filter::prepared {
 public:
  typedef states_cache<phrase_state<T>> states_t;
  typedef std::vector<position::value_t> positions_t;

  DECLARE_SHARED_PTR(phrase_query<T>);

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

class fixed_phrase_query : public phrase_query<FixedContainer> {
 public:

  DECLARE_SHARED_PTR(fixed_phrase_query);

  fixed_phrase_query(
      typename phrase_query<FixedContainer>::states_t&& states,
      typename phrase_query<FixedContainer>::positions_t&& positions,
      bstring&& stats,
      boost_t boost) noexcept
    : phrase_query<FixedContainer>(std::move(states), std::move(positions), std::move(stats), boost) {
  }

  using filter::prepared::execute;

  doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord,
      const attribute_view& /*ctx*/) const {
    // get phrase state for the specified reader
    auto phrase_state = states_.find(rdr);

    if (!phrase_state) {
      // invalid state
      return doc_iterator::empty();
    }

    // get features required for query & order
    auto features = ord.features() | by_phrase::required();

    typedef conjunction<doc_iterator::ptr> conjunction_t;
    typedef phrase_iterator<conjunction_t, fixed_phrase_frequency> phrase_iterator_t;

    conjunction_t::doc_iterators_t itrs;
    itrs.reserve(phrase_state->terms.size());

    phrase_iterator_t::positions_t positions;
    positions.reserve(phrase_state->terms.size());

    // find term using cached state
    auto terms = phrase_state->reader->iterator();
    auto position = positions_.begin();

    for (const auto& term_state : phrase_state->terms) {
      // use bytes_ref::blank here since we do not need just to "jump"
      // to cached state, and we are not interested in term value itself
      if (!terms->seek(bytes_ref::NIL, *term_state)) {
        return doc_iterator::empty();
      }

      auto docs = terms->postings(features); // postings
      auto& pos = docs->attributes().get<irs::position>(); // needed postings attributes

      if (!pos) {
        // positions not found
        return doc_iterator::empty();
      }

      positions.emplace_back(std::ref(*pos), *position);

      // add base iterator
      itrs.emplace_back(std::move(docs));

      ++position;
    }

    return memory::make_shared<phrase_iterator_t>(
      std::move(itrs),
      std::move(positions),
      rdr,
      *phrase_state->reader,
      stats_.c_str(),
      ord,
      boost()
    );
  }
}; // fixed_phrase_query

class variadic_phrase_query : public phrase_query<VariadicContainer> {
 public:

  DECLARE_SHARED_PTR(variadic_phrase_query);

  variadic_phrase_query(
      states_t&& states,
      positions_t&& positions,
      bstring&& stats,
      boost_t boost) noexcept
    : phrase_query<VariadicContainer>(std::move(states), std::move(positions), std::move(stats), boost) {
  }

  using filter::prepared::execute;

  doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord,
      const attribute_view& /*ctx*/) const override {
    // get phrase state for the specified reader
    auto phrase_state = states_.find(rdr);

    if (!phrase_state) {
      // invalid state
      return doc_iterator::empty();
    }

    // get features required for query & order
    auto features = ord.features() | by_phrase::required();

    typedef conjunction<doc_iterator::ptr> conjunction_t;
    typedef phrase_iterator<conjunction_t, variadic_phrase_frequency> phrase_iterator_t;

    conjunction_t::doc_iterators_t conj_itrs;
    conj_itrs.reserve(phrase_state->terms.size());

    typedef position_score_iterator_adapter<doc_iterator::ptr> adapter_t;
    typedef disjunction<doc_iterator::ptr, adapter_t, true> disjunction_t;

    phrase_iterator_t::positions_t positions;
    positions.resize(phrase_state->terms.size());

    // find term using cached state
    auto terms = phrase_state->reader->iterator();
    auto position = positions_.begin();

    size_t i = 0;
    for (const auto& term_states : phrase_state->terms) {
      auto is_found = false;
      auto& ps = positions[i++];
      ps.second = *position;

      disjunction_t::doc_iterators_t disj_itrs;
      disj_itrs.reserve(term_states.size());
      for (const auto& term_state : term_states) {
        // use bytes_ref::blank here since we do not need just to "jump"
        // to cached state, and we are not interested in term value itself
        if (!terms->seek(bytes_ref::NIL, *term_state)) {
          continue;
        }

        auto docs = terms->postings(features); // postings
        auto& pos = docs->attributes().get<irs::position>(); // needed postings attributes

        if (!pos) {
          // positions not found
          continue;
        }

        // add base iterator
        disj_itrs.emplace_back(std::move(docs));

        is_found = true;
      }
      if (!is_found) {
        return doc_iterator::empty();
      }
      auto disj = make_disjunction<disjunction_t>(std::move(disj_itrs));
      typedef irs::compound_doc_iterator<adapter_t> compound_doc_iterator_t;
      #ifdef IRESEARCH_DEBUG
        ps.first = dynamic_cast<compound_doc_iterator_t*>(disj.get());
        assert(ps.first);
      #else
        ps.first = static_cast<compound_doc_iterator_t*>(disj.get());
      #endif
      conj_itrs.emplace_back(std::move(disj));
      ++position;
    }

    return memory::make_shared<phrase_iterator_t>(
      std::move(conj_itrs),
      std::move(positions),
      rdr,
      *phrase_state->reader,
      stats_.c_str(),
      ord,
      boost()
    );
  }
}; // variadic_phrase_query

// -----------------------------------------------------------------------------
// --SECTION--                                          by_phrase implementation
// -----------------------------------------------------------------------------

/* static */ const flags& by_phrase::required() {
  static flags req{ frequency::type(), position::type() };
  return req;
}

DEFINE_FILTER_TYPE(by_phrase)
DEFINE_FACTORY_DEFAULT(by_phrase)

by_phrase::by_phrase(): filter(by_phrase::type()) {
}

bool by_phrase::equals(const filter& rhs) const noexcept {
  const by_phrase& trhs = static_cast<const by_phrase&>(rhs);
  return filter::equals(rhs) && fld_ == trhs.fld_ && phrase_ == trhs.phrase_;
}

by_phrase::phrase_part::phrase_part() : type(PhrasePartType::TERM), st() {
}

by_phrase::phrase_part::phrase_part(const phrase_part& other) {
  allocate(other);
}

by_phrase::phrase_part::phrase_part(phrase_part&& other) noexcept {
  allocate(std::move(other));
}

by_phrase::phrase_part& by_phrase::phrase_part::operator=(const phrase_part& other) {
  if (&other == this) {
    return *this;
  }
  recreate(other);
  return *this;
}

by_phrase::phrase_part& by_phrase::phrase_part::operator=(phrase_part&& other) noexcept {
  if (&other == this) {
    return *this;
  }
  recreate(std::move(other));
  return *this;
}

bool by_phrase::phrase_part::operator==(const phrase_part& other) const noexcept {
  if (type != other.type) {
    return false;
  }
  switch (type) {
    case PhrasePartType::TERM:
      return st == other.st;
    case PhrasePartType::PREFIX:
      return pt == other.pt;
    case PhrasePartType::WILDCARD:
      return wt == other.wt;
    case PhrasePartType::LEVENSHTEIN:
      return lt == other.lt;
    case PhrasePartType::SET:
      return ct == other.ct;
    case PhrasePartType::RANGE:
      return rt == other.rt;
    default:
      assert(false);
  }
  return false;
}

/*static*/ bool by_phrase::phrase_part::variadic_type_collect(
    const term_reader& reader,
    const phrase_part& phr_part,
    phrase_term_visitor<variadic_terms_collectors>& ptv) {
  auto found = false;
  switch (phr_part.type) {
    case PhrasePartType::TERM:
      term_query::visit(reader, phr_part.st.term, ptv);
      found = ptv.found();
      break;
    case PhrasePartType::PREFIX:
      by_prefix::visit(reader, phr_part.pt.term, ptv);
      found = ptv.found();
      break;
    case PhrasePartType::WILDCARD:
      by_wildcard::visit(reader, phr_part.wt.term, ptv);
      found = ptv.found();
      break;
    case PhrasePartType::LEVENSHTEIN:
      by_edit_distance::visit(
        reader, phr_part.lt.term, phr_part.lt.max_distance, phr_part.lt.provider,
        phr_part.lt.with_transpositions, ptv);
      found = ptv.found();
      break;
    case PhrasePartType::SET:
      for (const auto& term : phr_part.ct.terms) {
        term_query::visit(reader, term, ptv);
        found |= ptv.found();
        ptv.reset();
      }
      break;
    case PhrasePartType::RANGE:
      by_range::visit(reader, phr_part.rt.rng, ptv);
      found = ptv.found();
      break;
    default:
      assert(false);
  }
  return found;
}

void by_phrase::phrase_part::allocate(const phrase_part& other) {
  type = other.type;
  switch (type) {
    case PhrasePartType::TERM:
      new (&st) simple_term(other.st);
      break;
    case PhrasePartType::PREFIX:
      new (&pt) prefix_term(other.pt);
      break;
    case PhrasePartType::WILDCARD:
      new (&wt) wildcard_term(other.wt);
      break;
    case PhrasePartType::LEVENSHTEIN:
      new (&lt) levenshtein_term(other.lt);
      break;
    case PhrasePartType::SET:
      new (&ct) set_term(other.ct);
      break;
    case PhrasePartType::RANGE:
      new (&rt) range_term(other.rt);
      break;
    default:
      assert(false);
  }
}

void by_phrase::phrase_part::allocate(phrase_part&& other) noexcept {
  type = other.type;
  switch (type) {
    case PhrasePartType::TERM:
      new (&st) simple_term(std::move(other.st));
      break;
    case PhrasePartType::PREFIX:
      new (&pt) prefix_term(std::move(other.pt));
      break;
    case PhrasePartType::WILDCARD:
      new (&wt) wildcard_term(std::move(other.wt));
      break;
    case PhrasePartType::LEVENSHTEIN:
      new (&lt) levenshtein_term(std::move(other.lt));
      break;
    case PhrasePartType::SET:
      new (&ct) set_term(std::move(other.ct));
      break;
    case PhrasePartType::RANGE:
      new (&rt) range_term(std::move(other.rt));
      break;
    default:
      assert(false);
  }
}

void by_phrase::phrase_part::destroy() noexcept {
  switch (type) {
    case PhrasePartType::TERM:
      st.~simple_term();
      break;
    case PhrasePartType::PREFIX:
      pt.~prefix_term();
      break;
    case PhrasePartType::WILDCARD:
      wt.~wildcard_term();
      break;
    case PhrasePartType::LEVENSHTEIN:
      lt.~levenshtein_term();
      break;
    case PhrasePartType::SET:
      ct.~set_term();
      break;
    case PhrasePartType::RANGE:
      rt.~range_term();
      break;
    default:
      assert(false);
  }
}

void by_phrase::phrase_part::recreate(const phrase_part& other) {
  if (type != other.type) {
    destroy();
  }
  allocate(other);
}

void by_phrase::phrase_part::recreate(phrase_part&& other) noexcept {
  if (type != other.type) {
    destroy();
  }
  allocate(std::move(other));
}

size_t hash_value(const by_range::range_t& rng);

size_t hash_value(const by_phrase::phrase_part& info) {
  auto seed = std::hash<int>()(static_cast<std::underlying_type<by_phrase::PhrasePartType>::type>(info.type));
  switch (info.type) {
    case by_phrase::PhrasePartType::TERM:
      ::boost::hash_combine(seed, info.st.term);
      break;
    case by_phrase::PhrasePartType::PREFIX:
      ::boost::hash_combine(seed, info.pt.scored_terms_limit);
      ::boost::hash_combine(seed, info.pt.term);
      break;
    case by_phrase::PhrasePartType::WILDCARD:
      ::boost::hash_combine(seed, info.wt.scored_terms_limit);
      ::boost::hash_combine(seed, info.wt.term);
      break;
    case by_phrase::PhrasePartType::LEVENSHTEIN:
      ::boost::hash_combine(seed, info.lt.with_transpositions);
      ::boost::hash_combine(seed, info.lt.max_distance);
      ::boost::hash_combine(seed, info.lt.scored_terms_limit);
      ::boost::hash_combine(seed, info.lt.provider);
      ::boost::hash_combine(seed, info.lt.term);
      break;
    case by_phrase::PhrasePartType::SET:
      std::for_each(
        info.ct.terms.cbegin(), info.ct.terms.cend(),
        [&seed](const bstring& term) {
          ::boost::hash_combine(seed, term);
      });
      break;
    case by_phrase::PhrasePartType::RANGE:
      ::boost::hash_combine(seed, info.rt.scored_terms_limit);
      ::boost::hash_combine(seed, info.rt.rng);
      break;
    default:
      assert(false);
  }
  return seed;
}

size_t by_phrase::hash() const noexcept {
  size_t seed = 0;
  ::boost::hash_combine(seed, filter::hash());
  ::boost::hash_combine(seed, fld_);
  std::for_each(
    phrase_.cbegin(), phrase_.cend(),
    [&seed](const by_phrase::term_t& term) {
      ::boost::hash_combine(seed, term);
  });
  return seed;
}

filter::prepared::ptr by_phrase::prepare(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost,
    const attribute_view& /*ctx*/) const {
  if (fld_.empty() || phrase_.empty()) {
    // empty field or phrase
    return filter::prepared::empty();
  }

  const auto phrase_size = phrase_.size();
  if (1 == phrase_size) {
    const auto& term_info = phrase_.begin()->second;
    boost *= this->boost();
    switch (term_info.type) {
      case PhrasePartType::TERM: // similar to `term_query`
        return term_query::make(index, ord, boost, fld_, term_info.st.term);
      case PhrasePartType::PREFIX:
        return by_prefix::prepare(index, ord, boost, fld_, term_info.pt.term,
                                  term_info.pt.scored_terms_limit);
      case PhrasePartType::WILDCARD:
        return by_wildcard::prepare(index, ord, boost, fld_, term_info.wt.term,
                                    term_info.wt.scored_terms_limit);
      case PhrasePartType::LEVENSHTEIN:
        return by_edit_distance::prepare(index, ord, boost, fld_, term_info.lt.term,
                                         term_info.lt.scored_terms_limit,
                                         term_info.lt.max_distance, term_info.lt.provider,
                                         term_info.lt.with_transpositions);
      case PhrasePartType::SET:
        // do nothing
        break;
      case PhrasePartType::RANGE:
        return by_range::prepare(index, ord, boost, fld_, term_info.rt.rng,
                                 term_info.rt.scored_terms_limit);
      default:
        assert(false);
        return filter::prepared::empty();
    }
  }

  // prepare phrase stats (collector for each term)
  if (is_simple_term_only_) {
    return fixed_prepare_collect(index, ord, boost, fixed_terms_collectors(ord, phrase_size));
  }
  return variadic_prepare_collect(index, ord, boost, variadic_terms_collectors(ord, phrase_size));
}

filter::prepared::ptr by_phrase::fixed_prepare_collect(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost,
    fixed_terms_collectors collectors) const {
  // per segment phrase states
  fixed_phrase_query::states_t phrase_states(index.size());

  // per segment phrase terms
  phrase_state<FixedContainer>::terms_states_t phrase_terms;
  auto phrase_size = phrase_.size();
  phrase_terms.reserve(phrase_size);

  // iterate over the segments
  const string_ref field = fld_;

  for (const auto& segment : index) {
    // get term dictionary for field
    const term_reader* reader = segment.field(field);

    if (!reader) {
      continue;
    }

    // check required features
    if (!by_phrase::required().is_subset_of(reader->meta().features)) {
      continue;
    }

    collectors.collect(segment, *reader); // collect field statistics once per segment

    size_t term_offset = 0;
    auto is_ord_empty = ord.empty();

    phrase_part::phrase_term_visitor<fixed_terms_collectors> ptv(
      segment, *reader, collectors, phrase_terms);

    for (const auto& word : phrase_) {
      assert(PhrasePartType::TERM == word.second.type);
      ptv.reset(term_offset);
      term_query::visit(*reader, word.second.st.term, ptv);
      if (!ptv.found()) {
        if (is_ord_empty) {
          break;
        }
        // continue here because we should collect
        // stats for other terms in phrase
      }
      ++term_offset;
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
  size_t base_offset = first_pos();

  // finish stats
  fixed_phrase_query::positions_t positions(phrase_size);
  auto pos_itr = positions.begin();

  for (const auto& term : phrase_) {
    *pos_itr = position::value_t(term.first - base_offset);
    ++pos_itr;
  }

  bstring stats(ord.stats_size(), 0); // aggregated phrase stats
  auto* stats_buf = const_cast<byte_type*>(stats.data());

  ord.prepare_stats(stats_buf);
  collectors.finish(stats_buf, index);

  return memory::make_shared<fixed_phrase_query>(
    std::move(phrase_states),
    std::move(positions),
    std::move(stats),
    this->boost() * boost
  );
}

filter::prepared::ptr by_phrase::variadic_prepare_collect(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost,
    variadic_terms_collectors collectors) const {
  // per segment phrase states
  variadic_phrase_query::states_t phrase_states(index.size());

  // per segment phrase terms
  phrase_state<VariadicContainer>::terms_states_t phrase_terms;
  auto phrase_size = phrase_.size();
  phrase_terms.resize(phrase_size);

  // iterate over the segments
  const string_ref field = fld_;

  const auto is_ord_empty = ord.empty();

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

    collectors.collect(segment, *reader); // collect field statistics once per segment

    size_t term_offset = 0;
    size_t found_words_count = 0;

    phrase_part::phrase_term_visitor<variadic_terms_collectors> ptv(
      segment, *reader, collectors);

    for (const auto& word : phrase_) {
      ptv.reset(phrase_terms[term_offset], term_offset);
      if (!phrase_part::variadic_type_collect(*reader, word.second, ptv)) {
        if (is_ord_empty) {
          break;
        }
        // continue here because we should collect
        // stats for other terms in phrase
      } else {
        ++found_words_count;
      }
      ++term_offset;
    }

    // we have not found all needed terms
    if (found_words_count != phrase_size) {
      for (auto& pt : phrase_terms) {
        pt.clear();
      }
      continue;
    }

    auto& state = phrase_states.insert(segment);
    state.terms = std::move(phrase_terms);
    state.reader = reader;

    phrase_terms.resize(phrase_size);
  }

  // offset of the first term in a phrase
  size_t base_offset = first_pos();

  // finish stats
  variadic_phrase_query::positions_t positions(phrase_size);
  auto pos_itr = positions.begin();

  for (const auto& term : phrase_) {
    *pos_itr = position::value_t(term.first - base_offset);
    ++pos_itr;
  }

  bstring stats(ord.stats_size(), 0); // aggregated phrase stats
  auto* stats_buf = const_cast<byte_type*>(stats.data());

  ord.prepare_stats(stats_buf);
  collectors.finish(stats_buf, index);

  return memory::make_shared<variadic_phrase_query>(
    std::move(phrase_states),
    std::move(positions),
    std::move(stats),
    this->boost() * boost
  );
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
