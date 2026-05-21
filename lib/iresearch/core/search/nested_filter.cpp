////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "nested_filter.hpp"

#include <tuple>
#include <utility>
#include <variant>

#include "analysis/token_attributes.hpp"
#include "search/cost.hpp"
#include "search/prepared_state_visitor.hpp"
#include "search/prev_doc.hpp"
#include "search/score.hpp"
#include "search/scorer.hpp"
#include "utils/attribute_helper.hpp"
#include "utils/type_limits.hpp"

namespace irs {
template<template<typename> typename M>
struct HasScoreHelper<M<NoopAggregator>> : std::false_type {};
}  // namespace irs

namespace {

using namespace irs;

static_assert(std::variant_size_v<ByNestedOptions::MatchType> == 2);

const Scorers& GetOrder(const ByNestedOptions::MatchType& match,
                        const Scorers& ord) noexcept {
  // cppcheck-suppress returnTempReference
  return std::visit(
    irs::Visitor{[&](Match v) noexcept -> const Scorers& {
                   return kMatchNone == v ? Scorers::kUnordered : ord;
                 },
                 [&ord](const DocIteratorProvider&) noexcept -> const Scorers& {
                   return ord;
                 }},
    match);
}

bool IsValid(const ByNestedOptions::MatchType& match) noexcept {
  return std::visit(
    irs::Visitor{[](Match v) noexcept { return v.Min <= v.Max; },
                 [](const DocIteratorProvider& v) {
                   { return nullptr != v; }
                 }},
    match);
}

class ScorerWrapper : public doc_iterator {
 public:
  explicit ScorerWrapper(doc_iterator::ptr it, ScoreFunction&& score) noexcept
    : it_{std::move(it)} {
    IRS_ASSERT(it_);
    score_ = std::move(score);
  }

  doc_id_t value() const final { return it_->value(); }

  doc_id_t seek(doc_id_t target) final { return it_->seek(target); }

  bool next() final { return it_->next(); }

  attribute* get_mutable(irs::type_info::type_id id) final {
    if (irs::type<score>::id() == id) {
      return &score_;
    }

    return it_->get_mutable(id);
  }

 private:
  doc_iterator::ptr it_;
  score score_;
};

class NoneMatcher;

template<typename Matcher>
class ChildToParentJoin : public doc_iterator, private Matcher {
 public:
  ChildToParentJoin(doc_iterator::ptr&& parent, const prev_doc& prev_parent,
                    doc_iterator::ptr&& child, Matcher&& matcher) noexcept
    : Matcher{std::move(matcher)},
      parent_{std::move(parent)},
      child_{std::move(child)},
      prev_parent_{&prev_parent} {
    IRS_ASSERT(parent_);
    IRS_ASSERT(prev_parent);
    IRS_ASSERT(child_);

    std::get<attribute_ptr<document>>(attrs_) =
      irs::get_mutable<irs::document>(parent_.get());
    IRS_ASSERT(std::get<attribute_ptr<document>>(attrs_).ptr);

    child_doc_ = irs::get<irs::document>(*child_);

    std::get<attribute_ptr<cost>>(attrs_) =
      irs::get_mutable<cost>(child_.get());

    if constexpr (HasScore_v<Matcher>) {
      PrepareScore();
    }
  }

  doc_id_t value() const noexcept final {
    return std::get<attribute_ptr<document>>(attrs_).ptr->value;
  }

  attribute* get_mutable(irs::type_info::type_id id) final {
    return irs::get_mutable(attrs_, id);
  }

  doc_id_t seek(doc_id_t target) final {
    const auto& doc = *std::get<attribute_ptr<document>>(attrs_).ptr;

    if (IRS_UNLIKELY(target <= doc.value)) {
      return doc.value;
    }

    auto parent = parent_->seek(target);

    if (doc_limits::eof(parent)) {
      return doc_limits::eof();
    }

    return SeekInternal(parent);
  }

  bool next() final {
    if (IRS_LIKELY(parent_->next())) {
      return !doc_limits::eof(SeekInternal(value()));
    }

    return false;
  }

 private:
  friend Matcher;

  using Attributes =
    std::tuple<attribute_ptr<document>, attribute_ptr<cost>, score>;

  // Returns min possible first child given the current parent.
  doc_id_t FirstChildApprox() const {
    IRS_ASSERT(!doc_limits::eof((*prev_parent_)()));
    return (*prev_parent_)() + 1;
  }

  doc_id_t SeekInternal(doc_id_t parent) {
    IRS_ASSERT(!doc_limits::eof(parent));

    for (doc_id_t first_child = child_->seek(FirstChildApprox());
         (first_child = Matcher::Accept(first_child, parent));
         first_child = child_->seek(FirstChildApprox())) {
      parent = parent_->seek(first_child);

      if (doc_limits::eof(parent) ||
          (parent == first_child && !parent_->next())) {  // Skip parent docs
        return doc_limits::eof();
      }
    }

    return value();
  }

  void PrepareScore();

  doc_iterator::ptr parent_;
  doc_iterator::ptr child_;
  Attributes attrs_;
  const prev_doc* prev_parent_;
  const score* child_score_;
  const document* child_doc_;
};

template<typename Matcher>
void ChildToParentJoin<Matcher>::PrepareScore() {
  auto& score = std::get<irs::score>(attrs_);
  child_score_ = irs::get<irs::score>(*child_);
  child_doc_ = irs::get<document>(*child_);

  if (!std::is_same_v<Matcher, NoneMatcher> &&
      (child_doc_ == nullptr || child_score_ == nullptr ||
       child_score_->IsDefault())) {
    IRS_ASSERT(Matcher::size());
    score = ScoreFunction::Default(Matcher::size());
  } else {
    static_assert(HasScore_v<Matcher>);
    score = static_cast<Matcher&>(*this).PrepareScore();
  }
}

template<typename Merger>
struct ScoreBuffer;

template<>
struct ScoreBuffer<NoopAggregator> {
  explicit ScoreBuffer(const NoopAggregator&) {}
};

template<typename Merger, size_t Size>
struct ScoreBuffer<Aggregator<Merger, Size>> {
  static constexpr bool IsDynamic = Size == std::numeric_limits<size_t>::max();

  using BufferType =
    std::conditional_t<IsDynamic, bstring, std::array<score_t, Size>>;

  explicit ScoreBuffer(const Aggregator<Merger, Size>& merger) noexcept(
    !IsDynamic) {
    if constexpr (IsDynamic) {
      buf.resize(merger.byte_size());
    }
  }

  score_t* data() noexcept { return reinterpret_cast<score_t*>(buf.data()); }

  BufferType buf{};
};

class NoneMatcher : public NoopAggregator {
 public:
  using JoinType = ChildToParentJoin<NoneMatcher>;

  template<typename Merger>
  NoneMatcher(Merger&& merger, score_t none_boost) noexcept
    : boost_{none_boost}, size_{merger.size()} {}

  constexpr doc_id_t Accept(const doc_id_t child,
                            const doc_id_t parent) const noexcept {
    IRS_ASSERT(!doc_limits::eof(parent));
    return child < parent ? parent + 1 : 0;
  }

  ScoreFunction PrepareScore() const {
    return ScoreFunction::Constant(boost_, size_);
  }

 private:
  score_t boost_;
  uint32_t size_;
};

template<typename Merger>
class AnyMatcher : public Merger, private score_ctx {
 public:
  using JoinType = ChildToParentJoin<AnyMatcher<Merger>>;

  explicit AnyMatcher(Merger&& merger) noexcept : Merger{std::move(merger)} {}

  constexpr doc_id_t Accept(const doc_id_t child,
                            const doc_id_t parent) const noexcept {
    IRS_ASSERT(!doc_limits::eof(parent));
    return child < parent ? 0 : child;
  }

  ScoreFunction PrepareScore() {
    static_assert(HasScore_v<Merger>);

    return {*this, [](score_ctx* ctx, score_t* res) noexcept {
              IRS_ASSERT(ctx);
              IRS_ASSERT(res);
              auto& self = static_cast<JoinType&>(*ctx);
              auto& merger = static_cast<Merger&>(self);

              auto& child = *self.child_;
              const auto parent_doc = self.value();
              const auto* child_doc = self.child_doc_;
              const auto& child_score = *self.child_score_;

              child_score(res);
              while (child.next() && child_doc->value < parent_doc) {
                child_score(merger.temp());
                merger(res, merger.temp());
              }
            }};
  }
};

template<typename Merger>
class PredMatcher : public Merger,
                    private ScoreBuffer<Merger>,
                    private score_ctx {
 public:
  using BufferType = ScoreBuffer<Merger>;
  using JoinType = ChildToParentJoin<PredMatcher<Merger>>;

  PredMatcher(Merger&& merger, doc_iterator::ptr&& pred) noexcept
    : Merger{std::move(merger)},
      BufferType{static_cast<const Merger&>(*this)},
      pred_{std::move(pred)} {
    if (IRS_UNLIKELY(!pred_)) {
      pred_ = doc_iterator::empty();
    }

    pred_doc_ = irs::get<document>(*pred_);
    IRS_ASSERT(pred_doc_);
  }

  doc_id_t Accept(const doc_id_t first_child, const doc_id_t parent) {
    IRS_ASSERT(!doc_limits::eof(parent));

    if (first_child > parent) {
      return first_child;
    }

    auto& self = static_cast<JoinType&>(*this);

    if (first_child != pred_->seek(self.FirstChildApprox())) {
      return parent + 1;
    }

    auto& child = *self.child_;
    auto& merger = static_cast<Merger&>(*this);
    auto& buf = static_cast<BufferType&>(*this);

    const auto* child_doc = self.child_doc_;
    const auto& child_score = *self.child_score_;

    if constexpr (HasScore_v<Merger>) {
      child_score(buf.data());
    }

    while (pred_->next() && pred_doc_->value < parent) {
      if (!child.next() || pred_doc_->value != child_doc->value) {
        return parent + 1;
      }

      if constexpr (HasScore_v<Merger>) {
        child_score(merger.temp());
        merger(buf.data(), merger.temp());
      }
    }

    return 0;
  }

  ScoreFunction PrepareScore() noexcept {
    static_assert(HasScore_v<Merger>);

    return {*this, [](score_ctx* ctx, score_t* res) noexcept {
              IRS_ASSERT(ctx);
              IRS_ASSERT(res);
              auto& self = static_cast<PredMatcher&>(*ctx);
              auto& merger = static_cast<Merger&>(self);
              auto& buf = static_cast<ScoreBuffer<Merger>&>(self);
              std::memcpy(res, buf.data(), merger.byte_size());
            }};
  }

 private:
  doc_iterator::ptr pred_;
  const document* pred_doc_;
};

template<typename Merger>
class RangeMatcher : public Merger,
                     private ScoreBuffer<Merger>,
                     private score_ctx {
 public:
  using BufferType = ScoreBuffer<Merger>;
  using JoinType = ChildToParentJoin<RangeMatcher<Merger>>;

  RangeMatcher(Match match, Merger&& merger) noexcept
    : Merger{std::move(merger)},
      BufferType{static_cast<const Merger&>(*this)},
      match_{match} {
    // This case is handled by MinMatcher
    IRS_ASSERT(match_ != Match{0});
  }

  doc_id_t Accept(const doc_id_t first_child, const doc_id_t parent) {
    IRS_ASSERT(!doc_limits::eof(parent));

    const auto [min, max] = match_;
    IRS_ASSERT(min <= max);

    if (first_child > parent) {
      if (min == 0) {
        if constexpr (HasScore_v<Merger>) {
          // Reset score value as we are not able
          // to find any childs
          auto& merger = static_cast<Merger&>(*this);
          auto& buf = static_cast<BufferType&>(*this);
          std::memset(buf.data(), 0, merger.byte_size());
        }
        return 0;
      }

      return first_child;
    }

    auto& self = static_cast<JoinType&>(*this);
    auto& merger = static_cast<Merger&>(*this);
    auto& buf = static_cast<BufferType&>(*this);

    auto& child = *self.child_;
    const auto* child_doc = self.child_doc_;
    const auto& child_score = *self.child_score_;

    // Already matched the first child
    doc_id_t count = 1;

    if constexpr (HasScore_v<Merger>) {
      child_score(buf.data());
    }
    while (child.next() && child_doc->value < parent) {
      if (++count > max) {
        return parent + 1;
      }

      if constexpr (HasScore_v<Merger>) {
        child_score(merger.temp());
        merger(buf.data(), merger.temp());
      }
    }

    return min <= count ? 0 : parent + 1;
  }

  ScoreFunction PrepareScore() noexcept {
    static_assert(HasScore_v<Merger>);

    return {*this, [](score_ctx* ctx, score_t* res) noexcept {
              IRS_ASSERT(ctx);
              IRS_ASSERT(res);
              auto& self = static_cast<RangeMatcher&>(*ctx);
              auto& merger = static_cast<Merger&>(self);
              auto& buf = static_cast<ScoreBuffer<Merger>&>(self);
              std::memcpy(res, buf.data(), merger.byte_size());
            }};
  }

  const Match& range() const noexcept { return match_; }

 private:
  const Match match_;
};

template<typename Merger>
class MinMatcher : public Merger,
                   private ScoreBuffer<Merger>,
                   private score_ctx {
 public:
  using BufferType = ScoreBuffer<Merger>;
  using JoinType = ChildToParentJoin<MinMatcher<Merger>>;

  MinMatcher(doc_id_t min, Merger&& merger) noexcept
    : Merger{std::move(merger)},
      BufferType{static_cast<const Merger&>(*this)},
      min_{min} {}

  doc_id_t Accept(const doc_id_t first_child, const doc_id_t parent) {
    IRS_ASSERT(!doc_limits::eof(parent));

    if (0 == min_) {
      if constexpr (HasScore_v<Merger>) {
        // Reset score value as we might not be able
        // to find any childs
        auto& merger = static_cast<Merger&>(*this);
        auto& buf = static_cast<BufferType&>(*this);
        std::memset(buf.data(), 0, merger.byte_size());
      }
      return 0;
    }

    if (first_child > parent) {
      return first_child;
    }

    doc_id_t count = min_ - 1;

    if (!count) {
      return 0;
    }

    auto& self = static_cast<JoinType&>(*this);
    auto& merger = static_cast<Merger&>(*this);
    auto& buf = static_cast<BufferType&>(*this);

    auto& child = *self.child_;
    const auto* child_doc = self.child_doc_;
    const auto& child_score = *self.child_score_;

    if constexpr (HasScore_v<Merger>) {
      child_score(buf.data());
    }

    while (child.next() && child_doc->value < parent) {
      if (!--count) {
        return 0;
      }

      if constexpr (HasScore_v<Merger>) {
        child_score(merger.temp());
        merger(buf.data(), merger.temp());
      }
    }

    return count ? parent + 1 : 0;
  }

  ScoreFunction PrepareScore() noexcept {
    static_assert(HasScore_v<Merger>);

    return {*this, [](score_ctx* ctx, score_t* res) noexcept {
              IRS_ASSERT(ctx);
              IRS_ASSERT(res);
              auto& self = static_cast<JoinType&>(*ctx);
              auto& merger = static_cast<Merger&>(self);
              auto& buf = static_cast<BufferType&>(self);

              auto& child = *self.child_;
              const auto parent_doc = self.value();
              const auto* child_doc = self.child_doc_;
              const auto& child_score = *self.child_score_;

              while (child_doc->value < parent_doc) {
                child_score(merger.temp());
                merger(buf.data(), merger.temp());
                if (!child.next()) {
                  break;
                }
              }

              std::memcpy(res, buf.data(), merger.byte_size());
            }};
  }

  Match range() const noexcept { return Match{min_}; }

 private:
  const doc_id_t min_;
};

template<typename A, typename Visitor>
auto ResolveMatchType(const SubReader& segment,
                      const ByNestedOptions::MatchType& match,
                      score_t none_boost, A&& aggregator, Visitor&& visitor) {
  return std::visit(
    irs::Visitor{
      [&](Match v) {
        if (v == kMatchNone) {
          return visitor(NoneMatcher{std::forward<A>(aggregator), none_boost});
        } else if (v == kMatchAny) {
          return visitor(AnyMatcher<A>{std::forward<A>(aggregator)});
        } else if (v.IsMinMatch()) {
          IRS_ASSERT(doc_limits::eof(v.Max));
          return visitor(MinMatcher<A>{v.Min, std::forward<A>(aggregator)});
        } else {
          return visitor(RangeMatcher<A>{v, std::forward<A>(aggregator)});
        }
      },
      [&](const DocIteratorProvider& v) {
        return visitor(PredMatcher<A>{std::forward<A>(aggregator), v(segment)});
      }},
    match);
}

}  // namespace

namespace irs {

class ByNestedQuery : public filter::prepared {
 public:
  ByNestedQuery(DocIteratorProvider parent, prepared::ptr&& child,
                ScoreMergeType merge_type, ByNestedOptions::MatchType match,
                score_t none_boost) noexcept
    : parent_{std::move(parent)},
      child_{std::move(child)},
      match_{std::move(match)},
      merge_type_{merge_type},
      none_boost_{none_boost} {
    IRS_ASSERT(parent_);
    IRS_ASSERT(child_);
    IRS_ASSERT(IsValid(match_));
  }

  doc_iterator::ptr execute(const ExecutionContext& ctx) const final;

  void visit(const SubReader& segment, PreparedStateVisitor& visitor,
             score_t boost) const final {
    // TODO(MBkkt) maybe use none_boost for NoneMatcher?
    // boost *= this->boost();

    if (!visitor.Visit(*this, boost)) {
      return;
    }

    IRS_ASSERT(child_);
    child_->visit(segment, visitor, boost);
  }

  score_t boost() const noexcept final { return kNoBoost; }

 private:
  DocIteratorProvider parent_;
  prepared::ptr child_;
  ByNestedOptions::MatchType match_;
  ScoreMergeType merge_type_;
  score_t none_boost_;
};

doc_iterator::ptr ByNestedQuery::execute(const ExecutionContext& ctx) const {
  auto& rdr = ctx.segment;
  auto& ord = ctx.scorers;

  auto parent = parent_(rdr);

  if (IRS_UNLIKELY(!parent || doc_limits::eof(parent->value()))) {
    return doc_iterator::empty();
  }

  const auto* prev = irs::get<irs::prev_doc>(*parent);

  if (IRS_UNLIKELY(!prev || !*prev)) {
    return doc_iterator::empty();
  }

  auto child = child_->execute({.segment = rdr,
                                .scorers = GetOrder(match_, ord),
                                .ctx = ctx.ctx,
                                // TODO(MBkkt) wand for nested?
                                .wand = {}});

  if (IRS_UNLIKELY(!child)) {
    return doc_iterator::empty();
  }

  return ResoveMergeType(
    merge_type_, ord.buckets().size(),
    [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
      return ResolveMatchType(
        rdr, match_, none_boost_, std::forward<A>(aggregator),
        [&]<typename M>(M&& matcher) -> irs::doc_iterator::ptr {
          if constexpr (std::is_same_v<NoneMatcher, M>) {
            if (doc_limits::eof(child->value())) {  // Match all parents
              if constexpr (!std::is_same_v<NoopAggregator, A>) {
                auto func = ScoreFunction::Constant(
                  none_boost_, static_cast<uint32_t>(ord.buckets().size()));
                auto* score = irs::get_mutable<irs::score>(parent.get());
                if (IRS_UNLIKELY(!score)) {
                  return memory::make_managed<ScorerWrapper>(std::move(parent),
                                                             std::move(func));
                }
                *score = std::move(func);
              }
              return std::move(parent);
            }
          } else if constexpr (std::is_same_v<MinMatcher<A>, M> ||
                               std::is_same_v<RangeMatcher<A>, M>) {
            // Unordered case for the range [0..EOF] is the equivalent to
            // matching all parents
            if constexpr (std::is_same_v<NoopAggregator, A>) {
              if (Match{0} == matcher.range() &&
                  doc_limits::eof(child->value())) {
                return std::move(parent);
              }
            }
          } else {
            if (doc_limits::eof(child->value())) {
              return doc_iterator::empty();
            }
          }

          return memory::make_managed<ChildToParentJoin<M>>(
            std::move(parent), *prev, std::move(child), std::move(matcher));
        });
    });
}

filter::prepared::ptr ByNestedFilter::prepare(const PrepareContext& ctx) const {
  auto& [parent, child, match, merge_type] = options();

  if (!parent || !child || !IsValid(match)) {
    return prepared::empty();
  }

  const auto sub_boost = ctx.boost * boost();

  auto prepared_child = child->prepare({
    .index = ctx.index,
    .memory = ctx.memory,
    .scorers = GetOrder(match, ctx.scorers),
    .ctx = ctx.ctx,
    .boost = sub_boost,
  });

  if (!prepared_child) {
    return prepared::empty();
  }

  return memory::make_tracked<ByNestedQuery>(
    ctx.memory, parent, std::move(prepared_child), merge_type, match,
    /*none_boost*/ sub_boost);
}

}  // namespace irs
