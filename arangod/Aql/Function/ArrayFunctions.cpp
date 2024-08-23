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
#include "Aql/Range.h"
#include "Basics/Exceptions.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/debugging.h"
#include "Containers/FlatHashSet.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <list>
#include <set>
#include <unordered_map>
#include <vector>

using namespace arangodb;

namespace arangodb::aql {

namespace {

/// @brief internal recursive flatten helper
void flattenList(VPackSlice const& array, size_t maxDepth, size_t curDepth,
                 VPackBuilder& result) {
  TRI_ASSERT(result.isOpenArray());
  for (auto tmp : VPackArrayIterator(array)) {
    if (tmp.isArray() && curDepth < maxDepth) {
      flattenList(tmp, maxDepth, curDepth + 1, result);
    } else {
      // Copy the content of tmp into the result
      result.add(tmp);
    }
  }
}

/// @brief Checks if the given list contains the element
bool listContainsElement(VPackOptions const* vopts, AqlValue const& list,
                         AqlValue const& testee, size_t& index) {
  TRI_ASSERT(list.isArray());
  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(list);

  AqlValueMaterializer testeeMaterializer(vopts);
  VPackSlice testeeSlice = testeeMaterializer.slice(testee);

  VPackArrayIterator it(slice);
  while (it.valid()) {
    if (basics::VelocyPackHelper::equal(testeeSlice, it.value(), false,
                                        vopts)) {
      index = static_cast<size_t>(it.index());
      return true;
    }
    it.next();
  }
  return false;
}

/// @brief Checks if the given list contains the element
/// DEPRECATED
bool listContainsElement(VPackOptions const* options, VPackSlice const& list,
                         VPackSlice const& testee, size_t& index) {
  TRI_ASSERT(list.isArray());
  for (size_t i = 0; i < static_cast<size_t>(list.length()); ++i) {
    if (basics::VelocyPackHelper::equal(testee, list.at(i), false, options)) {
      index = i;
      return true;
    }
  }
  return false;
}

bool listContainsElement(VPackOptions const* options, VPackSlice const& list,
                         VPackSlice const& testee) {
  size_t unused;
  return listContainsElement(options, list, testee, unused);
}

}  // namespace

/// @brief function PUSH
AqlValue functions::Push(ExpressionContext* expressionContext, AstNode const&,
                         VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "PUSH";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& list =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue const& toPush =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  AqlValueMaterializer toPushMaterializer(vopts);
  VPackSlice p = toPushMaterializer.slice(toPush);

  if (list.isNull(true)) {
    transaction::BuilderLeaser builder(trx);
    builder->openArray();
    builder->add(p);
    builder->close();
    return AqlValue(builder->slice(), builder->size());
  }

  if (!list.isArray()) {
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  AqlValueMaterializer materializer(vopts);
  VPackSlice l = materializer.slice(list);

  for (VPackSlice it : VPackArrayIterator(l)) {
    builder->add(it);
  }
  if (parameters.size() == 3) {
    auto options = trx->transactionContextPtr()->getVPackOptions();
    AqlValue const& unique =
        aql::functions::extractFunctionParameterValue(parameters, 2);
    if (!unique.toBoolean() || !listContainsElement(options, l, p)) {
      builder->add(p);
    }
  } else {
    builder->add(p);
  }
  builder->close();
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function POP
AqlValue functions::Pop(ExpressionContext* expressionContext, AstNode const&,
                        VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "POP";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& list =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (list.isNull(true)) {
    return AqlValue(AqlValueHintNull());
  }

  if (!list.isArray()) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(list);

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  auto iterator = VPackArrayIterator(slice);
  while (iterator.valid() && iterator.index() + 1 != iterator.size()) {
    builder->add(iterator.value());
    iterator.next();
  }
  builder->close();
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function APPEND
AqlValue functions::Append(ExpressionContext* expressionContext, AstNode const&,
                           VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "APPEND";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& list =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue const& toAppend =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  if (toAppend.isNull(true)) {
    return list.clone();
  }

  AqlValueMaterializer toAppendMaterializer(vopts);
  VPackSlice t = toAppendMaterializer.slice(toAppend);

  if (t.isArray() && t.length() == 0) {
    return list.clone();
  }

  bool unique = false;
  if (parameters.size() == 3) {
    AqlValue const& a =
        aql::functions::extractFunctionParameterValue(parameters, 2);
    unique = a.toBoolean();
  }

  AqlValueMaterializer materializer(vopts);
  VPackSlice l = materializer.slice(list);

  if (l.isNull()) {
    return toAppend.clone();
  }

  if (!l.isArray()) {
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  auto options = trx->transactionContextPtr()->getVPackOptions();
  containers::FlatHashSet<VPackSlice, basics::VelocyPackHelper::VPackHash,
                          basics::VelocyPackHelper::VPackEqual>
      added(11, basics::VelocyPackHelper::VPackHash(),
            basics::VelocyPackHelper::VPackEqual(options));

  transaction::BuilderLeaser builder(trx);
  builder->openArray();

  for (VPackSlice it : VPackArrayIterator(l)) {
    if (!unique || added.insert(it).second) {
      builder->add(it);
    }
  }

  AqlValueMaterializer materializer2(vopts);
  VPackSlice slice = materializer2.slice(toAppend);

  if (!slice.isArray()) {
    if (!unique || added.find(slice) == added.end()) {
      builder->add(slice);
    }
  } else {
    for (VPackSlice it : VPackArrayIterator(slice)) {
      if (!unique || added.insert(it).second) {
        builder->add(it);
      }
    }
  }
  builder->close();
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function UNSHIFT
AqlValue functions::Unshift(ExpressionContext* expressionContext,
                            AstNode const&,
                            VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "UNSHIFT";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& list =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!list.isNull(true) && !list.isArray()) {
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  AqlValue const& toAppend =
      aql::functions::extractFunctionParameterValue(parameters, 1);
  bool unique = false;
  if (parameters.size() == 3) {
    AqlValue const& a =
        aql::functions::extractFunctionParameterValue(parameters, 2);
    unique = a.toBoolean();
  }

  size_t unused;
  if (unique && list.isArray() &&
      listContainsElement(vopts, list, toAppend, unused)) {
    // Short circuit, nothing to do return list
    return list.clone();
  }

  AqlValueMaterializer materializer(vopts);
  VPackSlice a = materializer.slice(toAppend);

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  builder->add(a);

  if (list.isArray()) {
    AqlValueMaterializer listMaterializer(vopts);
    VPackSlice v = listMaterializer.slice(list);
    for (VPackSlice it : VPackArrayIterator(v)) {
      builder->add(it);
    }
  }
  builder->close();
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function SHIFT
AqlValue functions::Shift(ExpressionContext* expressionContext, AstNode const&,
                          VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "SHIFT";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& list =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  if (list.isNull(true)) {
    return AqlValue(AqlValueHintNull());
  }

  if (!list.isArray()) {
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();

  if (list.length() > 0) {
    AqlValueMaterializer materializer(vopts);
    VPackSlice l = materializer.slice(list);

    auto iterator = VPackArrayIterator(l);
    // This jumps over the first element
    iterator.next();
    while (iterator.valid()) {
      builder->add(iterator.value());
      iterator.next();
    }
  }
  builder->close();

  return AqlValue(builder->slice(), builder->size());
}

/// @brief function REMOVE_VALUE
AqlValue functions::RemoveValue(ExpressionContext* expressionContext,
                                AstNode const&,
                                VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "REMOVE_VALUE";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& list =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (list.isNull(true)) {
    return AqlValue(AqlValueHintEmptyArray());
  }

  if (!list.isArray()) {
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  auto options = trx->transactionContextPtr()->getVPackOptions();

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  bool useLimit = false;
  int64_t limit = list.length();

  if (parameters.size() == 3) {
    AqlValue const& limitValue =
        aql::functions::extractFunctionParameterValue(parameters, 2);
    if (!limitValue.isNull(true)) {
      limit = limitValue.toInt64();
      useLimit = true;
    }
  }

  AqlValue const& toRemove =
      aql::functions::extractFunctionParameterValue(parameters, 1);
  AqlValueMaterializer toRemoveMaterializer(vopts);
  VPackSlice r = toRemoveMaterializer.slice(toRemove);

  AqlValueMaterializer materializer(vopts);
  VPackSlice v = materializer.slice(list);

  for (VPackSlice it : VPackArrayIterator(v)) {
    if (useLimit && limit == 0) {
      // Just copy
      builder->add(it);
      continue;
    }
    if (basics::VelocyPackHelper::equal(r, it, false, options)) {
      --limit;
      continue;
    }
    builder->add(it);
  }
  builder->close();
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function REMOVE_VALUES
AqlValue functions::RemoveValues(ExpressionContext* expressionContext,
                                 AstNode const&,
                                 VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "REMOVE_VALUES";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& list =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue const& values =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  if (values.isNull(true)) {
    return list.clone();
  }

  if (list.isNull(true)) {
    return AqlValue(AqlValueHintEmptyArray());
  }

  if (!list.isArray() || !values.isArray()) {
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer valuesMaterializer(vopts);
  VPackSlice v = valuesMaterializer.slice(values);

  AqlValueMaterializer listMaterializer(vopts);
  VPackSlice l = listMaterializer.slice(list);

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (VPackSlice it : VPackArrayIterator(l)) {
    if (!listContainsElement(vopts, v, it)) {
      builder->add(it);
    }
  }
  builder->close();
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function REMOVE_NTH
AqlValue functions::RemoveNth(ExpressionContext* expressionContext,
                              AstNode const&,
                              VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "REMOVE_NTH";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& list =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (list.isNull(true)) {
    return AqlValue(AqlValueHintEmptyArray());
  }

  if (!list.isArray()) {
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  double const count = static_cast<double>(list.length());
  AqlValue const& position =
      aql::functions::extractFunctionParameterValue(parameters, 1);
  double p = position.toDouble();
  if (p >= count || p < -count) {
    // out of bounds
    return list.clone();
  }

  if (p < 0) {
    p += count;
  }

  AqlValueMaterializer materializer(vopts);
  VPackSlice v = materializer.slice(list);

  transaction::BuilderLeaser builder(trx);
  size_t target = static_cast<size_t>(p);
  size_t cur = 0;
  builder->openArray();
  for (VPackSlice it : VPackArrayIterator(v)) {
    if (cur != target) {
      builder->add(it);
    }
    cur++;
  }
  builder->close();
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function ReplaceNth
AqlValue functions::ReplaceNth(ExpressionContext* expressionContext,
                               AstNode const&,
                               VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "REPLACE_NTH";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& baseArray =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue const& offset =
      aql::functions::extractFunctionParameterValue(parameters, 1);
  AqlValue const& newValue =
      aql::functions::extractFunctionParameterValue(parameters, 2);
  AqlValue const& paddValue =
      aql::functions::extractFunctionParameterValue(parameters, 3);

  bool havePadValue = parameters.size() == 4;

  if (!baseArray.isArray()) {
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  if (offset.isNull(true)) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, AFN);
  }
  auto length = baseArray.length();
  uint64_t replaceOffset;
  int64_t posParam = offset.toInt64();
  if (posParam >= 0) {
    replaceOffset = static_cast<uint64_t>(posParam);
  } else {
    replaceOffset = (static_cast<int64_t>(length) + posParam < 0)
                        ? 0
                        : static_cast<uint64_t>(length + posParam);
  }

  if (length < replaceOffset && !havePadValue) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, AFN);
  }

  AqlValueMaterializer materializer1(vopts);
  VPackSlice arraySlice = materializer1.slice(baseArray);
  AqlValueMaterializer materializer2(vopts);
  VPackSlice replaceValue = materializer2.slice(newValue);

  transaction::BuilderLeaser builder(trx);
  builder->openArray();

  VPackArrayIterator it(arraySlice);
  while (it.valid()) {
    if (it.index() != replaceOffset) {
      builder->add(it.value());
    } else {
      builder->add(replaceValue);
    }
    it.next();
  }

  uint64_t pos = length;
  if (replaceOffset >= length) {
    AqlValueMaterializer materializer(vopts);
    VPackSlice paddVpValue = materializer.slice(paddValue);
    while (pos < replaceOffset) {
      builder->add(paddVpValue);
      ++pos;
    }
    builder->add(replaceValue);
  }
  builder->close();
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function POSITION
AqlValue functions::Position(ExpressionContext* expressionContext,
                             AstNode const&,
                             VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "POSITION";

  AqlValue const& list =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!list.isArray()) {
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  bool returnIndex = false;
  if (parameters.size() == 3) {
    AqlValue const& a =
        aql::functions::extractFunctionParameterValue(parameters, 2);
    returnIndex = a.toBoolean();
  }

  if (list.length() > 0) {
    AqlValue const& searchValue =
        aql::functions::extractFunctionParameterValue(parameters, 1);

    transaction::Methods* trx = &expressionContext->trx();
    auto* vopts = &trx->vpackOptions();

    size_t index;
    if (listContainsElement(vopts, list, searchValue, index)) {
      if (!returnIndex) {
        // return true
        return AqlValue(AqlValueHintBool(true));
      }
      // return position
      return AqlValue(AqlValueHintUInt(index));
    }
  }

  // not found
  if (!returnIndex) {
    // return false
    return AqlValue(AqlValueHintBool(false));
  }

  // return -1
  return AqlValue(AqlValueHintInt(-1));
}

AqlValue functions::Interleave(aql::ExpressionContext* expressionContext,
                               AstNode const&,
                               VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "INTERLEAVE";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();

  struct ArrayIteratorPair {
    VPackArrayIterator current;
    VPackArrayIterator end;
  };

  std::list<velocypack::ArrayIterator> iters;
  std::vector<AqlValueMaterializer> materializers;
  materializers.reserve(parameters.size());

  for (AqlValue const& parameter : parameters) {
    auto& materializer = materializers.emplace_back(vopts);
    VPackSlice slice = materializer.slice(parameter);

    if (!slice.isArray()) {
      // not an array
      registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
      return AqlValue(AqlValueHintNull());
    } else if (slice.isEmptyArray()) {
      continue;  // skip empty array here
    }

    iters.emplace_back(velocypack::ArrayIterator(slice));
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();

  while (!iters.empty()) {  // in this loop we only deal with nonempty arrays
    for (auto i = iters.begin(); i != iters.end();) {
      builder->add(i->value());  // thus this will always be valid on
                                 // the first iteration
      i->next();
      if (*i == std::default_sentinel) {
        i = iters.erase(i);
      } else {
        i++;
      }
    }
  }

  builder->close();
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function RANGE
AqlValue functions::Range(ExpressionContext* expressionContext, AstNode const&,
                          VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "RANGE";

  AqlValue const& left =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue const& right =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  double from = left.toDouble();
  double to = right.toDouble();

  if (parameters.size() < 3) {
    return AqlValue(left.toInt64(), right.toInt64());
  }

  AqlValue const& stepValue =
      aql::functions::extractFunctionParameterValue(parameters, 2);
  if (stepValue.isNull(true)) {
    // no step specified. return a real range object
    return AqlValue(left.toInt64(), right.toInt64());
  }

  double step = stepValue.toDouble();

  if (step == 0.0 || (from < to && step < 0.0) || (from > to && step > 0.0)) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  transaction::Methods* trx = &expressionContext->trx();

  transaction::BuilderLeaser builder(trx);
  builder->openArray(true);
  if (step < 0.0 && to <= from) {
    TRI_ASSERT(step != 0.0);
    Range::throwIfTooBigForMaterialization(
        static_cast<uint64_t>((from - to) / -step));
    for (; from >= to; from += step) {
      builder->add(VPackValue(from));
    }
  } else {
    TRI_ASSERT(step != 0.0);
    Range::throwIfTooBigForMaterialization(
        static_cast<uint64_t>((to - from) / step));
    for (; from <= to; from += step) {
      builder->add(VPackValue(from));
    }
  }
  builder->close();
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function COUNT_DISTINCT
AqlValue functions::CountDistinct(ExpressionContext* expressionContext,
                                  AstNode const&,
                                  VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "COUNT_DISTINCT";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(value);

  auto options = trx->transactionContextPtr()->getVPackOptions();
  containers::FlatHashSet<VPackSlice, basics::VelocyPackHelper::VPackHash,
                          basics::VelocyPackHelper::VPackEqual>
      values(512, basics::VelocyPackHelper::VPackHash(),
             basics::VelocyPackHelper::VPackEqual(options));

  for (VPackSlice s : VPackArrayIterator(slice)) {
    if (!s.isNone()) {
      values.emplace(s.resolveExternal());
    }
  }

  return AqlValue(AqlValueHintUInt(values.size()));
}

/// @brief function UNIQUE
AqlValue functions::Unique(ExpressionContext* expressionContext, AstNode const&,
                           VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "UNIQUE";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(value);

  auto options = trx->transactionContextPtr()->getVPackOptions();
  containers::FlatHashSet<VPackSlice, basics::VelocyPackHelper::VPackHash,
                          basics::VelocyPackHelper::VPackEqual>
      values(512, basics::VelocyPackHelper::VPackHash(),
             basics::VelocyPackHelper::VPackEqual(options));

  transaction::BuilderLeaser builder(trx);
  builder->openArray();

  for (VPackSlice s : VPackArrayIterator(slice)) {
    if (s.isNone()) {
      continue;
    }

    s = s.resolveExternal();

    if (values.emplace(s).second) {
      builder->add(s);
    }
  }

  builder->close();
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function SORTED_UNIQUE
AqlValue functions::SortedUnique(ExpressionContext* expressionContext,
                                 AstNode const&,
                                 VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "SORTED_UNIQUE";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(value);

  basics::VelocyPackHelper::VPackLess<true> less(
      trx->transactionContext()->getVPackOptions(), &slice, &slice);
  std::set<VPackSlice, basics::VelocyPackHelper::VPackLess<true>> values(less);
  for (VPackSlice it : VPackArrayIterator(slice)) {
    if (!it.isNone()) {
      values.insert(it);
    }
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (auto const& it : values) {
    builder->add(it);
  }
  builder->close();
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function SORTED
AqlValue functions::Sorted(ExpressionContext* expressionContext, AstNode const&,
                           VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "SORTED";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(value);

  basics::VelocyPackHelper::VPackLess<true> less(
      trx->transactionContext()->getVPackOptions(), &slice, &slice);
  std::map<VPackSlice, size_t, basics::VelocyPackHelper::VPackLess<true>>
      values(less);
  for (VPackSlice it : VPackArrayIterator(slice)) {
    if (!it.isNone()) {
      auto [itr, emplaced] = values.try_emplace(it, 1);
      if (!emplaced) {
        ++itr->second;
      }
    }
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (auto const& it : values) {
    for (size_t i = 0; i < it.second; ++i) {
      builder->add(it.first);
    }
  }
  builder->close();
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function UNION
AqlValue functions::Union(ExpressionContext* expressionContext, AstNode const&,
                          VPackFunctionParametersView parameters) {
  static char const* AFN = "UNION";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  size_t const n = parameters.size();
  for (size_t i = 0; i < n; ++i) {
    AqlValue const& value =
        aql::functions::extractFunctionParameterValue(parameters, i);

    if (!value.isArray()) {
      // not an array
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    TRI_IF_FAILURE("AqlFunctions::OutOfMemory1") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    AqlValueMaterializer materializer(vopts);
    VPackSlice slice = materializer.slice(value);

    // this passes ownership for the JSON contents into result
    for (VPackSlice it : VPackArrayIterator(slice)) {
      builder->add(it);
      TRI_IF_FAILURE("AqlFunctions::OutOfMemory2") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    }
  }
  builder->close();
  TRI_IF_FAILURE("AqlFunctions::OutOfMemory3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return AqlValue(builder->slice(), builder->size());
}

/// @brief function UNION_DISTINCT
AqlValue functions::UnionDistinct(ExpressionContext* expressionContext,
                                  AstNode const&,
                                  VPackFunctionParametersView parameters) {
  static char const* AFN = "UNION_DISTINCT";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();

  size_t const n = parameters.size();
  containers::FlatHashSet<VPackSlice, basics::VelocyPackHelper::VPackHash,
                          basics::VelocyPackHelper::VPackEqual>
      values(512, basics::VelocyPackHelper::VPackHash(),
             basics::VelocyPackHelper::VPackEqual(vopts));

  std::vector<AqlValueMaterializer> materializers;
  materializers.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    AqlValue const& value =
        aql::functions::extractFunctionParameterValue(parameters, i);

    if (!value.isArray()) {
      // not an array
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    materializers.emplace_back(vopts);
    VPackSlice slice = materializers.back().slice(value);

    for (VPackSlice v : VPackArrayIterator(slice)) {
      v = v.resolveExternal();
      if (values.find(v) == values.end()) {
        TRI_IF_FAILURE("AqlFunctions::OutOfMemory1") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        values.emplace(v);
      }
    }
  }

  TRI_IF_FAILURE("AqlFunctions::OutOfMemory2") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (auto const& it : values) {
    builder->add(it);
  }
  builder->close();

  TRI_IF_FAILURE("AqlFunctions::OutOfMemory3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return AqlValue(builder->slice(), builder->size());
}

/// @brief function INTERSECTION
AqlValue functions::Intersection(ExpressionContext* expressionContext,
                                 AstNode const&,
                                 VPackFunctionParametersView parameters) {
  static char const* AFN = "INTERSECTION";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  std::unordered_map<VPackSlice, size_t, basics::VelocyPackHelper::VPackHash,
                     basics::VelocyPackHelper::VPackEqual>
      values(512, basics::VelocyPackHelper::VPackHash(),
             basics::VelocyPackHelper::VPackEqual(vopts));

  size_t const n = parameters.size();
  std::vector<AqlValueMaterializer> materializers;
  materializers.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    AqlValue const& value =
        aql::functions::extractFunctionParameterValue(parameters, i);

    if (!value.isArray()) {
      // not an array
      registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
      return AqlValue(AqlValueHintNull());
    }

    materializers.emplace_back(vopts);
    VPackSlice slice = materializers.back().slice(value);

    for (VPackSlice it : VPackArrayIterator(slice)) {
      if (i == 0) {
        // round one

        TRI_IF_FAILURE("AqlFunctions::OutOfMemory1") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        values.try_emplace(it, 1);
      } else {
        // check if we have seen the same element before
        auto found = values.find(it);
        if (found != values.end()) {
          // already seen
          if ((*found).second < i) {
            (*found).second = 0;
          } else {
            (*found).second = i + 1;
          }
        }
      }
    }
  }

  TRI_IF_FAILURE("AqlFunctions::OutOfMemory2") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (auto const& it : values) {
    if (it.second == n) {
      builder->add(it.first);
    }
  }
  builder->close();

  TRI_IF_FAILURE("AqlFunctions::OutOfMemory3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function OUTERSECTION
AqlValue functions::Outersection(ExpressionContext* expressionContext,
                                 AstNode const&,
                                 VPackFunctionParametersView parameters) {
  static char const* AFN = "OUTERSECTION";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  std::unordered_map<VPackSlice, size_t, basics::VelocyPackHelper::VPackHash,
                     basics::VelocyPackHelper::VPackEqual>
      values(512, basics::VelocyPackHelper::VPackHash(),
             basics::VelocyPackHelper::VPackEqual(vopts));

  size_t const n = parameters.size();
  std::vector<AqlValueMaterializer> materializers;
  materializers.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    AqlValue const& value =
        aql::functions::extractFunctionParameterValue(parameters, i);

    if (!value.isArray()) {
      // not an array
      registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
      return AqlValue(AqlValueHintNull());
    }

    materializers.emplace_back(vopts);
    VPackSlice slice = materializers.back().slice(value);

    for (VPackSlice it : VPackArrayIterator(slice)) {
      // check if we have seen the same element before
      auto result = values.insert({it, 1});
      if (!result.second) {
        // already seen
        TRI_ASSERT(result.first->second > 0);
        ++(result.first->second);
      }
    }
  }

  TRI_IF_FAILURE("AqlFunctions::OutOfMemory2") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (auto const& it : values) {
    if (it.second == 1) {
      builder->add(it.first);
    }
  }
  builder->close();

  TRI_IF_FAILURE("AqlFunctions::OutOfMemory3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function FLATTEN
AqlValue functions::Flatten(ExpressionContext* expressionContext,
                            AstNode const&,
                            VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  // cppcheck-suppress variableScope
  static char const* AFN = "FLATTEN";

  AqlValue const& list =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  if (!list.isArray()) {
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  size_t maxDepth = 1;
  if (parameters.size() == 2) {
    AqlValue const& maxDepthValue =
        aql::functions::extractFunctionParameterValue(parameters, 1);
    bool failed;
    double tmpMaxDepth = maxDepthValue.toDouble(failed);
    if (failed || tmpMaxDepth < 1) {
      maxDepth = 1;
    } else {
      maxDepth = static_cast<size_t>(tmpMaxDepth);
    }
  }

  AqlValueMaterializer materializer(vopts);
  VPackSlice listSlice = materializer.slice(list);

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  flattenList(listSlice, maxDepth, 0, *builder.get());
  builder->close();
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function FIRST
AqlValue functions::First(ExpressionContext* expressionContext, AstNode const&,
                          VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "FIRST";

  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    aql::functions::registerWarning(expressionContext, AFN,
                                    TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  if (value.length() == 0) {
    return AqlValue(AqlValueHintNull());
  }

  bool mustDestroy;
  return value.at(0, mustDestroy, true);
}

/// @brief function LAST
AqlValue functions::Last(ExpressionContext* expressionContext, AstNode const&,
                         VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "LAST";

  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    aql::functions::registerWarning(expressionContext, AFN,
                                    TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  VPackValueLength const n = value.length();

  if (n == 0) {
    return AqlValue(AqlValueHintNull());
  }

  bool mustDestroy;
  return value.at(n - 1, mustDestroy, true);
}

/// @brief function NTH
AqlValue functions::Nth(ExpressionContext* expressionContext, AstNode const&,
                        VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "NTH";

  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    aql::functions::registerWarning(expressionContext, AFN,
                                    TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  VPackValueLength const n = value.length();

  if (n == 0) {
    return AqlValue(AqlValueHintNull());
  }

  AqlValue const& position =
      aql::functions::extractFunctionParameterValue(parameters, 1);
  int64_t index = position.toInt64();

  if (index < 0 || index >= static_cast<int64_t>(n)) {
    return AqlValue(AqlValueHintNull());
  }

  bool mustDestroy;
  return value.at(index, mustDestroy, true);
}

/// @brief function Minus
AqlValue functions::Minus(ExpressionContext* expressionContext, AstNode const&,
                          VPackFunctionParametersView parameters) {
  static char const* AFN = "MINUS";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& baseArray =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!baseArray.isArray()) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  auto options = trx->transactionContextPtr()->getVPackOptions();
  std::unordered_map<VPackSlice, size_t, basics::VelocyPackHelper::VPackHash,
                     basics::VelocyPackHelper::VPackEqual>
      contains(512, basics::VelocyPackHelper::VPackHash(),
               basics::VelocyPackHelper::VPackEqual(options));

  // Fill the original map
  AqlValueMaterializer materializer(vopts);
  VPackSlice arraySlice = materializer.slice(baseArray);

  VPackArrayIterator it(arraySlice);
  while (it.valid()) {
    contains.try_emplace(it.value(), it.index());
    it.next();
  }

  // Iterate through all following parameters and delete found elements from the
  // map
  for (size_t k = 1; k < parameters.size(); ++k) {
    AqlValue const& next =
        aql::functions::extractFunctionParameterValue(parameters, k);
    if (!next.isArray()) {
      registerWarning(expressionContext, AFN,
                      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return AqlValue(AqlValueHintNull());
    }

    AqlValueMaterializer materializer(vopts);
    VPackSlice arraySlice = materializer.slice(next);

    for (VPackSlice search : VPackArrayIterator(arraySlice)) {
      auto find = contains.find(search);

      if (find != contains.end()) {
        contains.erase(find);
      }
    }
  }

  // We omit the normalize part from js, cannot occur here
  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (auto const& it : contains) {
    builder->add(it.first);
  }
  builder->close();
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function Slice
AqlValue functions::Slice(ExpressionContext* expressionContext, AstNode const&,
                          VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "SLICE";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& baseArray =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!baseArray.isArray()) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  // determine lower bound
  AqlValue fromValue =
      aql::functions::extractFunctionParameterValue(parameters, 1);
  int64_t from = fromValue.toInt64();
  if (from < 0) {
    from = baseArray.length() + from;
    if (from < 0) {
      from = 0;
    }
  }

  // determine upper bound
  AqlValue const& toValue =
      aql::functions::extractFunctionParameterValue(parameters, 2);
  int64_t to;
  if (toValue.isNull(true)) {
    to = baseArray.length();
  } else {
    to = toValue.toInt64();
    if (to >= 0) {
      to += from;
    } else {
      // negative to value
      to = baseArray.length() + to;
      if (to < 0) {
        to = 0;
      }
    }
  }

  AqlValueMaterializer materializer(vopts);
  VPackSlice arraySlice = materializer.slice(baseArray);

  transaction::BuilderLeaser builder(trx);
  builder->openArray();

  int64_t pos = 0;
  VPackArrayIterator it(arraySlice);
  while (it.valid()) {
    if (pos >= from && pos < to) {
      builder->add(it.value());
    }
    ++pos;
    if (pos >= to) {
      // done
      break;
    }
    it.next();
  }

  builder->close();
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function TO_ARRAY
AqlValue functions::ToArray(ExpressionContext* ctx, AstNode const&,
                            VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (value.isArray()) {
    // return copy of the original array
    return value.clone();
  }

  if (value.isNull(true)) {
    return AqlValue(AqlValueHintEmptyArray());
  }

  auto* trx = &ctx->trx();
  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  if (value.isBoolean() || value.isNumber() || value.isString()) {
    // return array with single member
    builder->add(value.slice());
  } else if (value.isObject()) {
    AqlValueMaterializer materializer(&trx->vpackOptions());
    VPackSlice slice = materializer.slice(value);
    // return an array with the attribute values
    for (auto it : VPackObjectIterator(slice, true)) {
      if (it.value.isCustom()) {
        builder->add(VPackValue(trx->extractIdString(slice)));
      } else {
        builder->add(it.value);
      }
    }
  }
  builder->close();
  return AqlValue(builder->slice(), builder->size());
}

}  // namespace arangodb::aql
