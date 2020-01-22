////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "ngram_similarity_filter.hpp"
#include "min_match_disjunction.hpp"
#include "term_query.hpp"

#include <boost/functional/hash.hpp>

#include "shared.hpp"
#include "cost.hpp"
#include "analysis/token_attributes.hpp"

#include "index/index_reader.hpp"
#include "index/field_meta.hpp"
#include "utils/misc.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
///@class serial_min_match_disjunction
///@brief min_match_disjunction with strict serail order of matched terms
//////////////////////////////////////////////////////////////////////////////
template<typename DocIterator>
class serial_min_match_disjunction : public min_match_disjunction<DocIterator> {
 public:
  typedef std::vector<pointer_wrapper<position>> positions_t;
  using base = min_match_disjunction<DocIterator>;
  
  serial_min_match_disjunction(doc_iterators_t&& itrs,
    size_t min_match_count = 1,
    const order::prepared& ord = order::prepared::unordered())
    : base(std::forward<doc_iterators_t>(itrs), min_match_count, ord) {
    pos_.reserve(itrs_.size());
    for (const auto& itr : itrs_) {
      auto& attrs = itr->attributes();
      // get needed positions for iterators
      auto& pos = attrs.get<position>();
      if (pos) {
        pos_.emplace_back(pos);
      } else {
        pos_.emplace_back(nullptr);
      }
    }
  }

  virtual bool next() override {
    bool next = false;
    while (true == (next = base::next()) && !check_serial_positions()) {}
    return next;
  }

  virtual doc_id_t seek(doc_id_t target) override {
    const auto doc = base::seek(target);

    if (doc_limits::eof(doc) || check_serial_positions()) {
      return doc;
    }

    next();
    return this->value();
  }

 private:
  struct search_state {
    explicit search_state(uint32_t n): len(1), next_pos(n) {}
    size_t len;
    uint32_t next_pos;
  };
  using search_states_t = std::vector<search_state>;


  bool check_serial_positions() {
    search_states_t search_buf;
    size_t matched = 0;
    assert(pos_.size() == itrs_.size());
    positions_t::const_iterator pos_it = pos_.begin();
    for (const auto& doc : itrs_) {
      if (doc->value() == doc_.value && (*pos_it).get() != nullptr) {
        position& pos = *(*pos_it).get();
        auto found = search_buf.begin();
        while (pos.next()) {
          auto new_pos = pos.value();
          if (found != search_buf.end()) {
            found = std::find_if(found, search_buf.end(), [new_pos](const search_state& p) {
              return p.next_pos == new_pos;
              });
          }
          if (found != search_buf.end()) {
            ++(found->len);
            ++(found->next_pos);
            if (found->len > matched) {
              matched = found->len;
            }
          } else  {
            search_buf.emplace_back(new_pos + 1);
            found = search_buf.end();
            if (!matched) {
              matched = 1;
            }
          }
        }
      } else {
        // seal all current matches. As the doc iterator is missing term - this series are done
        std::for_each(search_buf.begin(), search_buf.end(), [](search_state& p) {
          ++(p.next_pos); 
        });
      }
      ++pos_it;
    }
    // !! full matched will be needed for counting score addition!
    return matched >= min_match_count_;
  }

  positions_t pos_;
};

//////////////////////////////////////////////////////////////////////////////
/// @class ngram_similarity_query
/// @brief prepared ngram similarity query implementation
//////////////////////////////////////////////////////////////////////////////
class ngram_similarity_query : public filter::prepared {
 public:
  typedef std::vector<reader_term_state> terms_states_t;
  typedef states_cache<terms_states_t> states_t;
  typedef std::vector<bstring> stats_t;

  DECLARE_SHARED_PTR(ngram_similarity_query);

  ngram_similarity_query(size_t min_match_count, states_t&& states, boost_t boost = no_boost())
      :prepared(boost), min_match_count_(min_match_count), states_(std::move(states)) {}

  using filter::prepared::execute;

  virtual doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord,
      const attribute_view& /*ctx*/) const override {
    auto query_state = states_.find(rdr);
    if (!query_state) {
      // invalid state
      return doc_iterator::empty();
    }

    auto features = ord.features() | by_ngram_similarity::features();

    // make iterator decorator with marker of "sequence start"
    // if no term found  reset marker to true, reset marker to false after every found term

    min_match_disjunction<doc_iterator::ptr>::doc_iterators_t itrs;
    itrs.reserve(query_state->size());
    for (auto& term_state : *query_state) {
      if (term_state.reader == nullptr) {
        itrs.emplace_back(doc_iterator::empty());
        continue;
      }

      auto term = term_state.reader->iterator();

      // use bytes_ref::blank here since we do not need just to "jump"
      // to cached state, and we are not interested in term value itself */
      if (!term->seek(bytes_ref::NIL, *term_state.cookie)) {
        continue;
      }
      // get postings
      auto docs = term->postings(features);
      // add iterator
      itrs.emplace_back(std::move(docs));
    }

    if (itrs.size() < min_match_count_) {
      return doc_iterator::empty();
    }

    return memory::make_shared<serial_min_match_disjunction<doc_iterator::ptr>>(
      std::move(itrs), min_match_count_, ord);
  }

 private:
  size_t min_match_count_;
  states_t states_;
};

// -----------------------------------------------------------------------------
// --SECTION--                                by_ngram_similarity implementation
// -----------------------------------------------------------------------------

/* static */ const flags& by_ngram_similarity::features() {
  static flags req{ frequency::type(), position::type() };
  return req;
}

DEFINE_FILTER_TYPE(by_ngram_similarity)
DEFINE_FACTORY_DEFAULT(by_ngram_similarity)

by_ngram_similarity::by_ngram_similarity(): filter(by_ngram_similarity::type()) {
}

bool by_ngram_similarity::equals(const filter& rhs) const noexcept {
  const by_ngram_similarity& trhs = static_cast<const by_ngram_similarity&>(rhs);
  return filter::equals(rhs) && fld_ == trhs.fld_ && ngrams_ == trhs.ngrams_ && threshold_ == trhs.threshold_;
}

size_t by_ngram_similarity::hash() const noexcept {
  size_t seed = 0;
  ::boost::hash_combine(seed, filter::hash());
  ::boost::hash_combine(seed, fld_);
  std::for_each(
    ngrams_.begin(), ngrams_.end(),
    [&seed](const by_ngram_similarity::term_t& term) {
      ::boost::hash_combine(seed, term);
    });
  ::boost::hash_combine(seed, threshold_);
  return seed;
}

filter::prepared::ptr by_ngram_similarity::prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    const attribute_view& /*ctx*/) const {
  if (ngrams_.empty() || fld_.empty() || threshold_ < 0. || threshold_ > 1.) {
    // empty field or terms
    return filter::prepared::empty();
  }

  size_t min_match_count = static_cast<size_t>(std::ceil(static_cast<double>(ngrams_.size()) * threshold_));


  ngram_similarity_query::states_t query_states(rdr.size());

  // per segment terms states
  ngram_similarity_query::terms_states_t term_states;
  term_states.reserve(ngrams_.size());

  // TODO: implement statistic collection
  //std::vector<order::prepared::collectors> term_stats;
  //term_stats.reserve(terms_.size());

  //for(auto size = terms_.size(); size; --size) {
  //  term_stats.emplace_back(ord.prepare_collectors(1)); // 1 term per bstring because a range is treated as a disjunction
  //}

  for (const auto& segment : rdr) {
    //auto term_itr = term_stats.begin();

    // get term dictionary for field
    const term_reader* field = segment.field(fld_);
    if (!field) {
      continue;
    }

    // check required features
    if (!features().is_subset_of(field->meta().features)) {
      continue;
    }
    size_t count_terms = 0;
    for (const auto& ngram : ngrams_) {
      //auto next_stats = irs::make_finally([&term_itr]()->void{ ++term_itr; });
      //term_itr->collect(segment, *field); // collect field statistics once per segment

      // find terms
      seek_term_iterator::ptr term = field->iterator();

      //if (!term->seek(ngram)) {
      //  if (ord.empty()) {
      //    break;
      //  } else {
      //    // continue here because we should collect
      //    // stats for other terms in phrase
      //    continue;
      //  }
      //}
      term_states.emplace_back();
      auto& state = term_states.back();
      if (term->seek(ngram)) {
        term->read(); // read term attributes
        //term_itr->collect(segment, *field, 0, term->attributes()); // collect statistics, 0 because only 1 term
        state.cookie = term->cookie();
        state.reader = field;
        ++count_terms;
      }
    }

    if (count_terms < min_match_count) {
      // we have not found enough terms
      term_states.clear();
      continue;
    }

    auto& state = query_states.insert(segment);
    state = std::move(term_states);

    term_states.reserve(ngrams_.size());
  }
  return memory::make_shared<ngram_similarity_query>(
      min_match_count,
      std::move(query_states),
      /*this->boost() **/ boost);
}

NS_END
