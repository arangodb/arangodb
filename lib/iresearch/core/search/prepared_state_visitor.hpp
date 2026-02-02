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

#pragma once

#include "types.hpp"

namespace irs {

struct term_reader;

class BooleanQuery;
class ByNestedQuery;
class MultiTermQuery;
struct MultiTermState;
class TermQuery;
struct TermState;
class FixedPhraseQuery;
struct FixedPhraseState;
class VariadicPhraseQuery;
struct VariadicPhraseState;
class NGramSimilarityQuery;
struct NGramState;

struct PreparedStateVisitor {
  virtual ~PreparedStateVisitor() = default;

  virtual bool Visit(const BooleanQuery& q, score_t boost) = 0;
  virtual bool Visit(const ByNestedQuery& q, score_t boost) = 0;
  virtual bool Visit(const TermQuery& q, const TermState& state,
                     score_t boost) = 0;
  virtual bool Visit(const MultiTermQuery& q, const MultiTermState& state,
                     score_t boost) = 0;
  virtual bool Visit(const FixedPhraseQuery& q, const FixedPhraseState& state,
                     score_t boost) = 0;
  virtual bool Visit(const VariadicPhraseQuery& q,
                     const VariadicPhraseState& state, score_t boost) = 0;
  virtual bool Visit(const NGramSimilarityQuery& q, const NGramState& state,
                     score_t boost) = 0;
};

}  // namespace irs
