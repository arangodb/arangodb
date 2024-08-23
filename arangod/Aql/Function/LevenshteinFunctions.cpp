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

#include "Aql/AqlValue.h"
#include "Aql/AqlValueMaterializer.h"
#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "IResearch/IResearchPDP.h"
#include "IResearch/VelocyPackHelper.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Sink.h>
#include <velocypack/Slice.h>

#include <utils/levenshtein_utils.hpp>

using namespace arangodb;

namespace arangodb::aql {
/// Executes LEVENSHTEIN_MATCH
AqlValue functions::LevenshteinMatch(ExpressionContext* ctx,
                                     AstNode const& node,
                                     VPackFunctionParametersView args) {
  static char const* AFN = "LEVENSHTEIN_MATCH";

  auto const& maxDistance =
      aql::functions::extractFunctionParameterValue(args, 2);

  if (ADB_UNLIKELY(!maxDistance.isNumber())) {
    registerInvalidArgumentWarning(ctx, AFN);
    return aql::AqlValue{aql::AqlValueHintNull{}};
  }

  bool withTranspositionsValue = true;
  int64_t maxDistanceValue = maxDistance.toInt64();

  if (args.size() > 3) {
    auto const& withTranspositions =
        aql::functions::extractFunctionParameterValue(args, 3);

    if (ADB_UNLIKELY(!withTranspositions.isBoolean())) {
      registerInvalidArgumentWarning(ctx, AFN);
      return AqlValue{AqlValueHintNull{}};
    }

    withTranspositionsValue = withTranspositions.toBoolean();
  }

  if (maxDistanceValue < 0 ||
      (!withTranspositionsValue &&
       maxDistanceValue > iresearch::kMaxLevenshteinDistance)) {
    registerInvalidArgumentWarning(ctx, AFN);
    return AqlValue{AqlValueHintNull{}};
  }

  if (withTranspositionsValue &&
      maxDistanceValue > iresearch::kMaxDamerauLevenshteinDistance) {
    // fallback to LEVENSHTEIN_DISTANCE
    auto const dist = functions::LevenshteinDistance(ctx, node, args);
    TRI_ASSERT(dist.isNumber());

    return AqlValue{AqlValueHintBool{dist.toInt64() <= maxDistanceValue}};
  }

  size_t const unsignedMaxDistanceValue = static_cast<size_t>(maxDistanceValue);

  auto& description = iresearch::getParametricDescription(
      static_cast<irs::byte_type>(unsignedMaxDistanceValue),
      withTranspositionsValue);

  if (ADB_UNLIKELY(!description)) {
    registerInvalidArgumentWarning(ctx, AFN);
    return AqlValue{AqlValueHintNull{}};
  }

  auto const& lhs = aql::functions::extractFunctionParameterValue(args, 0);
  auto const lhsValue = lhs.isString() ? iresearch::getBytesRef(lhs.slice())
                                       : irs::kEmptyStringView<irs::byte_type>;
  auto const& rhs = aql::functions::extractFunctionParameterValue(args, 1);
  auto const rhsValue = rhs.isString() ? iresearch::getBytesRef(rhs.slice())
                                       : irs::kEmptyStringView<irs::byte_type>;

  size_t const dist = irs::edit_distance(description, lhsValue, rhsValue);

  return AqlValue(AqlValueHintBool(dist <= unsignedMaxDistanceValue));
}

}  // namespace arangodb::aql
