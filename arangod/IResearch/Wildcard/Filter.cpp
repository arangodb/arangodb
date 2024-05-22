////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

#include "IResearch/Wildcard/Filter.h"
#include "IResearch/IResearchFilterFactoryCommon.h"
#include "IResearch/IResearchFilterFactory.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"

#include "analysis/token_attributes.hpp"
#include "search/boolean_query.hpp"

namespace arangodb::iresearch::wildcard {

class Iterator : public irs::doc_iterator {
 public:
  Iterator(icu_64_64::RegexMatcher* matcher, doc_iterator::ptr&& approx,
           doc_iterator::ptr&& columnIt)
      : _approx{std::move(approx)}, _columnIt{std::move(columnIt)} {
    TRI_ASSERT(_approx);
    TRI_ASSERT(_columnIt);
    _doc = irs::get<irs::document>(*_approx);
    _stored = irs::get<irs::payload>(*_columnIt);
    TRI_ASSERT(matcher);
    // We need to create our own matcher to avoid data race on matcher
    auto status = U_ZERO_ERROR;
    matcher = matcher->pattern().matcher(status);
    if (!matcher) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL_AQL,
                                     "Cannot create matcher for this pattern");
    }
    TRI_ASSERT(status == U_ZERO_ERROR);
    _matcher = matcher;
  }

  irs::attribute* get_mutable(irs::type_info::type_id type) final {
    return _approx->get_mutable(type);
  }

  irs::doc_id_t value() const final { return _doc->value; }

  bool next() final {
    while (_approx->next()) {
      if (Check(value())) {
        return true;
      }
    }
    return false;
  }

  irs::doc_id_t seek(irs::doc_id_t target) final {
    target = _approx->seek(target);
    if (Check(target)) {
      return target;
    }
    next();
    return value();
  }

  ~Iterator() override { delete _matcher; }

 private:
  bool Check(irs::doc_id_t doc) const {
    // TODO(MBkkt) we can reduce count of matches if we will account
    // positions of subiterators in _approx (they should growth)
    if (_columnIt->seek(doc) != doc) {
      return false;  // it's assert case
    }

    auto* terms_begin = _stored->value.data();
    auto* terms_end = terms_begin + _stored->value.size();
    while (terms_begin != terms_end) {
      auto size = irs::vread<uint32_t>(terms_begin);
      ++terms_begin;  // skip begin marker

      auto term = icu_64_64::UnicodeString::fromUTF8(
          icu_64_64::StringPiece{reinterpret_cast<char const*>(terms_begin),
                                 static_cast<int32_t>(size)});

      _matcher->reset(term);

      if (auto status = U_ZERO_ERROR; _matcher->matches(status)) {
        return true;
      }

      terms_begin += size + 1;  // skip data and end marker
    }

    return false;
  }

  // TODO(MBkkt) we want to use re2 instead of icu, because it works with utf-8
  icu_64_64::RegexMatcher* _matcher;
  doc_iterator::ptr _approx;
  doc_iterator::ptr _columnIt;
  irs::document const* _doc{};
  irs::payload const* _stored{};
};

class Query : public irs::filter::prepared {
 public:
  Query(icu_64_64::RegexMatcher* matcher, std::string_view field,
        prepared::ptr&& approx)
      : _matcher{matcher}, _field{field}, _approx{std::move(approx)} {
    TRI_ASSERT(_approx);
    TRI_IF_FAILURE("wildcard::Filter::needsMatcher") {
      if (!_matcher) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_DEBUG, "no matcher");
      }
    }
    TRI_IF_FAILURE("wildcard::Filter::dissallowMatcher") {
      if (_matcher) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_DEBUG, "matcher setted");
      }
    }
  }

  irs::doc_iterator::ptr execute(const irs::ExecutionContext& ctx) const final {
    auto approx = _approx->execute(ctx);
    if (!_matcher || approx == irs::doc_iterator::empty()) {
      return approx;
    }
    auto* column = ctx.segment.column(_field);
    if (column == nullptr) {
      return irs::doc_iterator::empty();
    }
    auto columnIt = column->iterator(irs::ColumnHint::kNormal);
    return irs::memory::make_managed<Iterator>(_matcher, std::move(approx),
                                               std::move(columnIt));
  }

  void visit(const irs::SubReader&, irs::PreparedStateVisitor&,
             irs::score_t) const final {
    // NOOP
  }

  irs::score_t boost() const noexcept final { return irs::kNoBoost; }

 private:
  icu_64_64::RegexMatcher* _matcher{};
  std::string _field;
  prepared::ptr _approx;
};

irs::filter::prepared::ptr Filter::prepare(
    const irs::PrepareContext& ctx) const {
  auto boostCtx = ctx.Boost(boost());
  auto& parts = options().parts;
  auto size = parts.size();
  prepared::ptr p;
  // TODO(MBkkt) by_phrase don't need to make position check for any ngram
  // except first, last and all not intersected

  if (size == 0) {
    irs::bytes_view token = options().token;
    if (token.size() != 1 && token.back() == 0xFF) {
      TRI_IF_FAILURE("wildcard::Filter::needsPrefix") {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_DEBUG,
                                       "term instead of prefix");
      }
      p = irs::by_term::prepare(boostCtx, field(), token);
    } else {
      TRI_IF_FAILURE("wildcard::Filter::dissallowPrefix") {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_DEBUG,
            absl::StrCat("prefix disabled for: ", irs::ViewCast<char>(token)));
      }
      if (token.back() == 0xFF) {
        token = irs::kEmptyStringView<irs::byte_type>;
      }
      p = irs::by_prefix::prepare(boostCtx, field(), token,
                                  FilterConstants::DefaultScoringTermsLimit);
    }
  } else if (size == 1 && options().hasPos) {
    TRI_IF_FAILURE("wildcard::Filter::needsPrefix") {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_DEBUG,
                                     "phrase instead of prefix");
    }
    p = irs::by_phrase::Prepare(boostCtx, field(), std::move(parts[0]));
  }

  if (p) {
    if (p == prepared::empty()) {
      return p;
    }
    return irs::memory::make_tracked<Query>(ctx.memory, options().matcher,
                                            field(), std::move(p));
  }
  TRI_IF_FAILURE("wildcard::Filter::needsPrefix") {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_DEBUG,
                                   "phrases instead of prefix");
  }

  irs::AndQuery::queries_t queries{{ctx.memory}};
  if (options().hasPos) {
    queries.resize(size);
    for (size_t i = 0; auto& part : parts) {
      p = irs::by_phrase::Prepare(ctx, field(), part);
      if (p == prepared::empty()) {
        return p;
      }
      queries[i++] = std::move(p);
    }
  } else {
    for (auto& part : parts) {
      for (auto const& term : part) {
        auto p = irs::by_term::prepare(
            ctx, field(), std::get<irs::by_term_options>(term.second).term);
        if (p == prepared::empty()) {
          return p;
        }
        queries.push_back(std::move(p));
      }
    }
    size = queries.size();
  }
  auto conjunction = irs::memory::make_tracked<irs::AndQuery>(ctx.memory);
  conjunction->prepare(boostCtx, irs::ScoreMergeType::kSum, std::move(queries),
                       size);
  return irs::memory::make_tracked<Query>(ctx.memory, options().matcher,
                                          field(), std::move(conjunction));
}

}  // namespace arangodb::iresearch::wildcard
