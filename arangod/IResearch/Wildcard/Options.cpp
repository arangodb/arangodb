////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

#include "IResearch/Wildcard/Options.h"
#include "Aql/ExpressionContext.h"
#include "IResearch/Wildcard/Analyzer.h"
#include "Basics/DownCast.h"

namespace arangodb::iresearch::wildcard {

Options::Options(std::string_view pattern, AnalyzerPool const& analyzer,
                 aql::ExpressionContext* ctx) {
  auto& ngram = basics::downCast<wildcard::Analyzer>(*analyzer.get()).ngram();
  auto const* term = irs::get<irs::term_attribute>(ngram);
  auto makePartsImpl = [&](std::string_view v) {
    if (!ngram.reset(v)) {
      return false;
    }
    irs::by_phrase_options part;
    for (size_t i = 0; ngram.next(); ++i) {
      part.insert(irs::by_term_options{irs::bstring{term->value}}, i);
    }
    if (part.empty()) {
      return false;
    }
    parts.push_back(std::move(part));
    return true;
  };

  irs::bytes_view best;
  auto makeParts = [&](auto const* begin, auto const* end) {
    TRI_ASSERT(begin <= end);
    std::string_view v{begin, end};
    if (!makePartsImpl(v) && best.size() <= v.size()) {
      best = irs::ViewCast<irs::byte_type>(v);
    }
  };

  std::string patternStr;
  patternStr.resize(2 + pattern.size());
  auto* patternFirst = patternStr.data();
  auto* patternLast = patternFirst;
  *patternLast++ = '\xFF';
  auto* patternCurr = pattern.data();
  auto* patternEnd = patternCurr + pattern.size();
  bool needsMatcher = false;
  bool escaped = false;
  for (; patternCurr != patternEnd; ++patternCurr) {
    if (escaped) {
      escaped = false;
      *patternLast++ = *patternCurr;
    } else if (*patternCurr == '\\') {
      escaped = true;
    } else if (*patternCurr == '_' || *patternCurr == '%') {
      if (*patternCurr == '_' ||
          (patternCurr != pattern.data() && patternCurr != patternEnd - 1)) {
        needsMatcher = true;
      }
      makeParts(patternFirst, patternLast);
      patternFirst = patternLast;
    } else {
      *patternLast++ = *patternCurr;
    }
  }
  // We ignore escaped because post-filtering ignore it
  if (patternFirst != patternLast) {
    *patternLast++ = '\xFF';
    makeParts(patternFirst, patternLast);
  }
  if (parts.empty()) {
    TRI_ASSERT(!best.empty());
    token = best;
  } else {
    hasPos = analyzer.features().hasFeatures(irs::IndexFeatures::POS);
  }
  if (needsMatcher || !hasPos) {
    matcher = ctx->buildLikeMatcher(pattern, true);
  }
}

}  // namespace arangodb::iresearch::wildcard
