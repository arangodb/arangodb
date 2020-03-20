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
#include "utils/map_utils.hpp"


NS_LOCAL

struct ngram_segment_state_t {
  const irs::term_reader* field{};
  std::vector<irs::seek_term_iterator::cookie_ptr> terms;
};

typedef irs::states_cache<ngram_segment_state_t> states_t;
typedef std::vector<irs::bstring> stats_t;

NS_END

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
///@class ngram_similarity_doc_iterator
///@brief adapter for min_match_disjunction with honor of terms orderings
//////////////////////////////////////////////////////////////////////////////
template<typename DocIterator>
class ngram_similarity_doc_iterator : public doc_iterator_base<doc_iterator>, score_ctx {
 public:
  struct position_t {
    position_t(position* p, document* d, score* s)
      : pos(p), doc(d), scr(s) {}
    position* pos;
    document* doc;
    score* scr;
  };

  using positions_t = std::vector<position_t>;
  using doc_iterators_t = typename min_match_disjunction<DocIterator>::doc_iterators_t;

  static positions_t extract_positions(const doc_iterators_t& itrs) {
    positions_t pos;
    pos.reserve(itrs.size());
    for (const auto& itr : itrs) {
      auto& attrs = itr->attributes();
      // get needed positions for iterators
      auto p = attrs.template get<position>().get();
      auto d = attrs.template get<document>().get();
      auto s = attrs.template get<score>().get();
      pos.emplace_back(p, d, s);
    }
    return pos;
  }

  ngram_similarity_doc_iterator(doc_iterators_t&& itrs,
    const states_t& states,
    const sub_reader& segment,
    const term_reader& field,
    boost_t boost,
    const byte_type* stats,
    size_t total_terms_count,
    size_t min_match_count = 1,
    const order::prepared& ord = order::prepared::unordered())
    : pos_(extract_positions(itrs)),
      min_match_count_(min_match_count),
      disjunction_(std::forward<doc_iterators_t>(itrs), min_match_count,
      order::prepared::unordered()),// we are not interested in disjunction`s scoring
      states_(states), total_terms_count_(total_terms_count) {
    scores_vals_.resize(pos_.size());

    attrs_.emplace(seq_freq_);
    doc_ = (attrs_.emplace<document>() = disjunction_.attributes().template get<document>()).get();
    attrs_.emplace<filter_boost>(filter_boost_);

    prepare_score(ord, ord.prepare_scorers(segment, field, stats, attrs_, boost));
    empty_order_ = ord.empty();
  }

  virtual bool next() override {
    bool next = false;
    while ((next = disjunction_.next()) && !check_serial_positions()) {}
    return next;
  }

  virtual doc_id_t value() const override {
    return doc_->value;
  }

  virtual doc_id_t seek(doc_id_t target) override {
    const auto doc = disjunction_.seek(target);

    if (doc_limits::eof(doc) || check_serial_positions()) {
      return doc;
    }

    next();
    return doc_->value;
  }

 private:
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

  bool check_serial_positions() {
    size_t potential = disjunction_.count_matched(); // how long max sequence could be in the best case
    search_buf_.clear();
    size_t longest_sequence_len = 0;
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
                auto current_sequence = found;
                // if we hit same position - set length to 0 to force checking candidates to the left
                size_t current_found_len = (found->first == current_pos ||
                                            found->second->scr == pos_iterator.scr) ? 0 : found->second->len + 1;
                auto initial_found = found;
                if (current_found_len > longest_sequence_len) {
                  longest_sequence_len = current_found_len;
                } else {
                  // maybe some previous candidates could produce better results.
                  // lets go leftward and check if there are any candidates which could became longer
                  // if we stick this ngram to them rather than the closest one found
                  for (++found; found != search_buf_.end(); ++found) {
                    if (found->second->scr != pos_iterator.scr &&
                        found->second->len + 1 > current_found_len) {
                      // we have better option. Replace this match!
                      current_sequence = found;
                      current_found_len = found->second->len + 1;
                      if (current_found_len > longest_sequence_len) {
                        longest_sequence_len = current_found_len;
                        break; // this match is the best - nothing to search further
                      }
                    }
                  }
                }
                if (current_found_len) {
                  auto new_candidate = std::make_shared<search_state>(current_sequence->second, current_pos, pos_iterator.scr);
                  auto res =  map_utils::try_emplace(search_buf_, current_pos, std::move(new_candidate));
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
          assert(i->second->len <= longest_sequence_len);
          if (i->second->len == longest_sequence_len) {
            bool delete_candidate = false;
            // only first longest sequence will contribute to frequency
            if (longest_sequence_.empty()) {
              longest_sequence_.push_back(i->second->scr);
              pos_sequence_.push_back(i->second->pos);
              auto cur_parent = i->second->parent;
              while (cur_parent) {
                longest_sequence_.push_back(cur_parent->scr);
                pos_sequence_.push_back(cur_parent->pos);
                cur_parent = cur_parent->parent;
              }
            } else {
              if (used_pos_.find(i->second->pos) != used_pos_.end() ||
                  i->second->scr != longest_sequence_[0]) {
                delete_candidate = true;
              } else {
                pos_sequence_.push_back(i->second->pos);
                auto cur_parent = i->second->parent;
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
      filter_boost_.value = (boost_t)longest_sequence_len / (boost_t)total_terms_count_;
    }
    return longest_sequence_len >= min_match_count_;
  }

  std::vector<const score*> longest_sequence_;
  positions_t pos_;
  frequency seq_freq_; // longest sequence frequency
  filter_boost filter_boost_;
  size_t min_match_count_;
  min_match_disjunction<DocIterator> disjunction_;
  mutable std::vector<const irs::byte_type*> scores_vals_;
  search_states_t search_buf_;
  const states_t& states_;
  std::vector<size_t> pos_sequence_;
  size_t total_terms_count_;
  const document* doc_;
  bool empty_order_;
  std::set<size_t> used_pos_; // longest sequence positions overlaping detector
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
        continue;
      }
      auto term = query_state.field->iterator();

      // use bytes_ref::blank here since we do not need just to "jump"
      // to cached state, and we are not interested in term value itself */
      if (!term->seek(bytes_ref::NIL, *term_state)) {
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

  doc_iterator::ptr execute_ngram_similarity(
    const sub_reader& rdr,
    const ngram_segment_state_t& query_state,
    const order::prepared& ord) const {
    min_match_disjunction<doc_iterator::ptr>::doc_iterators_t itrs;
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
    return memory::make_shared<ngram_similarity_doc_iterator<doc_iterator::ptr>>(
      std::move(itrs), states_, rdr, *query_state.field, boost(), stats_.c_str(),
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
  if (ngrams_.empty() || fld_.empty()) {
    // empty field or terms or invalid threshold
    return filter::prepared::empty();
  }

  size_t min_match_count = std::max(
    static_cast<size_t>(std::ceil(static_cast<double>(ngrams_.size()) * threshold_)), (size_t)1);

  states_t query_states(rdr.size());

  // per segment terms states
  ngram_segment_state_t term_states;
  term_states.terms.reserve(ngrams_.size());

  // prepare ngrams stats
  auto collectors = ord.fixed_prepare_collectors(ngrams_.size());

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
