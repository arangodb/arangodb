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
#include "phrase_filter.hpp"
#include "cost.hpp"
#include "term_query.hpp"
#include "conjunction.hpp"

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
#include "utils/misc.hpp"

#include <boost/functional/hash.hpp>

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class phrase_state 
/// @brief cached per reader phrase state
//////////////////////////////////////////////////////////////////////////////
struct phrase_state {
  typedef std::pair<seek_term_iterator::cookie_ptr, cost::cost_t> term_state_t;
  typedef std::vector<term_state_t> terms_states_t;

  phrase_state() = default;

  phrase_state(phrase_state&& rhs) NOEXCEPT
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
class phrase_query : public filter::prepared {
 public:
  typedef states_cache<phrase_state> states_t;
  typedef std::pair<
    attribute_store, // term level statistic
    position::value_t // expected term position
  > term_stats_t;
  typedef std::vector<term_stats_t> phrase_stats_t;

  DECLARE_SHARED_PTR(phrase_query);

  phrase_query(states_t&& states, phrase_stats_t&& stats)
    : states_(std::move(states)),
      stats_(std::move(stats)) {
  }

  using filter::prepared::execute;

  virtual doc_iterator::ptr execute(
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

    phrase_iterator::doc_iterators_t itrs;
    itrs.reserve(phrase_state->terms.size());

    phrase_iterator::positions_t positions;
    positions.reserve(phrase_state->terms.size());

    // find term using cached state
    auto terms = phrase_state->reader->iterator();
    auto term_stats = stats_.begin();
    for (auto& term_state : phrase_state->terms) {
      // use bytes_ref::blank here since we do not need just to "jump"
      // to cached state, and we are not interested in term value itself */
      if (!terms->seek(bytes_ref::NIL, *term_state.first)) {
        return doc_iterator::empty();
      }

      // get postings
      auto docs = terms->postings(features);

      // get needed postings attributes
      auto& pos = docs->attributes().get<position>();
      if (!pos) {
        // positions not found
        return doc_iterator::empty();
      }
      positions.emplace_back(std::ref(*pos), term_stats->second);

      // add base iterator
      itrs.emplace_back(doc_iterator::make<basic_doc_iterator>(
        rdr,
        *phrase_state->reader,
        term_stats->first, 
        std::move(docs), 
        ord, 
        term_state.second
      ));

      ++term_stats;
    }

    return make_conjunction<phrase_iterator>(
      std::move(itrs), ord, std::move(positions)
    );
  }

 private:
  states_t states_;
  phrase_stats_t stats_;
}; // phrase_query

// -----------------------------------------------------------------------------
// --SECTION--                                          by_phrase implementation
// -----------------------------------------------------------------------------

/* static */ const flags& by_phrase::required() {
  static flags req{ frequency::type(), position::type() };
  return req;
}

DEFINE_FILTER_TYPE(by_phrase);
DEFINE_FACTORY_DEFAULT(by_phrase);

by_phrase::by_phrase(): filter(by_phrase::type()) {
}

bool by_phrase::equals(const filter& rhs) const NOEXCEPT {
  const by_phrase& trhs = static_cast<const by_phrase&>(rhs);
  return filter::equals(rhs) && fld_ == trhs.fld_ && phrase_ == trhs.phrase_;
}

size_t by_phrase::hash() const NOEXCEPT {
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

  if (1 == phrase_.size()) {
    // similar to `term_query`
    const irs::bytes_ref term = phrase_.begin()->second;
    return term_query::make(rdr, ord, boost*this->boost(), fld_, term);
  }

  // per segment phrase states 
  phrase_query::states_t phrase_states(rdr.size());

  // per segment phrase terms
  phrase_state::terms_states_t phrase_terms;
  phrase_terms.reserve(phrase_.size());

  // prepare phrase stats (collector for each term)
  std::vector<order::prepared::stats> term_stats;
  term_stats.reserve(phrase_.size());

  for(auto size = phrase_.size(); size; --size) {
    term_stats.emplace_back(ord.prepare_stats());
  }

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

    // find terms
    seek_term_iterator::ptr term = tr->iterator();
    // get term metadata
    auto& meta = term->attributes().get<term_meta>();
    auto term_itr = term_stats.begin();

    for(auto& word: phrase_) {
      auto next_stats = irs::make_finally([&term_itr]()->void{ ++term_itr; });

      if (!term->seek(word.second)) {
        if (ord.empty()) {
          break;
        } else {
          // continue here because we should collect 
          // stats for other terms in phrase
          continue;
        }
      }

      term->read(); // read term attributes
      term_itr->collect(sr, *tr, term->attributes()); // collect statistics

      // estimate phrase & term
      const cost::cost_t term_estimation = meta ? meta->docs_count : cost::MAX;
      phrase_terms.emplace_back(term->cookie(), term_estimation);
    }

    // we have not found all needed terms
    if (phrase_terms.size() != phrase_.size()) {
      phrase_terms.clear();
      continue;
    }

    auto& state = phrase_states.insert(sr);
    state.terms = std::move(phrase_terms);
    state.reader = tr;

    phrase_terms.reserve(phrase_.size());
  }

  // offset of the first term in a phrase
  size_t base_offset = first_pos();

  // finish stats
  phrase_query::phrase_stats_t stats(phrase_.size());
  auto stat_itr = stats.begin();
  auto term_itr = term_stats.begin();
  assert(term_stats.size() == phrase_.size()); // initialized above

  for(auto& term: phrase_) {
    term_itr->finish(stat_itr->first, rdr);
    stat_itr->second = position::value_t(term.first - base_offset);
    ++stat_itr;
    ++term_itr;
  }

  auto q = memory::make_shared<phrase_query>(
    std::move(phrase_states),
    std::move(stats)
  );

  // apply boost
  irs::boost::apply(q->attributes(), this->boost() * boost);

  return IMPLICIT_MOVE_WORKAROUND(q);
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
