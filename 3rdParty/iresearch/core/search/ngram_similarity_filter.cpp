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

NS_LOCAL
typedef std::vector<reader_term_state> terms_states_t;
typedef states_cache<terms_states_t> states_t;
typedef std::vector<bstring> stats_t;
NS_END

//////////////////////////////////////////////////////////////////////////////
///@class serial_min_match_disjunction
///@brief min_match_disjunction with strict serail order of matched terms
//////////////////////////////////////////////////////////////////////////////
template<typename DocIterator>
class serial_min_match_disjunction : public doc_iterator_base, score_ctx {
 public: 

  struct position_t {
    position_t(position* p, document* d, score* s)
      : pos(p), doc(d), score(s) {}
    position* pos;
    document* doc;
    score* score;
  };

  typedef std::vector<position_t> positions_t;
  using base = min_match_disjunction<DocIterator>;
  using doc_iterators_t = typename base::doc_iterators_t;

  static positions_t extract_positions(const doc_iterators_t& itrs) {
    positions_t pos;
    pos.reserve(itrs.size());
    for (const auto& itr : itrs) {
      auto& attrs = itr->attributes();
      // get needed positions for iterators
      auto& p = attrs.get<position>();
      auto& d = attrs.get<document>();
      auto& s = attrs.get<score>();
      pos.emplace_back(p.get(), d.get(), s.get());
    }
    return pos;
  }

  serial_min_match_disjunction(doc_iterators_t&& itrs,
    const states_t& states,
    const sub_reader& segment,
    const term_reader& field,
    boost_t boost,
    const byte_type* stats,
    size_t min_match_count = 1,
    const order::prepared& ord = order::prepared::unordered())
    : pos_(extract_positions(itrs)),
      min_match_count_(min_match_count),
      disjunction_(std::forward<doc_iterators_t>(itrs), min_match_count,
      order::prepared::unordered()),// we are not interested in base`s scoring
      ord_(&ord), states_(states) {
    scores_vals_.resize(pos_.size());
    attrs_.emplace(seq_freq_); // expose frequency to scorers
    attrs_.emplace<document>() = disjunction_.attributes().get<document>();
    attrs_.emplace<filter_boost>(filter_boost_);

    if (scr_.prepare(ord, ord.prepare_scorers(segment, field, stats, attrs_, boost))) {
      attrs_.emplace(scr_);
    }

    prepare_score(ord, this, [](const score_ctx* ctx, byte_type* score) {
      auto& self = const_cast<serial_min_match_disjunction<DocIterator>&>(
        *static_cast<const serial_min_match_disjunction<DocIterator>*>(ctx)
        );
      self.score_impl(score);
    });
  }

  virtual bool next() override {
    bool next = false;
    while (true == (next = disjunction_.next()) && !check_serial_positions()) {}
    return next;
  }

  virtual doc_id_t value() const override {
    return disjunction_.value();
  }

  virtual doc_id_t seek(doc_id_t target) override {
    const auto doc = disjunction_.seek(target);

    if (doc_limits::eof(doc) || check_serial_positions()) {
      return doc;
    }

    next();
    return disjunction_.value();
  }

 private:

  inline void score_impl(byte_type* lhs) {
    const irs::byte_type** pVal = scores_vals_.data();
    const auto& winner = search_buf_.begin()->second.sequence;
    std::for_each(
      winner.begin(), winner.end(),
      [this, lhs, &pVal](const position_t* s) {
        if (&irs::score::no_score() != s->score) {
          s->score->evaluate();
          *pVal++ = s->score->c_str();
        }
      });
    ord_->merge(lhs, scores_vals_.data(), std::distance(scores_vals_.data(), pVal));
  }

  struct search_state {
    explicit search_state(uint32_t n, const position_t* s) : len(1), next_pos(n), sequence{ s } {}
    search_state(search_state&&) = default;

    size_t len;
    uint32_t next_pos;
    std::vector<const position_t*> sequence;
  };
   
  using search_states_t = std::map<uint32_t, search_state, std::greater<uint32_t>>;

  bool check_serial_positions() {
    size_t matched_iters = disjunction_.count_matched();
    search_buf_.clear();
    size_t matched = 0;
    size_t skipped_matched = 0;
    seq_freq_.value = 0;
    for (const auto& post : pos_) {
      if (post.doc->value == disjunction_.value()) {
        position& pos = *(post.pos);
        const size_t potential = matched_iters - skipped_matched;
        while (pos.next()) {
          auto new_pos = pos.value();
          
          auto found = search_buf_.lower_bound(new_pos);
          if (found != (search_buf_.end())) {
            if (found->second.sequence.back() != &post) {// we found ourselves
              ++(found->second.len);
              found->second.sequence.emplace_back(&post);
              if (found->second.len > matched) {
                  matched = found->second.len;
              }
              auto tmp(std::move(found->second));
              search_buf_.erase(found);
              search_buf_.emplace(new_pos, std::move(tmp));
            }
          } else  if ( potential > matched && potential >= min_match_count_) {
            // this ngram at this position  could potentially start a long enough sequence
            // so add it to candidate list
            search_buf_.emplace(std::piecewise_construct, 
                std::forward_as_tuple(new_pos), 
                std::forward_as_tuple(new_pos, &post));
            if (!matched) {
              matched = 1;
            }
          }
        }
        ++skipped_matched;
      }
      // Remove all now hopeless candidates !!Maybe not waste time there ? Let them live till next term
      const size_t potential = matched_iters - skipped_matched;
      //search_buf_.erase(
      //  std::remove_if(search_buf_.begin(), search_buf_.end(), [potential, matched](search_state& p) {
      //    return (p.len + potential) < matched;
      //    }),
      //  search_buf_.end());
      // Next term. Shift all expectations pos.
      //std::for_each(search_buf_.begin(), search_buf_.end(), [](search_state& p) {
      //  ++(p.next_pos);
      //});
      if (!potential) {
        break; // all further terms will not add anything
      }
      // if we have no scoring - we could stop searh once we got enough matches
      if (matched >= min_match_count_ && ord_->empty()) {
        break;
      }
    }
    if (matched >= min_match_count_  && !ord_->empty() ) { 
      // deduplicating  longest sequences  and cleanup lesser ones
      //search_buf_.erase(
      //  std::remove_if(search_buf_.begin(), search_buf_.end(), [this, matched](search_state& p) {
      //    return p.len < matched || p.sequence != search_buf_.front()->second.sequence;
      //    }),
      //  search_buf_.end());
      seq_freq_.value = 1; //;search_buf_.size();
      assert(!pos_.empty());
      filter_boost_.value = (boost_t)matched / (boost_t)pos_.size();
    }
    return matched >= min_match_count_;
  }
  positions_t pos_;
  frequency seq_freq_; // longest sequence frequency
  filter_boost filter_boost_;
  size_t min_match_count_;
  min_match_disjunction<DocIterator> disjunction_;
  const order::prepared* ord_;
  mutable std::vector<const irs::byte_type*> scores_vals_;
  search_states_t search_buf_;
  const states_t& states_;
  score scr_;
};

//////////////////////////////////////////////////////////////////////////////
/// @class ngram_similarity_query
/// @brief prepared ngram similarity query implementation
//////////////////////////////////////////////////////////////////////////////
class ngram_similarity_query : public filter::prepared {
 public:

  DECLARE_SHARED_PTR(ngram_similarity_query);

  ngram_similarity_query(size_t min_match_count, states_t&& states, bstring&& stats, boost_t boost = no_boost())
      :prepared(boost), min_match_count_(min_match_count), states_(std::move(states)), stats_(std::move(stats)) {}

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
        itrs.emplace_back(doc_iterator::empty());
        continue;
      }

      // get postings
      auto docs = term->postings(features);
      assert(docs);
      //auto& attrs = docs->attributes();
      //auto& score = attrs.get<irs::score>();
      //// set score
      //if (score) {
      //  score->prepare(ord, ord.prepare_scorers(rdr, *term_state.reader, stats_.c_str(), attrs, boost()));
      //}

      // add iterator
      itrs.emplace_back(std::move(docs));
    }

    if (itrs.size() < min_match_count_) {
      return doc_iterator::empty();
    }

    //!!!HACK
    auto field = query_state->front().reader;
    for (const auto& qs : *query_state)  {
      field = qs.reader;
      if (field) {
        break;
      }
    }
    if (field == nullptr) {
      return doc_iterator::empty();
    }

    return memory::make_shared<serial_min_match_disjunction<doc_iterator::ptr>>(
      //!!! reader should be only one!!!
      std::move(itrs), states_, rdr, *field, boost(), stats_.c_str(),  min_match_count_, ord);
  }

 private:
  size_t min_match_count_;
  states_t states_;
  bstring stats_;
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


  states_t query_states(rdr.size());

  // per segment terms states
  terms_states_t term_states;
  term_states.reserve(ngrams_.size());

  // prepare phrase stats (collector for each term)
  auto collectors = ord.prepare_collectors(ngrams_.size());


  for (const auto& segment : rdr) {
    // get term dictionary for field
    const term_reader* field = segment.field(fld_);
    if (!field) {
      continue;
    }

    // check required features
    if (!features().is_subset_of(field->meta().features)) {
      continue;
    }

    collectors.collect(segment, *field); // collect field statistics once per segment
    size_t term_itr = 0;
    size_t count_terms = 0;
    for (const auto& ngram : ngrams_) {
      auto next_stats = irs::make_finally([&term_itr]()->void{ ++term_itr; });
      // find terms
      seek_term_iterator::ptr term = field->iterator();

      term_states.emplace_back();
      auto& state = term_states.back();
      if (term->seek(ngram)) {
        term->read(); // read term attributes
        collectors.collect(segment, *field, term_itr, term->attributes()); // collect statistics
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

  bstring stats(ord.stats_size(), 0);
  auto* stats_buf = const_cast<byte_type*>(stats.data());

  ord.prepare_stats(stats_buf);
  collectors.finish(stats_buf, rdr);

  return memory::make_shared<ngram_similarity_query>(
      min_match_count,
      std::move(query_states),
      std::move(stats),
      this->boost() * boost);
}

NS_END
