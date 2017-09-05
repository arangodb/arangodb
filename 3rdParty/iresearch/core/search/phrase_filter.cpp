//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#include "shared.hpp"
#include "phrase_filter.hpp"
#include "cost.hpp"
#include "term_query.hpp"
#include "conjunction.hpp"

#if defined(_MSC_VER)
  #pragma warning( disable : 4706 )
#elif defined (__GNUC__)
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

  DECLARE_SPTR(phrase_query);

  phrase_query(states_t&& states, phrase_stats_t&& stats)
    : states_(std::move(states)),
      stats_(std::move(stats)) {
  }

  using filter::prepared::execute;

  virtual score_doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord) const override {
    typedef detail::conjunction<score_wrapper<
      score_doc_iterator::ptr>
    > conjunction_t;
    typedef phrase_iterator<conjunction_t> phrase_iterator_t;

    // get phrase state for the specified reader
    auto phrase_state = states_.find(rdr);
    if (!phrase_state) {
      // invalid state 
      return score_doc_iterator::empty();
    }

    // get features required for query & order
    auto features = ord.features() | by_phrase::required();

    phrase_iterator_t::doc_iterators_t itrs;
    itrs.reserve(phrase_state->terms.size());

    phrase_iterator_t::positions_t positions;
    positions.reserve(phrase_state->terms.size());

    // find term using cached state
    auto terms = phrase_state->reader->iterator();
    auto term_stats = stats_.begin();
    for (auto& term_state : phrase_state->terms) {
      // use bytes_ref::nil here since we do not need just to "jump"
      // to cached state, and we are not interested in term value itself */
      if (!terms->seek(bytes_ref::nil, *term_state.first)) {
        return score_doc_iterator::empty();
      }

      // get postings
      auto docs = terms->postings(features);

      // get needed postings attributes
      auto& pos = docs->attributes().get<position>();
      if (!pos) {
        // positions not found
        return score_doc_iterator::empty();
      }
      positions.emplace_back(std::cref(*pos), term_stats->second);

      // add base iterator
      itrs.emplace_back(score_doc_iterator::make<basic_score_iterator>(
        rdr,
        *phrase_state->reader,
        term_stats->first, 
        std::move(docs), 
        ord, 
        term_state.second
      ));

      ++term_stats;
    }

    return detail::make_conjunction<phrase_iterator_t>(
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

bool by_phrase::equals(const filter& rhs) const {
  const by_phrase& trhs = static_cast<const by_phrase&>(rhs);
  return filter::equals(rhs) && fld_ == trhs.fld_ && phrase_ == trhs.phrase_;
}

size_t by_phrase::hash() const {
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
    boost_t boost) const {
  if (fld_.empty() || phrase_.empty()) {
    // empty field or phrase
    return filter::prepared::empty();
  }

  // per segment phrase states 
  phrase_query::states_t phrase_states(rdr.size());

  // per segment phrase terms
  phrase_state::terms_states_t phrase_terms;
  phrase_terms.reserve(phrase_.size());

  // prepare phrase stats
  std::vector<order::prepared::stats> phrase_stats;
  phrase_stats.reserve(phrase_.size());
  for(auto& word : phrase_) {
    UNUSED(word);
    phrase_stats.emplace_back(ord.prepare_stats());
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

    auto term_stats = phrase_stats.begin();
    term_stats->field(sr, *tr);

    for(auto& word: phrase_) {
      if (!term->seek(word.second)) {
        if (ord.empty()) {
          break;
        } else {
          // continue here because we should collect 
          // stats for other terms in phrase
          continue;
        }
      }

      // read term attributes
      term->read();

      // estimate phrase & term
      const cost::cost_t term_estimation = meta 
        ? meta->docs_count 
        : cost::MAX;
      phrase_terms.emplace_back(term->cookie(), term_estimation);

      // collect stats
      term_stats->term(term->attributes());
      ++term_stats;
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
  phrase_query::phrase_stats_t stats;
  stats.reserve(phrase_.size());

  auto term_stats = phrase_stats.begin();
  for(auto& word : phrase_) {
    stats.emplace_back();

    auto& stat = stats.back();
    term_stats->finish(rdr, stat.first);
    stat.second = position::value_t(word.first-base_offset);

    ++term_stats;
  }

  auto q = memory::make_unique<phrase_query>(
    std::move(phrase_states),
    std::move(stats)
  );

  // apply boost
  iresearch::boost::apply(q->attributes(), this->boost() * boost);

  return std::move(q);
}

NS_END // ROOT
