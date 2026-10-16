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

#pragma once

#include "analysis/token_attributes.hpp"
#include "search/cost.hpp"
#include "search/score.hpp"
#include "utils/attribute_helper.hpp"
#include "utils/empty.hpp"
#include "utils/type_limits.hpp"

namespace irs {

// Adapter to use DocIterator with conjunction and disjunction.
template<typename DocIterator = doc_iterator::ptr>
struct ScoreAdapter {
  ScoreAdapter() noexcept = default;
  ScoreAdapter(DocIterator&& it) noexcept
    : it{std::move(it)},
      doc{irs::get<irs::document>(*this->it)},
      score{&irs::score::get(*this->it)} {
    IRS_ASSERT(doc);
  }

  ScoreAdapter(ScoreAdapter&&) noexcept = default;
  ScoreAdapter& operator=(ScoreAdapter&&) noexcept = default;

  auto* operator->() const noexcept { return it.get(); }

  const attribute* get(irs::type_info::type_id type) const noexcept {
    return it->get(type);
  }

  attribute* get_mutable(irs::type_info::type_id type) noexcept {
    return it->get_mutable(type);
  }

  operator DocIterator&&() && noexcept { return std::move(it); }

  explicit operator bool() const noexcept { return it != nullptr; }

  // access iterator value without virtual call
  doc_id_t value() const noexcept { return doc->value; }

  DocIterator it;
  const irs::document* doc{};
  const irs::score* score{};
};

using ScoreAdapters = std::vector<ScoreAdapter<>>;

// Helpers
template<typename T>
using EmptyWrapper = T;

struct SubScores {
  std::vector<irs::score*> scores;
  score_t sum_score = 0.f;
};

// Conjunction of N iterators
// -----------------------------------------------------------------------------
// c |  [0] <-- lead (the least cost iterator)
// o |  [1]    |
// s |  [2]    | tail (other iterators)
// t |  ...    |
//   V  [n] <-- end
// -----------------------------------------------------------------------------
// goto used instead of labeled cycles, with them we can achieve best perfomance
template<typename DocIterator, typename Merger>
struct ConjunctionBase : public doc_iterator,
                         protected Merger,
                         protected score_ctx {
 protected:
  explicit ConjunctionBase(Merger&& merger, std::vector<DocIterator>&& itrs,
                           std::vector<irs::score*>&& scorers)
    : Merger{std::move(merger)},
      itrs_{std::move(itrs)},
      scores_{std::move(scorers)} {
    IRS_ASSERT(std::is_sorted(
      itrs.begin(), itrs.end(), [](const auto& lhs, const auto& rhs) noexcept {
        return cost::extract(lhs, cost::kMax) < cost::extract(rhs, cost::kMax);
      }));
  }

  static void Score2(score_ctx* ctx, score_t* res) noexcept {
    auto& self = static_cast<ConjunctionBase&>(*ctx);
    auto& merger = static_cast<Merger&>(self);
    (*self.scores_.front())(res);
    (*self.scores_.back())(merger.temp());
    merger(res, merger.temp());
  }

  static void ScoreN(score_ctx* ctx, score_t* res) noexcept {
    auto& self = static_cast<ConjunctionBase&>(*ctx);
    auto& merger = static_cast<Merger&>(self);
    auto it = self.scores_.begin();
    auto end = self.scores_.end();
    (**it)(res);
    ++it;
    do {
      (**it)(merger.temp());
      merger(res, merger.temp());
      ++it;
    } while (it != end);
  }

  void PrepareScore(irs::score& score, auto score_2, auto score_n, auto min) {
    IRS_ASSERT(Merger::size());
    switch (scores_.size()) {
      case 0:
        score = ScoreFunction::Default(Merger::size());
        break;
      case 1:
        score = std::move(*scores_.front());
        break;
      case 2:
        score.Reset(*this, score_2, min);
        break;
      default:
        score.Reset(*this, score_n, min);
        break;
    }
  }

  auto begin() const noexcept { return itrs_.begin(); }
  auto end() const noexcept { return itrs_.end(); }
  size_t size() const noexcept { return itrs_.size(); }

  std::vector<DocIterator> itrs_;
  std::vector<score*> scores_;
};

template<typename DocIterator, typename Merger>
class Conjunction : public ConjunctionBase<DocIterator, Merger> {
  using Base = ConjunctionBase<DocIterator, Merger>;
  using Attributes =
    std::tuple<attribute_ptr<document>, attribute_ptr<cost>, score>;

 public:
  explicit Conjunction(Merger&& merger, std::vector<DocIterator>&& itrs,
                       std::vector<irs::score*>&& scores = {})
    : Base{std::move(merger), std::move(itrs), std::move(scores)},
      front_{this->itrs_.front().it.get()} {
    IRS_ASSERT(!this->itrs_.empty());
    IRS_ASSERT(front_);

    auto* front_doc = irs::get_mutable<document>(front_);
    front_doc_ = &front_doc->value;
    std::get<attribute_ptr<document>>(attrs_) = front_doc;

    std::get<attribute_ptr<cost>>(attrs_) = irs::get_mutable<cost>(front_);

    if constexpr (HasScore_v<Merger>) {
      auto& score = std::get<irs::score>(attrs_);
      this->PrepareScore(score, Base::Score2, Base::ScoreN,
                         ScoreFunction::DefaultMin);
    }
  }

  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return irs::get_mutable(attrs_, type);
  }

  doc_id_t value() const final { return *front_doc_; }

  bool next() override {
    if (IRS_UNLIKELY(!front_->next())) {
      return false;
    }

    return !doc_limits::eof(converge(*front_doc_));
  }

  doc_id_t seek(doc_id_t target) override {
    target = front_->seek(target);
    if (IRS_UNLIKELY(doc_limits::eof(target))) {
      return doc_limits::eof();
    }

    return converge(target);
  }

 private:
  // tries to converge front_ and other iterators to the specified target.
  // if it impossible tries to find first convergence place
  doc_id_t converge(doc_id_t target) {
    const auto begin = this->itrs_.begin() + 1;
    const auto end = this->itrs_.end();
  restart:
    IRS_ASSERT(!doc_limits::eof(target));
    for (auto it = begin; it != end; ++it) {
      const auto doc = (*it)->seek(target);
      if (target < doc) {
        target = front_->seek(doc);
        if (IRS_LIKELY(!doc_limits::eof(target))) {
          goto restart;
        }
        return target;
      }
    }
    return target;
  }

  Attributes attrs_;
  doc_iterator* front_;
  const doc_id_t* front_doc_{};
};

template<bool Root, typename DocIterator, typename Merger>
class BlockConjunction : public ConjunctionBase<DocIterator, Merger> {
  using Base = ConjunctionBase<DocIterator, Merger>;
  using Attributes = std::tuple<document, attribute_ptr<cost>, irs::score>;

 public:
  explicit BlockConjunction(Merger&& merger, std::vector<DocIterator>&& itrs,
                            SubScores&& scores, bool strict)
    : Base{std::move(merger), std::move(itrs), std::move(scores.scores)},
      sum_scores_{scores.sum_score},
      score_{static_cast<Merger&>(*this).size()} {
    IRS_ASSERT(this->itrs_.size() >= 2);
    IRS_ASSERT(!this->scores_.empty());
    std::sort(this->scores_.begin(), this->scores_.end(),
              [](const auto* lhs, const auto* rhs) {
                return lhs->max.tail > rhs->max.tail;
              });
    std::get<attribute_ptr<cost>>(attrs_) =
      irs::get_mutable<cost>(this->itrs_.front().it.get());
    auto& score = std::get<irs::score>(attrs_);
    score.max.leaf = score.max.tail = sum_scores_;
    auto min = strict ? MinStrictN : MinWeakN;
    if constexpr (Root) {
      auto score_root = [](score_ctx* ctx, score_t* res) noexcept {
        auto& self = static_cast<BlockConjunction&>(*ctx);
        std::memcpy(res, self.score_.temp(),
                    static_cast<Merger&>(self).byte_size());
      };
      this->PrepareScore(score, score_root, score_root, min);
    } else {
      this->PrepareScore(score, Base::Score2, Base::ScoreN, min);
    }
  }

  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return irs::get_mutable(attrs_, type);
  }

  IRS_FORCE_INLINE auto& score() { return std::get<irs::score>(attrs_).max; }

  doc_id_t value() const final { return std::get<document>(attrs_).value; }

  bool next() override { return !doc_limits::eof(seek(value() + 1)); }

  doc_id_t shallow_seek(doc_id_t target) final {
    target = ShallowSeekImpl(target);
    if (IRS_UNLIKELY(doc_limits::eof(target))) {
      return Seal();
    }
    return leafs_doc_;
  }

  doc_id_t seek(doc_id_t target) override {
    auto& doc = std::get<document>(attrs_).value;
    if (IRS_UNLIKELY(target <= doc)) {
      if constexpr (Root) {
        if (threshold_ < score_.temp()[0]) {
          return doc;
        }
        target = doc + !doc_limits::eof(doc);
      } else {
        return doc;
      }
    }
    auto& merger = static_cast<Merger&>(*this);
  align_leafs:
    target = ShallowSeekImpl(target);
  align_docs:
    if (IRS_UNLIKELY(doc_limits::eof(target))) {
      return Seal();
    }
    auto it = this->itrs_.begin();
    const auto seek_target = (*it)->seek(target);
    if (seek_target > leafs_doc_) {
      target = seek_target;
      goto align_leafs;
    }
    if (IRS_UNLIKELY(doc_limits::eof(seek_target))) {
      return Seal();
    }
    ++it;
    const auto end = this->itrs_.end();
    do {
      target = (*it)->seek(seek_target);
      if (target != seek_target) {
        if (target > leafs_doc_) {
          goto align_leafs;
        }
        goto align_docs;
      }
      ++it;
    } while (it != end);
    doc = seek_target;

    if constexpr (Root) {
      auto begin = this->scores_.begin();
      auto end = this->scores_.end();

      (**begin)(score_.temp());
      for (++begin; begin != end; ++begin) {
        (**begin)(merger.temp());
        merger(score_.temp(), merger.temp());
      }
      if (threshold_ < score_.temp()[0]) {
        return target;
      }
      ++target;
      if (target > leafs_doc_) {
        goto align_leafs;
      }
      goto align_docs;
    } else {
      return target;
    }
  }

 private:
  // TODO(MBkkt) Maybe optimize for 2?
  static void MinN(BlockConjunction& self, score_t arg) noexcept {
    for (auto* score : self.scores_) {
      const auto others = self.sum_scores_ - score->max.tail;
      if (arg <= others) {
        return;
      }
      score->Min(arg - others);
    }
  }

  static void MinStrictN(score_ctx* ctx, score_t arg) noexcept {
    auto& self = static_cast<BlockConjunction&>(*ctx);
    IRS_ASSERT(self.threshold_ <= arg);
    self.threshold_ = arg;
    MinN(self, arg);
  }

  static void MinWeakN(score_ctx* ctx, score_t arg) noexcept {
    auto& self = static_cast<BlockConjunction&>(*ctx);
    IRS_ASSERT(self.threshold_ <= arg);
    self.threshold_ = std::nextafter(arg, 0.f);
    MinN(self, arg);
  }

  IRS_NO_INLINE doc_id_t Seal() {
    leafs_doc_ = doc_limits::eof();
    std::get<document>(attrs_).value = doc_limits::eof();
    score().leaf = {};
    score().tail = {};
    return doc_limits::eof();
  }

  doc_id_t ShallowSeekImpl(doc_id_t target) {
    auto& doc = std::get<document>(attrs_).value;
    auto& merger = static_cast<Merger&>(*this);
    if (target <= leafs_doc_) {
    score_check:
      if (threshold_ < score().leaf) {
        return target;
      }
      target = leafs_doc_ + !doc_limits::eof(leafs_doc_);
    }
    IRS_ASSERT(doc <= leafs_doc_);
  eof_check:
    if (IRS_UNLIKELY(doc_limits::eof(target))) {
      return target;
    }

    auto max_leafs = doc_limits::eof();
    auto min_leafs = doc;
    score_t sum_leafs_score = 0.f;

    for (auto& it : this->itrs_) {
      auto max_leaf = it->shallow_seek(target);
      auto min_leaf = it.doc->value;
      IRS_ASSERT(min_leaf <= max_leaf);
      if (target < min_leaf) {
        target = min_leaf;
      }
      if (max_leafs < min_leaf) {
        goto eof_check;
      }
      if (min_leafs < min_leaf) {
        min_leafs = min_leaf;
      }
      if (max_leafs > max_leaf) {
        max_leafs = max_leaf;
      }
      IRS_ASSERT(min_leafs <= max_leafs);
      merger.Merge(sum_leafs_score, it.score->max.leaf);
    }

    leafs_doc_ = max_leafs;
    doc = min_leafs;
    score().leaf = sum_leafs_score;
    IRS_ASSERT(doc <= target);
    IRS_ASSERT(target <= leafs_doc_);
    goto score_check;
  }

  Attributes attrs_;
  score_t sum_scores_;
  doc_id_t leafs_doc_{doc_limits::invalid()};
  score_t threshold_{};
  typename Merger::Buffer score_;
};

// Returns conjunction iterator created from the specified sub iterators
template<template<typename> typename Wrapper = EmptyWrapper, typename Merger,
         typename DocIterator, typename... Args>
doc_iterator::ptr MakeConjunction(WandContext ctx, Merger&& merger,
                                  std::vector<DocIterator>&& itrs,
                                  Args&&... args) {
  if (const auto size = itrs.size(); 0 == size) {
    // empty or unreachable search criteria
    return doc_iterator::empty();
  } else if (1 == size) {
    // single sub-query
    return std::move(itrs.front());
  }

  // conjunction
  std::sort(
    itrs.begin(), itrs.end(), [](const auto& lhs, const auto& rhs) noexcept {
      return cost::extract(lhs, cost::kMax) < cost::extract(rhs, cost::kMax);
    });
  SubScores scores;
  if constexpr (HasScore_v<Merger>) {
    scores.scores.reserve(itrs.size());
    // TODO(MBkkt) Find better one
    static constexpr size_t kBlockConjunctionCostThreshold = 1;
    bool use_block = ctx.Enabled() && cost::extract(itrs.front(), cost::kMax) >
                                        kBlockConjunctionCostThreshold;
    for (auto& it : itrs) {
      // FIXME(gnusi): remove const cast
      auto* score = const_cast<irs::score*>(it.score);
      IRS_ASSERT(score);  // ensured by ScoreAdapter
      if (score->IsDefault()) {
        continue;
      }
      scores.scores.emplace_back(score);
      const auto tail = score->max.tail;
      use_block &= tail != std::numeric_limits<score_t>::max();
      if (use_block) {
        scores.sum_score += tail;
      }
    }
    use_block &= !scores.scores.empty();
    if (use_block) {
      return ResolveBool(ctx.root, [&]<bool Root>() -> irs::doc_iterator::ptr {
        return memory::make_managed<
          Wrapper<BlockConjunction<Root, DocIterator, Merger>>>(
          std::forward<Args>(args)..., std::forward<Merger>(merger),
          std::move(itrs), std::move(scores), ctx.strict);
      });
    }
    // TODO(MBkkt) We still could set min producer and root scoring
  }

  return memory::make_managed<Wrapper<Conjunction<DocIterator, Merger>>>(
    std::forward<Args>(args)..., std::forward<Merger>(merger), std::move(itrs),
    std::move(scores.scores));
}

}  // namespace irs
