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

#include <set>

#include "shared.hpp"
#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "index/field_meta.hpp"
#include "search/collectors.hpp"
#include "search/cost.hpp"
#include "search/disjunction.hpp"
#include "search/min_match_disjunction.hpp"
#include "search/states_cache.hpp"
#include "utils/misc.hpp"
#include "utils/map_utils.hpp"

namespace {

using namespace irs;

struct ngram_segment_state_t {
  const term_reader* field{};
  std::vector<seek_term_iterator::cookie_ptr> terms;
};

using states_t = states_cache<ngram_segment_state_t>;
using approximation = min_match_disjunction<doc_iterator::ptr>;

}

namespace iresearch {

//////////////////////////////////////////////////////////////////////////////
///@class ngram_similarity_doc_iterator
///@brief adapter for min_match_disjunction with honor of terms orderings
//////////////////////////////////////////////////////////////////////////////
class ngram_similarity_doc_iterator final
    : public doc_iterator, private score_ctx {
 public:
  ngram_similarity_doc_iterator(
      approximation::doc_iterators_t&& itrs,
      const sub_reader& segment,
      const term_reader& field,
      boost_t boost,
      const byte_type* stats,
      size_t total_terms_count,
      size_t min_match_count = 1,
      const order::prepared& ord = order::prepared::unordered())
    : pos_(itrs.begin(), itrs.end()),
      approx_(std::move(itrs), min_match_count), // we are not interested in disjunction`s scoring
      min_match_count_(min_match_count),
      total_terms_count_(static_cast<boost_t>(total_terms_count)), // avoid runtime conversion
      empty_order_(ord.empty()) {
    std::get<attribute_ptr<document>>(attrs_) = irs::get_mutable<document>(&approx_);

    // FIXME find a better estimation
    std::get<cost>(attrs_).reset(
      [this](){ return cost::extract(approx_); });

    if (!empty_order_) {
      auto& score = std::get<irs::score>(attrs_);

      score.realloc(ord);

      order::prepared::scorers scorers(
        ord, segment, field, stats,
        score.data(), *this, boost);

      irs::reset(score, std::move(scorers));
    }
  }

  virtual attribute* get_mutable(type_info::type_id type) noexcept override {
    return irs::get_mutable(attrs_, type);
  }

  virtual bool next() override {
    bool next = false;
    while ((next = approx_.next()) && !check_serial_positions()) {}
    return next;
  }

  virtual doc_id_t value() const override {
    return std::get<attribute_ptr<document>>(attrs_).ptr->value;
  }

  virtual doc_id_t seek(doc_id_t target) override {
    auto* doc_ = std::get<attribute_ptr<document>>(attrs_).ptr;

    if (doc_->value >= target) {
      return doc_->value;
    }
    const auto doc = approx_.seek(target);

    if (doc_limits::eof(doc) || check_serial_positions()) {
      return doc;
    }

    next();
    return doc_->value;
  }

 private:
  struct position_t {
    template<typename Iterator>
    position_t(Iterator& itr)
      : pos(&position::get_mutable(itr)),
        doc(irs::get<document>(itr)),
        scr(&irs::score::get(itr)) {
      assert(pos);
      assert(doc);
      assert(scr);
    }

    position* pos;
    const document* doc;
    const score* scr;
  };

  struct search_state {
    search_state(size_t p, const score* s) : parent{nullptr}, scr{s}, pos{p}, len(1) {}
    search_state(search_state&&) = default;
    search_state(const search_state&) = default;
    search_state& operator=(const search_state&) = default;

    // appending constructor
    search_state(std::shared_ptr<search_state>& other, size_t p, const score* s)
      : parent{other}, scr{s}, pos{p}, len(other->len + 1) {}

    std::shared_ptr<search_state> parent;
    const score* scr;
    size_t pos;
    size_t len;
  };

  using search_states_t = std::map<uint32_t, std::shared_ptr<search_state>, std::greater<uint32_t>>;
  using pos_temp_t = std::vector<std::pair<uint32_t, std::shared_ptr<search_state>>>;
  using attributes = std::tuple<
    attribute_ptr<document>,
    frequency,
    cost,
    score,
    filter_boost>;

  bool check_serial_positions();

  std::vector<position_t> pos_;
  approximation approx_;
  attributes attrs_;
  std::set<size_t> used_pos_; // longest sequence positions overlaping detector
  std::vector<const score*> longest_sequence_;
  std::vector<size_t> pos_sequence_;
  size_t min_match_count_;
  search_states_t search_buf_;
  boost_t total_terms_count_;
  bool empty_order_;
};

bool ngram_similarity_doc_iterator::check_serial_positions() {
  size_t potential = approx_.match_count(); // how long max sequence could be in the best case
  search_buf_.clear();
  size_t longest_sequence_len = 0;

  auto* doc_ = std::get<attribute_ptr<document>>(attrs_).ptr;
  auto& seq_freq_ = std::get<frequency>(attrs_);

  seq_freq_.value = 0;
  for (const auto& pos_iterator : pos_) {
    if (pos_iterator.doc->value == doc_->value) {
      position& pos = *(pos_iterator.pos);
      if (potential <= longest_sequence_len || potential < min_match_count_) {
        // this term could not start largest (or long enough) sequence.
        // skip it to first position to append to any existing candidates
        assert(!search_buf_.empty());
        pos.seek(search_buf_.rbegin()->first + 1);
      } else {
        pos.next();
      }
      if (!pos_limits::eof(pos.value())) {
        pos_temp_t swap_cache;
        auto last_found_pos = pos_limits::invalid();
        do {
          auto current_pos = pos.value();
          auto found = search_buf_.lower_bound(current_pos);
          if (found != search_buf_.end()) {
            if (last_found_pos != found->first) {
              last_found_pos = found->first;
              const auto* found_state = found->second.get();
              assert(found_state);
              auto current_sequence = found;
              // if we hit same position - set length to 0 to force checking candidates to the left
              size_t current_found_len = (found->first == current_pos ||
                                          found_state->scr == pos_iterator.scr) ? 0 : found_state->len + 1;
              auto initial_found = found;
              if (current_found_len > longest_sequence_len) {
                longest_sequence_len = current_found_len;
              } else {
                // maybe some previous candidates could produce better results.
                // lets go leftward and check if there are any candidates which could became longer
                // if we stick this ngram to them rather than the closest one found
                for (++found; found != search_buf_.end(); ++found) {
                  found_state = found->second.get();
                  assert(found_state);
                  if (found_state->scr != pos_iterator.scr &&
                      found_state->len + 1 > current_found_len) {
                    // we have better option. Replace this match!
                    current_sequence = found;
                    current_found_len = found_state->len + 1;
                    if (current_found_len > longest_sequence_len) {
                      longest_sequence_len = current_found_len;
                      break; // this match is the best - nothing to search further
                    }
                  }
                }
              }
              if (current_found_len) {
                auto new_candidate = std::make_shared<search_state>(current_sequence->second, current_pos, pos_iterator.scr);
                const auto res = search_buf_.try_emplace(current_pos, std::move(new_candidate));
                if (!res.second) {
                  // pos already used. This could be if same ngram used several times.
                  // replace with new length through swap cache - to not spoil
                  // candidate for following positions of same ngram
                  swap_cache.emplace_back(current_pos, std::move(new_candidate));
                }
              } else if (initial_found->second->scr == pos_iterator.scr &&
                         potential > longest_sequence_len && potential >= min_match_count_) {
                // we just hit same iterator and found no better place to join,
                // so it will produce new candidate
                search_buf_.emplace(std::piecewise_construct,
                  std::forward_as_tuple(current_pos),
                  std::forward_as_tuple(std::make_shared<search_state>(current_pos, pos_iterator.scr)));
              }
            }
          } else  if (potential > longest_sequence_len && potential >= min_match_count_) {
            // this ngram at this position  could potentially start a long enough sequence
            // so add it to candidate list
            search_buf_.emplace(std::piecewise_construct,
              std::forward_as_tuple(current_pos),
              std::forward_as_tuple(std::make_shared<search_state>(current_pos, pos_iterator.scr)));
            if (!longest_sequence_len) {
              longest_sequence_len = 1;
            }
          }
        } while (pos.next());
        for (auto& p : swap_cache) {
          auto res = search_buf_.find(p.first);
          assert(res != search_buf_.end());
          std::swap(res->second, p.second);
        }
      }
      --potential; // we are done with this term.
                   // next will have potential one less as less matches left

      if (!potential) {
        break; // all further terms will not add anything
      }

      if (longest_sequence_len + potential < min_match_count_) {
        break; // all further terms will not let us build long enough sequence
      }

      // if we have no scoring - we could stop searh once we got enough matches
      if (longest_sequence_len >= min_match_count_ && empty_order_) {
        break;
      }
    }
  }

  if (longest_sequence_len >= min_match_count_  && !empty_order_) {
    uint32_t freq = 0;
    size_t count_longest{ 0 };
    // try to optimize case with one longest candidate
    // performance profiling shows it is majority of cases
    for (auto i = search_buf_.begin(), end = search_buf_.end(); i != end; ++i) {
      if (i->second->len == longest_sequence_len) {
        ++count_longest;
        if (count_longest > 1) {
          break;
        }
      }
    }

    if (count_longest > 1) {
      longest_sequence_.clear();
      used_pos_.clear();
      longest_sequence_.reserve(longest_sequence_len);
      pos_sequence_.reserve(longest_sequence_len);
      for (auto i = search_buf_.begin(), end = search_buf_.end(); i != end;) {
        pos_sequence_.clear();
        const auto* state = i->second.get();
        assert(state && state->len <= longest_sequence_len);
        if (state->len == longest_sequence_len) {
          bool delete_candidate = false;
          // only first longest sequence will contribute to frequency
          if (longest_sequence_.empty()) {
            longest_sequence_.push_back(state->scr);
            pos_sequence_.push_back(state->pos);
            auto cur_parent = state->parent;
            while (cur_parent) {
              longest_sequence_.push_back(cur_parent->scr);
              pos_sequence_.push_back(cur_parent->pos);
              cur_parent = cur_parent->parent;
            }
          } else {
            if (used_pos_.find(state->pos) != used_pos_.end() ||
                state->scr != longest_sequence_[0]) {
              delete_candidate = true;
            } else {
              pos_sequence_.push_back(state->pos);
              auto cur_parent = state->parent;
              size_t j = 1;
              while (cur_parent) {
                assert(j < longest_sequence_.size());
                if (longest_sequence_[j] != cur_parent->scr ||
                  used_pos_.find(cur_parent->pos) != used_pos_.end()) {
                  delete_candidate = true;
                  break;
                }
                pos_sequence_.push_back(cur_parent->pos);
                cur_parent = cur_parent->parent;
                ++j;
              }
            }
          }
          if (!delete_candidate) {
            ++freq;
            used_pos_.insert(std::begin(pos_sequence_),
                            std::end(pos_sequence_));
          }
        }
        ++i;
      }
    } else {
      freq = 1;
    }
    seq_freq_.value = freq;
    assert(!pos_.empty());
    std::get<filter_boost>(attrs_).value
      = static_cast<boost_t>(longest_sequence_len) / total_terms_count_;
  }
  return longest_sequence_len >= min_match_count_;
}

//////////////////////////////////////////////////////////////////////////////
/// @class ngram_similarity_query
/// @brief prepared ngram similarity query implementation
//////////////////////////////////////////////////////////////////////////////
class ngram_similarity_query : public filter::prepared {
 public:
  ngram_similarity_query(
     size_t min_match_count, states_t&& states,
     bstring&& stats, boost_t boost = no_boost())
   : prepared(boost), min_match_count_(min_match_count),
     states_(std::move(states)), stats_(std::move(stats)) {
  }

  using filter::prepared::execute;

  virtual doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord,
      const attribute_provider*) const override {
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
    using disjunction_t = irs::disjunction_iterator<doc_iterator::ptr>;

    const auto& features = irs::flags::empty_instance();

    disjunction_t::doc_iterators_t itrs;
    itrs.reserve(query_state.terms.size());
    for (auto& term_state : query_state.terms) {
      if (term_state == nullptr) {
        continue;
      }
      auto term = query_state.field->iterator();

      // use bytes_ref::blank here since we do not need just to "jump"
      // to cached state, and we are not interested in term value itself */
      if (!term->seek(bytes_ref::NIL, *term_state)) {
        continue;
      }

      // get postings
      auto docs = term->postings(features);
      assert(docs);

      // add iterator
      itrs.emplace_back(std::move(docs));
    }

    if (itrs.empty()) {
      return doc_iterator::empty();
    }

    return make_disjunction<disjunction_t>(std::move(itrs));
  }

  doc_iterator::ptr execute_ngram_similarity(
      const sub_reader& rdr,
      const ngram_segment_state_t& query_state,
      const order::prepared& ord) const {
    approximation::doc_iterators_t itrs;
    itrs.reserve(query_state.terms.size());
    auto features = ord.features() | by_ngram_similarity::features();
    for (auto& term_state : query_state.terms) {
      if (term_state == nullptr) {
        continue;
      }
      auto term = query_state.field->iterator();

      // use bytes_ref::blank here since we do not need just to "jump"
      // to cached state, and we are not interested in term value itself */
      if (!term->seek(bytes_ref::NIL, *term_state)) {
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

    return memory::make_managed<ngram_similarity_doc_iterator>(
        std::move(itrs), rdr, *query_state.field, boost(), stats_.c_str(),
        query_state.terms.size(), min_match_count_, ord);
  }

  size_t min_match_count_;
  states_t states_;
  bstring stats_;
};

// -----------------------------------------------------------------------------
// --SECTION--                                by_ngram_similarity implementation
// -----------------------------------------------------------------------------

/* static */ const flags& by_ngram_similarity::features() {
  static const flags req{ irs::type<frequency>::get(), irs::type<position>::get() };
  return req;
}

DEFINE_FACTORY_DEFAULT(by_ngram_similarity)

filter::prepared::ptr by_ngram_similarity::prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    const attribute_provider* /*ctx*/) const {
  const auto threshold = std::max(0.f, std::min(1.f, options().threshold));
  const auto& ngrams = options().ngrams;

  if (ngrams.empty() || field().empty()) {
    // empty field or terms or invalid threshold
    return filter::prepared::empty();
  }

  size_t min_match_count = std::max(
    static_cast<size_t>(std::ceil(static_cast<double>(ngrams.size()) * threshold)), (size_t)1);

  states_t query_states(rdr);

  // per segment terms states
  const auto terms_count = ngrams.size();
  std::vector<seek_term_iterator::cookie_ptr> term_states;
  term_states.reserve(terms_count);

  // prepare ngrams stats
  field_collectors field_stats(ord);
  term_collectors term_stats(ord, terms_count);

  const string_ref field_name = this->field();

  for (const auto& segment : rdr) {
    // get term dictionary for field
    const term_reader* field = segment.field(field_name);

    if (!field) {
      continue;
    }

    // check required features
    if (!features().is_subset_of(field->meta().features)) {
      continue;
    }

    field_stats.collect(segment, *field); // collect field statistics once per segment
    size_t term_idx = 0;
    size_t count_terms = 0;
    seek_term_iterator::ptr term = field->iterator();
    for (const auto& ngram : ngrams) {
      term_states.emplace_back();
      auto& state = term_states.back();
      if (term->seek(ngram)) {
        term->read(); // read term attributes
        term_stats.collect(segment, *field, term_idx, *term); // collect statistics
        state = term->cookie();
        ++count_terms;
      }

      ++term_idx;
    }

    if (count_terms < min_match_count) {
      // we have not found enough terms
      term_states.clear();
      continue;
    }

    auto& state = query_states.insert(segment);
    state.terms = std::move(term_states);
    state.field = field;

    term_states.reserve(terms_count);
  }

  bstring stats(ord.stats_size(), 0);
  auto* stats_buf = const_cast<byte_type*>(stats.data());

  for (size_t term_idx = 0; term_idx < terms_count; ++term_idx) {
    term_stats.finish(stats_buf, term_idx, field_stats, rdr);
  }

  return memory::make_managed<ngram_similarity_query>(
      min_match_count,
      std::move(query_states),
      std::move(stats),
      this->boost() * boost);
}

}
