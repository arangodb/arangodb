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

#ifndef IRESEARCH_CONJUNCTION_H
#define IRESEARCH_CONJUNCTION_H

#include "cost.hpp"
#include "score_doc_iterators.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/type_limits.hpp"

NS_ROOT

////////////////////////////////////////////////////////////////////////////////
/// @class score_iterator_adapter
/// @brief adapter to use doc_iterator with conjunction and disjunction
////////////////////////////////////////////////////////////////////////////////
template<typename DocIterator>
struct score_iterator_adapter {
  typedef DocIterator doc_iterator_t;

  score_iterator_adapter(doc_iterator_t&& it) noexcept
    : it(std::move(it)) {
    auto& attrs = this->it->attributes();
    score = &irs::score::extract(attrs);
    doc = attrs.template get<irs::document>().get();
    assert(doc);
  }

  score_iterator_adapter(const score_iterator_adapter&) = default;
  score_iterator_adapter& operator=(const score_iterator_adapter&) = default;

  score_iterator_adapter(score_iterator_adapter&& rhs) noexcept
    : it(std::move(rhs.it)),
      doc(rhs.doc),
      score(rhs.score) {
  }

  score_iterator_adapter& operator=(score_iterator_adapter&& rhs) noexcept {
    if (this != &rhs) {
      it = std::move(rhs.it);
      score = rhs.score;
      doc = rhs.doc;
    }
    return *this;
  }

  typename doc_iterator_t::element_type* operator->() const noexcept {
    return it.get();
  }

  operator doc_iterator_t&() noexcept {
    return it;
  }

  // access iterator value without virtual call
  doc_id_t value() const noexcept {
    return doc->value;
  }

  doc_iterator_t it;
  const irs::document* doc;
  const irs::score* score;
}; // score_iterator_adapter

////////////////////////////////////////////////////////////////////////////////
/// @class conjunction
///-----------------------------------------------------------------------------
/// c |  [0] <-- lead (the least cost iterator)
/// o |  [1]    |
/// s |  [2]    | tail (other iterators)
/// t |  ...    |
///   V  [n] <-- end
///-----------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////
template<typename DocIterator>
class conjunction : public doc_iterator_base<doc_iterator>, score_ctx {
 public:
  typedef score_iterator_adapter<DocIterator> doc_iterator_t;
  typedef std::vector<doc_iterator_t> doc_iterators_t;
  typedef typename doc_iterators_t::const_iterator iterator;

  conjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord = order::prepared::unordered())
    : itrs_(std::move(itrs)),
      merger_(ord.prepare_merger()) {
    assert(!itrs_.empty());

    // sort subnodes in ascending order by their cost
    std::sort(itrs_.begin(), itrs_.end(),
      [](const doc_iterator_t& lhs, const doc_iterator_t& rhs) {
        return cost::extract(lhs->attributes(), cost::MAX) < cost::extract(rhs->attributes(), cost::MAX);
    });

    // set front iterator
    front_ = itrs_.front().it.get();
    assert(front_);
    front_doc_ = (attrs_.emplace<irs::document>()
                    = front_->attributes().get<irs::document>()).get();
    assert(front_doc_);

    // estimate iterator (front's cost is already cached)
    estimate(cost::extract(front_->attributes(), cost::MAX));

    // copy scores into separate container
    // to avoid extra checks
    scores_.reserve(itrs_.size());
    scores_vals_.reserve(itrs_.size());
    for (auto& it : itrs_) {
      const auto* score = it.score;
      if (&irs::score::no_score() != score) {
        scores_.push_back(score);
        scores_vals_.push_back(score->c_str());
      }
    }
   
    // prepare score
    switch (scores_.size()) {
      case 0:
        prepare_score(ord, nullptr, [](const score_ctx*, byte_type*) { /*NOOP*/});
        break;
      case 1:
        prepare_score(ord, this, [](const score_ctx* ctx, byte_type* score) {
          auto& self = *static_cast<const conjunction*>(ctx);
          self.scores_[0]->evaluate();
          self.merger_(score, &self.scores_vals_[0], 1);
        });
        break;
      case 2:
        prepare_score(ord, this, [](const score_ctx* ctx, byte_type* score) {
          auto& self = *static_cast<const conjunction*>(ctx);
          self.scores_[0]->evaluate();
          self.scores_[1]->evaluate();
          self.merger_(score, &self.scores_vals_[0], 2);
        });
        break;
      case 3:
        prepare_score(ord, this, [](const score_ctx* ctx, byte_type* score) {
          auto& self = *static_cast<const conjunction*>(ctx);
          self.scores_[0]->evaluate();
          self.scores_[1]->evaluate();
          self.scores_[2]->evaluate();
          self.merger_(score, &self.scores_vals_[0], 3);
        });
        break;
      default:
        prepare_score(ord, this, [](const score_ctx* ctx, byte_type* score) {
          auto& self = *static_cast<const conjunction*>(ctx);
          for (auto* it_score : self.scores_) {
            it_score->evaluate();
          }
          self.merger_(score, &self.scores_vals_[0], self.scores_vals_.size());
        });
        break;
    }
  }

  iterator begin() const noexcept { return itrs_.begin(); }
  iterator end() const noexcept { return itrs_.end(); }

  // size of conjunction
  size_t size() const noexcept { return itrs_.size(); }

  virtual doc_id_t value() const override final {
    return front_doc_->value;
  }

  virtual bool next() override {
    if (!front_->next()) {
      return false;
    }

    return !doc_limits::eof(converge(front_doc_->value));
  }

  virtual doc_id_t seek(doc_id_t target) override {
    if (doc_limits::eof(target = front_->seek(target))) {
      return doc_limits::eof();
    }

    return converge(target);
  }

 private:
  // tries to converge front_ and other iterators to the specified target.
  // if it impossible tries to find first convergence place
  doc_id_t converge(doc_id_t target) {
    assert(!doc_limits::eof(target));

    for (auto rest = seek_rest(target); target != rest; rest = seek_rest(target)) {
      target = front_->seek(rest);
      if (doc_limits::eof(target)) {
        break;
      }
    }

    return target;
  }

  // seeks all iterators except the
  // first to the specified target
  doc_id_t seek_rest(doc_id_t target) {
    assert(!doc_limits::eof(target));

    for (auto it = itrs_.begin()+1, end = itrs_.end(); it != end; ++it) {
      const auto doc = (*it)->seek(target);

      if (target < doc) {
        return doc;
      }
    }

    return target;
  }

  doc_iterators_t itrs_;
  std::vector<const irs::score*> scores_; // valid sub-scores
  mutable std::vector<const irs::byte_type*> scores_vals_;
  const irs::document* front_doc_{};
  irs::doc_iterator* front_;
  order::prepared::merger merger_;
}; // conjunction

//////////////////////////////////////////////////////////////////////////////
/// @returns conjunction iterator created from the specified sub iterators 
//////////////////////////////////////////////////////////////////////////////
template<typename Conjunction, typename... Args>
doc_iterator::ptr make_conjunction(
    typename Conjunction::doc_iterators_t&& itrs,
    Args&&... args) {
  switch (itrs.size()) {
    case 0:
      // empty or unreachable search criteria
      return doc_iterator::empty();
    case 1:
      // single sub-query
      return std::move(itrs.front());
  }

  // conjunction
  return doc_iterator::make<Conjunction>(
      std::move(itrs), std::forward<Args>(args)...
  );
}

NS_END // ROOT

#endif // IRESEARCH_CONJUNCTION_H
