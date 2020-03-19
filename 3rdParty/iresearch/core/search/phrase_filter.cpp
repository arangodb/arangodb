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

#include "utils/automaton.hpp" // FOR FST_NO_DYNAMIC_LINKING

#include "phrase_filter.hpp"

#include <boost/functional/hash.hpp>

#include "shared.hpp"
#include "cost.hpp"
#include "term_query.hpp"
#include "conjunction.hpp"
#include "disjunction.hpp"
#include "levenshtein_filter.hpp"
#include "prefix_filter.hpp"
#include "wildcard_filter.hpp"

#if defined(_MSC_VER)
  #pragma warning( disable : 4706 )
#elif defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wparentheses"
#endif

#include "phrase_iterator.hpp"

#if defined(_MSC_VER)
  #pragma warning( default : 4706 )
#elif defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

#include "analysis/token_attributes.hpp"

#include "index/index_reader.hpp"
#include "index/field_meta.hpp"
#include "utils/fst_table_matcher.hpp"
#include "utils/levenshtein_utils.hpp"
#include "utils/misc.hpp"
#include "utils/utf8_utils.hpp"
#include "utils/wildcard_utils.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class phrase_state 
/// @brief cached per reader phrase state
//////////////////////////////////////////////////////////////////////////////
template<template<typename...> class T>
struct phrase_state {
  typedef seek_term_iterator::cookie_ptr term_state_t;
  typedef T<term_state_t> terms_states_t;

  phrase_state() = default;

  phrase_state(phrase_state&& rhs) noexcept
    : terms(std::move(rhs.terms)),
      reader(rhs.reader) {
    rhs.reader = nullptr;
  }

  phrase_state& operator=(const phrase_state&) = delete;

  terms_states_t terms;
  const term_reader* reader{};
}; // phrase_state

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

class fixed_phrase_query : public phrase_query<order::prepared::FixedContainer> {
 public:

  DECLARE_SHARED_PTR(fixed_phrase_query);

  fixed_phrase_query(
      typename phrase_query<order::prepared::FixedContainer>::states_t&& states,
      typename phrase_query<order::prepared::FixedContainer>::positions_t&& positions,
      bstring&& stats,
      boost_t boost) noexcept
    : phrase_query<order::prepared::FixedContainer>(std::move(states), std::move(positions), std::move(stats), boost) {
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

class variadic_phrase_query : public phrase_query<order::prepared::VariadicContainer> {
 public:

  DECLARE_SHARED_PTR(variadic_phrase_query);

  variadic_phrase_query(
      states_t&& states,
      positions_t&& positions,
      bstring&& stats,
      boost_t boost) noexcept
    : phrase_query<order::prepared::VariadicContainer>(std::move(states), std::move(positions), std::move(stats), boost) {
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
  type = other.type;
  allocate();
  switch (type) {
    case PhrasePartType::TERM:
      this->st = other.st;
      break;
    case PhrasePartType::PREFIX:
      this->pt = other.pt;
      break;
    case PhrasePartType::WILDCARD:
      this->wt = other.wt;
      break;
    case PhrasePartType::LEVENSHTEIN:
      this->lt = other.lt;
      break;
    case PhrasePartType::SET:
      this->ct = other.ct;
      break;
    default:
      assert(false);
  }
}

by_phrase::phrase_part::phrase_part(phrase_part&& other) noexcept {
  type = other.type;
  allocate();
  switch (type) {
    case PhrasePartType::TERM:
      this->st = std::move(other.st);
      break;
    case PhrasePartType::PREFIX:
      this->pt = std::move(other.pt);
      break;
    case PhrasePartType::WILDCARD:
      this->wt = std::move(other.wt);
      break;
    case PhrasePartType::LEVENSHTEIN:
      this->lt = std::move(other.lt);
      break;
    case PhrasePartType::SET:
      this->ct = std::move(other.ct);
      break;
    default:
      assert(false);
  }
}

by_phrase::phrase_part::phrase_part(const simple_term& st) {
  type = PhrasePartType::TERM;
  allocate();
  this->st = st;
}

by_phrase::phrase_part::phrase_part(simple_term&& st) noexcept {
  type = PhrasePartType::TERM;
  allocate();
  this->st = std::move(st);
}

by_phrase::phrase_part::phrase_part(const prefix_term& pt) {
  type = PhrasePartType::PREFIX;
  allocate();
  this->pt = pt;
}

by_phrase::phrase_part::phrase_part(prefix_term&& pt) noexcept {
  type = PhrasePartType::PREFIX;
  allocate();
  this->pt = std::move(pt);
}

by_phrase::phrase_part::phrase_part(const wildcard_term& wt) {
  type = PhrasePartType::WILDCARD;
  allocate();
  this->wt = wt;
}

by_phrase::phrase_part::phrase_part(wildcard_term&& wt) noexcept {
  type = PhrasePartType::WILDCARD;
  allocate();
  this->wt = std::move(wt);
}

by_phrase::phrase_part::phrase_part(const levenshtein_term& lt) {
  type = PhrasePartType::LEVENSHTEIN;
  allocate();
  this->lt = lt;
}

by_phrase::phrase_part::phrase_part(levenshtein_term&& lt) noexcept {
  type = PhrasePartType::LEVENSHTEIN;
  allocate();
  this->lt = std::move(lt);
}

by_phrase::phrase_part::phrase_part(const set_term& ct) {
  type = PhrasePartType::SET;
  allocate();
  this->ct = ct;
}

by_phrase::phrase_part::phrase_part(set_term&& ct) noexcept {
  type = PhrasePartType::SET;
  allocate();
  this->ct = std::move(ct);
}

by_phrase::phrase_part& by_phrase::phrase_part::operator=(const phrase_part& other) noexcept {
  if (&other == this) {
    return *this;
  }
  recreate(other.type);
  switch (type) {
    case PhrasePartType::TERM:
      st = other.st;
      break;
    case PhrasePartType::PREFIX:
      pt = other.pt;
      break;
    case PhrasePartType::WILDCARD:
      wt = other.wt;
      break;
    case PhrasePartType::LEVENSHTEIN:
      lt = other.lt;
      break;
    case PhrasePartType::SET:
      ct = other.ct;
      break;
    default:
      assert(false);
  }
  return *this;
}

by_phrase::phrase_part& by_phrase::phrase_part::operator=(phrase_part&& other) noexcept {
  if (&other == this) {
    return *this;
  }
  recreate(other.type);
  switch (type) {
    case PhrasePartType::TERM:
      st = std::move(other.st);
      break;
    case PhrasePartType::PREFIX:
      pt = std::move(other.pt);
      break;
    case PhrasePartType::WILDCARD:
      wt = std::move(other.wt);
      break;
    case PhrasePartType::LEVENSHTEIN:
      lt = std::move(other.lt);
      break;
    case PhrasePartType::SET:
      ct = std::move(other.ct);
      break;
    default:
      assert(false);
  }
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
    default:
      assert(false);
  }
  return false;
}

void by_phrase::phrase_part::allocate() noexcept {
  switch (type) {
    case PhrasePartType::TERM:
      new (&st) simple_term();
      break;
    case PhrasePartType::PREFIX:
      new (&pt) prefix_term();
      break;
    case PhrasePartType::WILDCARD:
      new (&wt) wildcard_term();
      break;
    case PhrasePartType::LEVENSHTEIN:
      new (&lt) levenshtein_term();
      break;
    case PhrasePartType::SET:
      new (&ct) set_term();
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
    default:
      assert(false);
  }
}

void by_phrase::phrase_part::recreate(PhrasePartType new_type) noexcept {
  if (type != new_type) {
    destroy();
    type = new_type;
    allocate();
  }
}

size_t hash_value(const by_phrase::phrase_part& info) {
  auto seed = std::hash<int>()(static_cast<int>(info.type));
  switch (info.type) {
    case by_phrase::PhrasePartType::TERM:
      ::boost::hash_combine(seed, std::hash<bstring>()(info.st.term));
      break;
    case by_phrase::PhrasePartType::PREFIX:
      ::boost::hash_combine(seed, std::hash<size_t>()(info.pt.scored_terms_limit));
      ::boost::hash_combine(seed, std::hash<bstring>()(info.pt.term));
      break;
    case by_phrase::PhrasePartType::WILDCARD:
      ::boost::hash_combine(seed, std::hash<size_t>()(info.wt.scored_terms_limit));
      ::boost::hash_combine(seed, std::hash<bstring>()(info.wt.term));
      break;
    case by_phrase::PhrasePartType::LEVENSHTEIN:
      ::boost::hash_combine(seed, std::hash<bool>()(info.lt.with_transpositions));
      ::boost::hash_combine(seed, std::hash<byte_type>()(info.lt.max_distance));
      ::boost::hash_combine(seed, std::hash<size_t>()(info.lt.scored_terms_limit));
      ::boost::hash_combine(seed, std::hash<by_edit_distance::pdp_f>()(info.lt.provider));
      ::boost::hash_combine(seed, std::hash<bstring>()(info.lt.term));
      break;
    case by_phrase::PhrasePartType::SET:
      std::for_each(
        info.ct.terms.cbegin(), info.ct.terms.cend(),
        [&seed](const bstring& term) {
          ::boost::hash_combine(seed, term);
      });
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
    phrase_.begin(), phrase_.end(),
    [&seed](const by_phrase::term_t& term) {
      ::boost::hash_combine(seed, term);
  });
  return seed;
}

filter::prepared::ptr by_phrase::prepare(
    const index_reader& rdr,
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
    switch (term_info.type) {
      case PhrasePartType::TERM: // similar to `term_query`
        return term_query::make(rdr, ord, boost*this->boost(), fld_, term_info.st.term);
      case PhrasePartType::PREFIX:
        return by_prefix::prepare(rdr, ord, boost*this->boost(), fld_, term_info.pt.term,
                                  term_info.pt.scored_terms_limit);
      case PhrasePartType::WILDCARD:
        return by_wildcard::prepare(rdr, ord, boost*this->boost(), fld_,
                                    term_info.wt.term, term_info.wt.scored_terms_limit);
      case PhrasePartType::LEVENSHTEIN:
        return by_edit_distance::prepare(rdr, ord, boost*this->boost(), fld_,
                                         term_info.lt.term, term_info.lt.scored_terms_limit,
                                         term_info.lt.max_distance, term_info.lt.provider,
                                         term_info.lt.with_transpositions);
      case PhrasePartType::SET:
        // do nothing
        break;
      default:
        assert(false);
    }
  }

  // prepare phrase stats (collector for each term)
  if (is_simple_term_only_) {
    return fixed_prepare_collect(rdr, ord, boost, ord.fixed_prepare_collectors(phrase_size));
  }
  return variadic_prepare_collect(rdr, ord, boost, ord.variadic_prepare_collectors(phrase_size));
}

filter::prepared::ptr by_phrase::fixed_prepare_collect(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    order::prepared::fixed_terms_collectors collectors) const {
  // per segment phrase states
  fixed_phrase_query::states_t phrase_states(rdr.size());

  // per segment phrase terms
  phrase_state<order::prepared::FixedContainer>::terms_states_t phrase_terms;
  auto phrase_size = phrase_.size();
  phrase_terms.reserve(phrase_size);

  // iterate over the segments
  const string_ref field = fld_;

  for (const auto& sr : rdr) {
    // get term dictionary for field
    const term_reader* tr = sr.field(field);

    if (!tr) {
      continue;
    }

    // check required features
    if (!by_phrase::required().is_subset_of(tr->meta().features)) {
      continue;
    }

    collectors.collect(sr, *tr); // collect field statistics once per segment

    // find terms
    seek_term_iterator::ptr term = tr->iterator();
    size_t term_itr = 0;

    for (const auto& word : phrase_) {
      assert(PhrasePartType::TERM == word.second.type);
      auto next_stats = irs::make_finally([&term_itr]()->void{ ++term_itr; });
      if (!term->seek(word.second.st.term)) {
        if (ord.empty()) {
          break;
        } else {
          // continue here because we should collect
          // stats for other terms in phrase
          continue;
        }
      }

      term->read(); // read term attributes
      collectors.collect(sr, *tr, term_itr, term->attributes()); // collect statistics

      // estimate phrase & term
      phrase_terms.emplace_back(term->cookie());
    }

    // we have not found all needed terms
    if (phrase_terms.size() != phrase_size) {
      phrase_terms.clear();
      continue;
    }

    auto& state = phrase_states.insert(sr);
    state.terms = std::move(phrase_terms);
    state.reader = tr;

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
  collectors.finish(stats_buf, rdr);

  return memory::make_shared<fixed_phrase_query>(
    std::move(phrase_states),
    std::move(positions),
    std::move(stats),
    this->boost() * boost
  );
}

irs::bytes_ref unescape(const irs::bytes_ref& in, irs::bstring& out);

filter::prepared::ptr by_phrase::variadic_prepare_collect(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    order::prepared::variadic_terms_collectors collectors) const {
  // per segment phrase states
  variadic_phrase_query::states_t phrase_states(rdr.size());

  // per segment phrase terms
  phrase_state<order::prepared::VariadicContainer>::terms_states_t phrase_terms;
  auto phrase_size = phrase_.size();
  phrase_terms.resize(phrase_size);

  // iterate over the segments
  const string_ref field = fld_;

  bstring buf;
  for (const auto& sr : rdr) {
    // get term dictionary for field
    const term_reader* tr = sr.field(field);

    if (!tr) {
      continue;
    }

    // check required features
    if (!by_phrase::required().is_subset_of(tr->meta().features)) {
      continue;
    }

    collectors.collect(sr, *tr); // collect field statistics once per segment

    // find terms
    seek_term_iterator::ptr term = tr->iterator();
    size_t term_itr = 0;
    size_t found_words_count = 0;

    size_t i = 0;
    for (const auto& word : phrase_) {
      auto next_stats = irs::make_finally([&term_itr]()->void{ ++term_itr; });
      auto& pt = phrase_terms[i++];
      auto type = word.second.type;
      bytes_ref pattern;
      auto stop = false;
      switch (word.second.type) {
        case PhrasePartType::TERM:
          pattern = word.second.st.term;
          break;
        case PhrasePartType::PREFIX:
          pattern = word.second.pt.term;
          break;
        case PhrasePartType::WILDCARD:
          pattern = word.second.wt.term;
          switch (irs::wildcard_type(pattern)) {
            case WildcardType::INVALID:
              if (ord.empty()) {
                stop = true;
                // else we should collect
                // stats for other terms in phrase
              }
              break;
            case WildcardType::TERM_ESCAPED:
              pattern = unescape(pattern, buf);
              #if IRESEARCH_CXX > IRESEARCH_CXX_14
                [[fallthrough]];
              #endif
            case WildcardType::TERM:
              type = PhrasePartType::TERM;
              break;
            case WildcardType::MATCH_ALL:
              pattern = bytes_ref::EMPTY; // empty prefix == match all
              type = PhrasePartType::PREFIX;
              break;
            case WildcardType::PREFIX_ESCAPED:
              pattern = unescape(pattern, buf);
              #if IRESEARCH_CXX > IRESEARCH_CXX_14
                [[fallthrough]];
              #endif
            case WildcardType::PREFIX: {
              assert(!pattern.empty());
              const auto* begin = pattern.c_str();
              const auto* end = begin + pattern.size();

              // pattern is already checked to be a valid UTF-8 sequence
              const auto* pos = utf8_utils::find<false>(begin, end, WildcardMatch::ANY_STRING);
              assert(pos != end);
              pattern = bytes_ref(begin, size_t(pos - begin)); // remove trailing '%'
              type = PhrasePartType::PREFIX;
              break;
            }
            case WildcardType::WILDCARD:
              // do nothing
              break;
            default:
              assert(false);
          }
          break;
        case PhrasePartType::LEVENSHTEIN:
          if (0 == word.second.lt.max_distance) {
            type = PhrasePartType::TERM;
          }
          break;
        case PhrasePartType::SET:
          if (word.second.ct.terms.size() == 1) {
            pattern = word.second.ct.terms.front();
            type = PhrasePartType::TERM;
          }
          break;
        default:
          assert(false);
      }
      if (stop) {
        break;
      }

      switch (type) {
        case PhrasePartType::TERM: {
          if (!term->seek(pattern)) {
            if (ord.empty()) {
              stop = true;
              // else we should collect
              // stats for other terms in phrase
            }
            break;
          }

          term->read(); // read term attributes
          collectors.collect(sr, *tr, term_itr, term->attributes()); // collect statistics

          // estimate phrase & term
          pt.emplace_back(term->cookie());
          ++found_words_count;
          break;
        }
        case PhrasePartType::PREFIX: {
          // seek to prefix
          if (SeekResult::END == term->seek_ge(pattern)) {
            if (ord.empty()) {
              stop = true;
              // else we should collect
              // stats for other terms in phrase
            }
            break;
          }

          const auto& value = term->value();

          #ifdef IRESEARCH_DEBUG
            IRESEARCH_IGNORE_UNUSED auto found = false;
          #endif
          while (starts_with(value, pattern)) {
            #ifdef IRESEARCH_DEBUG
              found = true;
            #endif
            term->read();
            collectors.collect(sr, *tr, term_itr, term->attributes()); // collect statistics

            // estimate phrase & term
            pt.emplace_back(term->cookie());
            if (!term->next()) {
              break;
            }
          }
          #ifdef IRESEARCH_DEBUG
            assert(found);
          #endif
          ++found_words_count;
          break;
        }
        case PhrasePartType::WILDCARD: {
          const auto& acceptor = from_wildcard(pattern);
          automaton_table_matcher matcher(acceptor, fst::fsa::kRho);

          if (fst::kError == matcher.Properties(0)) {
            IR_FRMT_ERROR("Expected deterministic, epsilon-free acceptor, "
                          "got the following properties " IR_UINT64_T_SPECIFIER "",
                          acceptor.Properties(automaton_table_matcher::FST_PROPERTIES, false));
            if (ord.empty()) {
              stop = true;
              // else we should collect
              // stats for other terms in phrase
            }
            break;
          }

          auto term_wildcard = tr->iterator(matcher);

          auto found = false;
          while (term_wildcard->next()) {
            found = true;
            term_wildcard->read();
            collectors.collect(sr, *tr, term_itr, term_wildcard->attributes()); // collect statistics

            // estimate phrase & term
            pt.emplace_back(term_wildcard->cookie());
          }
          if (found) {
            ++found_words_count;
          } else if (ord.empty()) {
            stop = true;
            // else we should collect
            // stats for other terms in phrase
          }
          break;
        }
        case PhrasePartType::LEVENSHTEIN: {
          assert(word.second.lt.provider);
          const auto& d = (*word.second.lt.provider)(word.second.lt.max_distance,
                                                     word.second.lt.with_transpositions);
          if (!d) {
            if (ord.empty()) {
              stop = true;
              // else we should collect
              // stats for other terms in phrase
            }
            break;
          }
          const auto& acceptor = irs::make_levenshtein_automaton(d, word.second.lt.term);
          automaton_table_matcher matcher(acceptor, fst::fsa::kRho);

          if (fst::kError == matcher.Properties(0)) {
            IR_FRMT_ERROR("Expected deterministic, epsilon-free acceptor, "
                          "got the following properties " IR_UINT64_T_SPECIFIER "",
                          acceptor.Properties(automaton_table_matcher::FST_PROPERTIES, false));
            if (ord.empty()) {
              stop = true;
              // else we should collect
              // stats for other terms in phrase
            }
            break;
          }
          auto term_levenshtein = tr->iterator(matcher);

          auto found = false;
          while (term_levenshtein->next()) {
            found = true;
            term_levenshtein->read();
            collectors.collect(sr, *tr, term_itr, term_levenshtein->attributes()); // collect statistics

            // estimate phrase & term
            pt.emplace_back(term_levenshtein->cookie());
          }
          if (found) {
            ++found_words_count;
          } else if (ord.empty()) {
            stop = true;
            // else we should collect
            // stats for other terms in phrase
          }
          break;
        }
        case PhrasePartType::SET: {
          auto found = false;
          for (const auto& pat : word.second.ct.terms) {
            if (!term->seek(pat)) {
              continue;
            }
            found = true;

            term->read(); // read term attributes
            collectors.collect(sr, *tr, term_itr, term->attributes()); // collect statistics

            // estimate phrase & term
            pt.emplace_back(term->cookie());
          }
          if (found) {
            ++found_words_count;
          } else if (ord.empty()) {
            stop = true;
            // else we should collect
            // stats for other terms in phrase
          }
          break;
        }
        default:
          assert(false);
      }
      if (stop) {
        break;
      }
    }

    // we have not found all needed terms
    if (found_words_count != phrase_size) {
      for (auto& pt : phrase_terms) {
        pt.clear();
      }
      continue;
    }

    auto& state = phrase_states.insert(sr);
    state.terms = std::move(phrase_terms);
    state.reader = tr;

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
  collectors.finish(stats_buf, rdr);

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
