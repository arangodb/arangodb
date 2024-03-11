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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AqlValue.h"
#include "Aql/AqlValueMaterializer.h"
#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Basics/Result.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchFilterFactory.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/KeyGenerator.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <utils/ngram_match_utils.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

using namespace arangodb;

namespace arangodb::aql {
namespace {
template<bool search_semantics>
AqlValue NgramSimilarityHelper(
    char const* AFN, ExpressionContext* ctx,
    aql::functions::VPackFunctionParametersView args) {
  TRI_ASSERT(ctx);
  if (args.size() < 3) {
    aql::functions::registerWarning(
        ctx, AFN,
        Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH,
               "Minimum 3 arguments are expected."});
    return AqlValue(AqlValueHintNull());
  }

  auto const& attribute =
      aql::functions::extractFunctionParameterValue(args, 0);
  if (ADB_UNLIKELY(!attribute.isString())) {
    aql::functions::registerInvalidArgumentWarning(ctx, AFN);
    return aql::AqlValue{aql::AqlValueHintNull{}};
  }
  auto const attributeValue =
      arangodb::iresearch::getStringRef(attribute.slice());

  auto const& target = aql::functions::extractFunctionParameterValue(args, 1);
  if (ADB_UNLIKELY(!target.isString())) {
    aql::functions::registerInvalidArgumentWarning(ctx, AFN);
    return aql::AqlValue{aql::AqlValueHintNull{}};
  }
  auto const targetValue = arangodb::iresearch::getStringRef(target.slice());

  auto const& ngramSize =
      aql::functions::extractFunctionParameterValue(args, 2);
  if (ADB_UNLIKELY(!ngramSize.isNumber())) {
    aql::functions::registerInvalidArgumentWarning(ctx, AFN);
    return aql::AqlValue{aql::AqlValueHintNull{}};
  }
  auto const ngramSizeValue = ngramSize.toInt64();

  if (ADB_UNLIKELY(ngramSizeValue < 1)) {
    aql::functions::registerWarning(
        ctx, AFN,
        Result{TRI_ERROR_BAD_PARAMETER,
               "Invalid ngram size. Should be 1 or greater"});
    return aql::AqlValue{aql::AqlValueHintNull{}};
  }

  auto utf32Attribute = basics::StringUtils::characterCodes(
      attributeValue.data(), attributeValue.size());
  auto utf32Target = basics::StringUtils::characterCodes(targetValue.data(),
                                                         targetValue.size());

  auto const similarity = irs::ngram_similarity<uint32_t, search_semantics>(
      utf32Target.data(), utf32Target.size(), utf32Attribute.data(),
      utf32Attribute.size(), ngramSizeValue);
  return AqlValue(AqlValueHintDouble(similarity));
}
}  // namespace

/// Executes NGRAM_SIMILARITY based on binary ngram similarity
AqlValue functions::NgramSimilarity(ExpressionContext* ctx, AstNode const&,
                                    VPackFunctionParametersView args) {
  static char const* AFN = "NGRAM_SIMILARITY";
  return NgramSimilarityHelper<true>(AFN, ctx, args);
}

/// Executes NGRAM_POSITIONAL_SIMILARITY based on positional ngram similarity
AqlValue functions::NgramPositionalSimilarity(
    ExpressionContext* ctx, AstNode const&, VPackFunctionParametersView args) {
  static char const* AFN = "NGRAM_POSITIONAL_SIMILARITY";
  return NgramSimilarityHelper<false>(AFN, ctx, args);
}

/// Executes NGRAM_MATCH based on binary ngram similarity
AqlValue functions::NgramMatch(ExpressionContext* ctx, AstNode const&,
                               VPackFunctionParametersView args) {
  TRI_ASSERT(ctx);
  static char const* AFN = "NGRAM_MATCH";

  auto const argc = args.size();

  if (argc <
      3) {  // for const evaluation we need analyzer to be set explicitly (we
            // can`t access filter context) but we can`t set analyzer as
            // mandatory in function AQL signature - this will break SEARCH
    registerWarning(ctx, AFN,
                    Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH,
                           "Minimum 3 arguments are expected."});
    return AqlValue(AqlValueHintNull());
  }

  auto const& attribute =
      aql::functions::extractFunctionParameterValue(args, 0);
  if (ADB_UNLIKELY(!attribute.isString())) {
    registerInvalidArgumentWarning(ctx, AFN);
    return aql::AqlValue{aql::AqlValueHintNull{}};
  }
  auto const attributeValue = iresearch::getStringRef(attribute.slice());

  auto const& target = aql::functions::extractFunctionParameterValue(args, 1);
  if (ADB_UNLIKELY(!target.isString())) {
    registerInvalidArgumentWarning(ctx, AFN);
    return aql::AqlValue{aql::AqlValueHintNull{}};
  }
  auto const targetValue = iresearch::getStringRef(target.slice());

  auto threshold = iresearch::FilterConstants::DefaultNgramMatchThreshold;
  size_t analyzerPosition = 2;
  if (argc > 3) {  // 4 args given. 3rd is threshold
    auto const& thresholdArg =
        aql::functions::extractFunctionParameterValue(args, 2);
    analyzerPosition = 3;
    if (ADB_UNLIKELY(!thresholdArg.isNumber())) {
      registerInvalidArgumentWarning(ctx, AFN);
      return aql::AqlValue{aql::AqlValueHintNull{}};
    }
    threshold = thresholdArg.toDouble();
    if (threshold <= 0 || threshold > 1) {
      aql::functions::registerWarning(
          ctx, AFN,
          Result{TRI_ERROR_BAD_PARAMETER, "Threshold must be between 0 and 1"});
    }
  }

  auto const& analyzerArg =
      aql::functions::extractFunctionParameterValue(args, analyzerPosition);
  if (ADB_UNLIKELY(!analyzerArg.isString())) {
    registerInvalidArgumentWarning(ctx, AFN);
    return aql::AqlValue{aql::AqlValueHintNull{}};
  }
  TRI_ASSERT(ctx != nullptr);
  auto const analyzerId = iresearch::getStringRef(analyzerArg.slice());
  auto& server = ctx->vocbase().server();
  if (!server.hasFeature<iresearch::IResearchAnalyzerFeature>()) {
    aql::functions::registerWarning(ctx, AFN, TRI_ERROR_INTERNAL);
    return aql::AqlValue{aql::AqlValueHintNull{}};
  }
  auto& analyzerFeature =
      server.getFeature<iresearch::IResearchAnalyzerFeature>();
  auto& trx = ctx->trx();
  auto analyzer = analyzerFeature.get(analyzerId, ctx->vocbase(),
                                      trx.state()->analyzersRevision(),
                                      trx.state()->operationOrigin());
  if (!analyzer) {
    aql::functions::registerWarning(
        ctx, AFN,
        Result{TRI_ERROR_BAD_PARAMETER, "Unable to load requested analyzer"});
    return aql::AqlValue{aql::AqlValueHintNull{}};
  }

  auto analyzerImpl = analyzer->get();
  TRI_ASSERT(analyzerImpl);
  irs::term_attribute const* token =
      irs::get<irs::term_attribute>(*analyzerImpl);
  TRI_ASSERT(token);

  std::vector<irs::bstring> attrNgrams;
  analyzerImpl->reset(attributeValue);
  while (analyzerImpl->next()) {
    attrNgrams.emplace_back(token->value.data(), token->value.size());
  }

  std::vector<irs::bstring> targetNgrams;
  analyzerImpl->reset(targetValue);
  while (analyzerImpl->next()) {
    targetNgrams.emplace_back(token->value.data(), token->value.size());
  }

  // consider only non empty ngrams sets. As no ngrams emitted - means no data
  // in index = no match
  if (!targetNgrams.empty() && !attrNgrams.empty()) {
    size_t thresholdMatches =
        (size_t)std::ceil((float_t)targetNgrams.size() * threshold);
    size_t d = 0;  // will store upper-left cell value for current cache row
    std::vector<size_t> cache(attrNgrams.size() + 1, 0);
    for (auto const& targetNgram : targetNgrams) {
      size_t s_ngram_idx = 1;
      for (; s_ngram_idx <= attrNgrams.size(); ++s_ngram_idx) {
        size_t curMatches =
            d + (size_t)(attrNgrams[s_ngram_idx - 1] == targetNgram);
        if (curMatches >= thresholdMatches) {
          return aql::AqlValue{aql::AqlValueHintBool{true}};
        }
        auto tmp = cache[s_ngram_idx];
        cache[s_ngram_idx] = std::max(
            std::max(cache[s_ngram_idx - 1], cache[s_ngram_idx]), curMatches);
        d = tmp;
      }
    }
  }
  return aql::AqlValue{aql::AqlValueHintBool{false}};
}

}  // namespace arangodb::aql
