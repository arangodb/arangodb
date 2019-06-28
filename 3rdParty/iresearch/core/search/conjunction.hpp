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
struct score_iterator_adapter {
  score_iterator_adapter(doc_iterator::ptr&& it) NOEXCEPT
    : it(std::move(it)) {
    auto& attrs = this->it->attributes();
    score = &irs::score::extract(attrs);
    doc = attrs.get<irs::document>().get();
    assert(doc);
  }

  score_iterator_adapter(const score_iterator_adapter&) = default;
  score_iterator_adapter& operator=(const score_iterator_adapter&) = default;

  score_iterator_adapter(score_iterator_adapter&& rhs) NOEXCEPT
    : it(std::move(rhs.it)),
      doc(rhs.doc),
      score(rhs.score) {
  }

  score_iterator_adapter& operator=(score_iterator_adapter&& rhs) NOEXCEPT {
    if (this != &rhs) {
      it = std::move(rhs.it);
      score = rhs.score;
      doc = rhs.doc;
    }
    return *this;
  }

  doc_iterator* operator->() const NOEXCEPT {
    return it.get();
  }

  operator doc_iterator::ptr&() NOEXCEPT {
    return it;
  }

  // access iterator value without virtual call
  doc_id_t value() const NOEXCEPT {
    return doc->value;
  }

  doc_iterator::ptr it;
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
class conjunction : public doc_iterator_base {
 public:
  typedef score_iterator_adapter doc_iterator_t;
  typedef std::vector<doc_iterator_t> doc_iterators_t;
  typedef doc_iterators_t::const_iterator iterator;

  conjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord = order::prepared::unordered())
    : itrs_(std::move(itrs)),
      order_(&ord) {
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
    for (auto& it : itrs_) {
      const auto* score = it.score;
      if (&irs::score::no_score() != score) {
        scores_.push_back(score);
      }
    }

    if (scores_.empty()) {
      prepare_score(ord, [](byte_type*) { /*NOOP*/});
    } else {
      // prepare score
      prepare_score(ord, [this](byte_type* score) {
        order_->prepare_score(score);
        for (auto* it_score : scores_) {
          it_score->evaluate();
          order_->add(score, it_score->c_str());
        }
      });
    }
  }

  iterator begin() const NOEXCEPT { return itrs_.begin(); }
  iterator end() const NOEXCEPT { return itrs_.end(); }

  // size of conjunction
  size_t size() const NOEXCEPT { return itrs_.size(); }

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
      return target;
    }

    return converge(target);
  }

 private:
  // tries to converge front_ and other iterators to the specified target.
  // if it impossible tries to find first convergence place
  doc_id_t converge(doc_id_t target) {
    for (auto rest = seek_rest(target); target != rest;) {
      target = front_->seek(rest);
      rest = seek_rest(target);
    }

    return target;
  }

  // seeks all iterators except the
  // first to the specified target
  doc_id_t seek_rest(doc_id_t target) {
    if (doc_limits::eof(target)) {
      return target;
    }

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
  const irs::document* front_doc_{};
  irs::doc_iterator* front_;
  const irs::order::prepared* order_;
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
