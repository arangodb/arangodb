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
#include "disjunction.hpp"
#include <boost/functional/hash.hpp>
#include "shared.hpp"
#include "cost.hpp"
#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "index/field_meta.hpp"
#include "utils/misc.hpp"

NS_ROOT

NS_LOCAL

struct ngram_segment_state_t {
  const term_reader* field{};
  std::vector<seek_term_iterator::cookie_ptr> terms;
};

typedef states_cache<ngram_segment_state_t> states_t;
typedef std::vector<bstring> stats_t;

NS_END

//////////////////////////////////////////////////////////////////////////////
///@class ngram_similarity_doc_iterator
///@brief adapter for min_match_disjunction with honor of terms orderings
//////////////////////////////////////////////////////////////////////////////
template<typename DocIterator>
class ngram_similarity_doc_iterator : public doc_iterator_base, score_ctx {
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

  ngram_similarity_doc_iterator(doc_iterators_t&& itrs,
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
      order::prepared::unordered()),// we are not interested in disjunction`s scoring
      ord_(&ord), states_(states) {
    scores_vals_.resize(pos_.size());

    attrs_.emplace(seq_freq_); 
    attrs_.emplace<document>() = disjunction_.attributes().get<document>();
    attrs_.emplace<filter_boost>(filter_boost_);

    if (scr_.prepare(ord, ord.prepare_scorers(segment, field, stats, attrs_, boost))) {
      attrs_.emplace(scr_);
    }

    prepare_score(ord, this, [](const score_ctx* ctx, byte_type* score) {
      auto& self = const_cast<ngram_similarity_doc_iterator<DocIterator>&>(
        *static_cast<const ngram_similarity_doc_iterator<DocIterator>*>(ctx)
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
    explicit search_state(size_t pos, const position_t* s) : len(1), sequence{ s }, pos_sequence{pos} {}
    search_state(search_state&&) = default;
    search_state(const search_state&) = default;
    search_state& operator=(const search_state&) = default;

    size_t len;
    std::vector<const position_t*> sequence;
    std::vector<size_t> pos_sequence;
  };
   
  using search_states_t = std::map<uint32_t, search_state, std::greater<uint32_t>>;
  using pos_temp_t = std::vector<std::pair<uint32_t, search_state>>;

  bool check_serial_positions() {
    size_t matched_iters = disjunction_.count_matched();
    search_buf_.clear();
    size_t matched = 0;
    auto best_match = search_buf_.end();
    size_t skipped_matched = 0;
    seq_freq_.value = 0;
    for (const auto& post : pos_) {
      size_t potential = matched_iters - skipped_matched;
      if (post.doc->value == disjunction_.value()) {
        position& pos = *(post.pos);
        skipped_matched++;
        if (potential <= matched || potential < min_match_count_) {
          // this term could not start largest sequence. 
          // skip it to first position to append to any existing candidates  
          assert(!search_buf_.empty());
          pos.seek(search_buf_.rbegin()->first + 1);
        } else {
          pos.next();
        }
        if (pos_limits::eof(pos.value())) {
          continue;
        }
        pos_temp_t temp_cache;
        auto last_found_pos = pos_limits::invalid();
        do {
          auto new_pos = pos.value();
          auto found = search_buf_.lower_bound(new_pos);
          if (found != search_buf_.end()) {
            if (last_found_pos != found->first) {
              last_found_pos = found->first;
              auto new_found = found->second;
              if (found->first != new_pos) {
                ++(new_found.len);
                new_found.sequence.emplace_back(&post);
                new_found.pos_sequence.push_back(new_pos);
              } else {
                new_found.len = 0;
              }
              if (new_found.len > matched) {
                matched = new_found.len;
              } else {
                // maybe some previous candidates could produce better results.
                // lets go downward and check if there are any candidates which could became longer
                // if we stick this ngram to them rather than the closest one found
                for (++found; found != search_buf_.end(); ++found) {
                  auto down_found = found->second;
                  ++(down_found.len);
                  down_found.sequence.emplace_back(&post);
                  down_found.pos_sequence.push_back(new_pos);
                  if (down_found.len > new_found.len) {
                    // we have better option. Replace this match!
                    new_found = down_found;
                    if (down_found.len > matched) {
                      matched = down_found.len;
                      break; // this match is the best - nothing to search further
                    }
                  }
                }
              }
              if (new_found.len) {
                temp_cache.emplace_back(new_pos, std::move(new_found));
              }
            }
          } else  if (potential > matched && potential >= min_match_count_) {
            // this ngram at this position  could potentially start a long enough sequence
            // so add it to candidate list
            temp_cache.emplace_back(std::piecewise_construct, 
                std::forward_as_tuple(new_pos), 
                std::forward_as_tuple(new_pos, &post));
            if (!matched) {
              matched = 1;
            }
          }
        } while (pos.next());
        for (const auto& p : temp_cache) {
          auto res = search_buf_.insert(p);
          if (!res.second) {
            // pos already used. This could be if same ngram used several times. Replace
            res.first->second = p.second;
          }
        }
        --potential; // we are done with this term. Next will have potential one less
      }

      if (matched + potential < min_match_count_) {
        break; // all further terms will not let us build long enough sequence
      }
      
      if (!potential) {
        break; // all further terms will not add anything
      }
      
      // if we have no scoring - we could stop searh once we got enough matches
      if (matched >= min_match_count_ && ord_->empty()) {
        break;
      }
    }
    // deduplicate best match and count frequency for scoring
    // For now if several different sequences are longest - 
    // only most frequent one is counted
    if (matched >= min_match_count_  && !ord_->empty() ) { 
      std::set<size_t> used_pos;
      for (auto i = search_buf_.begin(), end = search_buf_.end(); i != end;) {
        if (i->second.len < matched) {
          i = search_buf_.erase(i);
        } else {
          // check if this positions are used in some other
          bool delete_candidate = false;
          for (auto p : i->second.pos_sequence) {
            if (used_pos.find(p) != used_pos.end()) {
              delete_candidate = true;
              break;
            }
          }
          if (delete_candidate) {
            i = search_buf_.erase(i);
          } else {
            for (auto p : i->second.pos_sequence) {
              used_pos.insert(p);
            }
            ++i;
          }
        }
      }
      using sequence_counter_t = std::map<std::vector<const position_t*>, uint32_t>;
      sequence_counter_t sequence_counter;
      std::for_each(search_buf_.begin(), search_buf_.end(), [&sequence_counter](const search_states_t::value_type& v) {
        sequence_counter[v.second.sequence]++;
      });
      uint32_t max_freq = 0;
      std::for_each(sequence_counter.begin(), sequence_counter.end(), [&max_freq](const sequence_counter_t::value_type& v) {
        if (v.second > max_freq) {
          max_freq = v.second;
        }
      });
      seq_freq_.value = max_freq;
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
      const attribute_view&) const override {
    typedef disjunction<doc_iterator::ptr> disjunction_t;
    auto query_state = states_.find(rdr);
    if (!query_state || !query_state->field) {
      // invalid state
      return doc_iterator::empty();
    }
    
    if (1 == min_match_count_ && ord.empty()) {
      return execute_simple_disjunction(*query_state);
    } else {
      return execute_ngram_similarity(rdr, *query_state, ord);
    }
  }

 private:
   doc_iterator::ptr execute_simple_disjunction(
       const ngram_segment_state_t& query_state) const {
     using disjunction_t = irs::disjunction<doc_iterator::ptr>;
     disjunction_t::doc_iterators_t itrs;
     itrs.reserve(query_state.terms.size());
     for (auto& term_state : query_state.terms) {
       if (term_state == nullptr) {
         // here we skip empty as no relative order of ngram matters
         continue;
       }
       auto term = query_state.field->iterator();

       // use bytes_ref::blank here since we do not need just to "jump"
       // to cached state, and we are not interested in term value itself */
       if (!term->seek(bytes_ref::NIL, *term_state)) {
         // here we skip empty as no relative order of ngram matters
         continue;
       }

       // get postings
       auto docs = term->postings(irs::flags::empty_instance());
       assert(docs);

       // add iterator
       itrs.emplace_back(std::move(docs));
     }

     if (itrs.empty()) {
       return doc_iterator::empty();
     }
     return make_disjunction<disjunction_t>(std::move(itrs));

   }

   doc_iterator::ptr execute_ngram_similarity (
       const sub_reader& rdr,
       const ngram_segment_state_t& query_state,
       const order::prepared& ord) const {
     min_match_disjunction<doc_iterator::ptr>::doc_iterators_t itrs;
     itrs.reserve(query_state.terms.size());
     auto features = ord.features() | by_ngram_similarity::features();
     for (auto& term_state : query_state.terms) {
       if (term_state == nullptr) {
         itrs.emplace_back(doc_iterator::empty());
         continue;
       }
       auto term = query_state.field->iterator();

       // use bytes_ref::blank here since we do not need just to "jump"
       // to cached state, and we are not interested in term value itself */
       if (!term->seek(bytes_ref::NIL, *term_state)) {
         itrs.emplace_back(doc_iterator::empty());
         continue;
       }

       // get postings
       auto docs = term->postings(features);
       assert(docs);

       // add iterator
       itrs.emplace_back(std::move(docs));
     }

     if (itrs.size() < min_match_count_) {
       return doc_iterator::empty();
     }
     return memory::make_shared<ngram_similarity_doc_iterator<doc_iterator::ptr>>(
       std::move(itrs), states_, rdr, *query_state.field, boost(), stats_.c_str(), min_match_count_, ord);
   }

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
    // empty field or terms or invalid threshold
    return filter::prepared::empty();
  }

  size_t min_match_count = static_cast<size_t>(std::ceil(static_cast<double>(ngrams_.size()) * threshold_));

  states_t query_states(rdr.size());

  // per segment terms states
  ngram_segment_state_t term_states;
  term_states.terms.reserve(ngrams_.size());

  // prepare ngrams stats 
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

    term_states.field = field;
    collectors.collect(segment, *field); // collect field statistics once per segment
    size_t term_itr = 0;
    size_t count_terms = 0;
    for (const auto& ngram : ngrams_) {
      auto next_stats = irs::make_finally([&term_itr]()->void{ ++term_itr; });
      // find terms
      seek_term_iterator::ptr term = field->iterator();

      term_states.terms.emplace_back();
      auto& state = term_states.terms.back();
      if (term->seek(ngram)) {
        term->read(); // read term attributes
        collectors.collect(segment, *field, term_itr, term->attributes()); // collect statistics
        state = term->cookie();
        ++count_terms;
      }
    }
    if (count_terms < min_match_count) {
      // we have not found enough terms
      term_states.terms.clear();
      term_states.field = nullptr;
      continue;
    }

    auto& state = query_states.insert(segment);
    state = std::move(term_states);

    term_states.terms.reserve(ngrams_.size());
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
