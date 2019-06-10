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
  typedef std::vector<position::value_t> positions_t;

  DECLARE_SHARED_PTR(phrase_query);

  phrase_query(
      states_t&& states,
      positions_t&& positions,
      bstring&& stats,
      boost_t boost
  ) NOEXCEPT
    : prepared(std::move(stats), boost),
      states_(std::move(states)),
      positions_(std::move(positions)) {
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

    conjunction::doc_iterators_t itrs;
    itrs.reserve(phrase_state->terms.size());

    phrase_iterator::positions_t positions;
    positions.reserve(phrase_state->terms.size());

    // find term using cached state
    auto terms = phrase_state->reader->iterator();
    auto position = positions_.begin();

    for (auto& term_state : phrase_state->terms) {
      // use bytes_ref::blank here since we do not need just to "jump"
      // to cached state, and we are not interested in term value itself */
      if (!terms->seek(bytes_ref::NIL, *term_state.first)) {
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

    return std::make_shared<phrase_iterator>(
      std::move(itrs),
      std::move(positions),
      rdr,
      *phrase_state->reader,
      stats(),
      ord,
      boost()
    );
  }

 private:
  states_t states_;
  positions_t positions_;
}; // phrase_query

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
  auto collectors = ord.prepare_collectors(phrase_.size());

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
    // get term metadata
    auto& meta = term->attributes().get<term_meta>();
    size_t term_itr = 0;

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
      collectors.collect(sr, *tr, term_itr, term->attributes()); // collect statistics

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
  bstring stats(ord.stats_size(), 0); // aggregated phrase stats
  phrase_query::positions_t positions(phrase_.size());
  auto pos_itr = positions.begin();

  for(auto& term: phrase_) {
    *pos_itr = position::value_t(term.first - base_offset);
    ++pos_itr;
  }

  collectors.finish(const_cast<byte_type*>(stats.data()), rdr);

  return memory::make_shared<phrase_query>(
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
