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

#include "analysis/token_attributes.hpp"
#include "search/cost.hpp"
#include "search/score.hpp"
#include "utils/frozen_attributes.hpp"
#include "utils/type_limits.hpp"

namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @class score_iterator_adapter
/// @brief adapter to use doc_iterator with conjunction and disjunction
////////////////////////////////////////////////////////////////////////////////
template<typename DocIterator>
struct score_iterator_adapter {
  typedef DocIterator doc_iterator_t;

  score_iterator_adapter() = default;
  score_iterator_adapter(doc_iterator_t&& it) noexcept
    : it(std::move(it)),
      doc(irs::get<irs::document>(*this->it)),
      score(&irs::score::get(*this->it)) {
    assert(doc);
  }

  score_iterator_adapter(score_iterator_adapter&&) = default;
  score_iterator_adapter& operator=(score_iterator_adapter&&) = default;

  typename doc_iterator_t::element_type* operator->() const noexcept {
    return it.get();
  }

  const attribute* get(irs::type_info::type_id type) const noexcept {
    return it->get(type);
  }

  attribute* get_mutable(irs::type_info::type_id type) noexcept {
    return it->get_mutable(type);
  }

  operator doc_iterator_t&&() noexcept {
    return std::move(it);
  }

  // access iterator value without virtual call
  doc_id_t value() const noexcept {
    return doc->value;
  }

  doc_iterator_t it;
  const irs::document* doc{};
  const irs::score* score{};
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
class conjunction : public doc_iterator, private score_ctx {
 public:
  using doc_iterator_t = score_iterator_adapter<DocIterator>;
  using doc_iterators_t = std::vector<doc_iterator_t>;
  using iterator = typename doc_iterators_t::const_iterator;

  static_assert(std::is_nothrow_move_constructible<doc_iterator_t>::value,
                "default move constructor expected");

  struct doc_iterators {
    // intentionally implicit
    doc_iterators(doc_iterators_t&& itrs) noexcept
      : itrs(std::move(itrs)) {
      assert(!this->itrs.empty());

      // sort subnodes in ascending order by their cost
      std::sort(this->itrs.begin(), this->itrs.end(),
        [](const doc_iterator_t& lhs, const doc_iterator_t& rhs) {
          return cost::extract(lhs, cost::MAX) < cost::extract(rhs, cost::MAX);
      });

      front = this->itrs.front().it.get();
      assert(front);
      front_doc = irs::get_mutable<document>(front);
      assert(front_doc);
    }

    doc_iterator* front;
    document* front_doc;
    doc_iterators_t itrs;
  }; // doc_iterators

  conjunction(
      doc_iterators&& itrs,
      const order::prepared& ord = order::prepared::unordered(),
      sort::MergeType merge_type = sort::MergeType::AGGREGATE)
    : itrs_(std::move(itrs.itrs)),
      front_(itrs.front),
      front_doc_(itrs.front_doc),
      merger_(ord.prepare_merger(merge_type)) {
    assert(!itrs_.empty());
    assert(front_);
    assert(front_doc_);
    std::get<attribute_ptr<document>>(attrs_) = itrs.front_doc;
    std::get<attribute_ptr<cost>>(attrs_)     = irs::get_mutable<cost>(itrs.front);

    prepare_score(ord);
  }

  iterator begin() const noexcept { return itrs_.begin(); }
  iterator end() const noexcept { return itrs_.end(); }

  // size of conjunction
  size_t size() const noexcept { return itrs_.size(); }

  virtual attribute* get_mutable(irs::type_info::type_id type) noexcept override final {
    return irs::get_mutable(attrs_, type);
  }

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
  using attributes = std::tuple<
    attribute_ptr<document>,
    attribute_ptr<cost>,
    score>;

  void prepare_score(const order::prepared& ord) {
    if (ord.empty()) {
      return;
    }

    auto& score = std::get<irs::score>(attrs_);

    score.realloc(ord);

    // copy scores into separate container
    // to avoid extra checks
    scores_.reserve(itrs_.size());
    for (auto& it : itrs_) {
      const auto* score = it.score;
      assert(score); // ensured by score_iterator_adapter
      if (!score->is_default()) {
        scores_.push_back(score);
      }
    }
    score_vals_.resize(scores_.size());

    // prepare score
    switch (scores_.size()) {
      case 0:
        assert(score.is_default());
        break;
      case 1:
        score.reset(*scores_.front());
        break;
      case 2:
        score.reset(this, [](score_ctx* ctx) -> const byte_type* {
          auto& self = *static_cast<conjunction*>(ctx);
          auto* score_buf = std::get<irs::score>(self.attrs_).data();
          self.score_vals_.front() = self.scores_.front()->evaluate();
          self.score_vals_.back() = self.scores_.back()->evaluate();
          self.merger_(score_buf, self.score_vals_.data(), 2);

          return score_buf;
        });
        break;
      case 3:
        score.reset(this, [](score_ctx* ctx) -> const byte_type* {
          auto& self = *static_cast<conjunction*>(ctx);
          auto* score_buf = std::get<irs::score>(self.attrs_).data();
          self.score_vals_.front() = self.scores_.front()->evaluate();
          self.score_vals_[1] = self.scores_[1]->evaluate();
          self.score_vals_.back() = self.scores_.back()->evaluate();
          self.merger_(score_buf, self.score_vals_.data(), 3);

          return score_buf;
        });
        break;
      default:
        score.reset(this, [](score_ctx* ctx) -> const byte_type* {
          auto& self = *static_cast<conjunction*>(ctx);
          auto* score_buf = std::get<irs::score>(self.attrs_).data();
          auto* score_val = self.score_vals_.data();
          for (auto* it_score : self.scores_) {
            *score_val++ = it_score->evaluate();
          }
          self.merger_(score_buf, self.score_vals_.data(), self.score_vals_.size());

          return score_buf;
        });
        break;
    }
  }

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

  attributes attrs_;
  doc_iterators_t itrs_;
  std::vector<const irs::score*> scores_; // valid sub-scores
  mutable std::vector<const irs::byte_type*> score_vals_;
  irs::doc_iterator* front_;
  const irs::document* front_doc_{};
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
  return memory::make_managed<Conjunction>(
    std::move(itrs),
    std::forward<Args>(args)...);
}

} // ROOT

#endif // IRESEARCH_CONJUNCTION_H
