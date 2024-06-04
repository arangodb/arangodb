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
#include "Basics/Exceptions.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/fpconv.h"
#include "Containers/FlatHashMap.h"
#include "Containers/FlatHashSet.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Sink.h>
#include <velocypack/Slice.h>

#include <vector>

using namespace arangodb;

namespace arangodb::aql {

namespace {

/// @brief extract attribute names from the arguments
void extractKeys(containers::FlatHashSet<std::string>& names,
                 ExpressionContext* expressionContext,
                 VPackOptions const* vopts,
                 aql::functions::VPackFunctionParametersView parameters,
                 size_t startParameter, char const* functionName) {
  size_t const n = parameters.size();

  for (size_t i = startParameter; i < n; ++i) {
    AqlValue const& param =
        aql::functions::extractFunctionParameterValue(parameters, i);

    if (param.isString()) {
      names.emplace(param.slice().copyString());
    } else if (param.isNumber()) {
      double number = param.toDouble();

      if (std::isnan(number) || number == HUGE_VAL || number == -HUGE_VAL) {
        names.emplace("null");
      } else {
        char buffer[24];
        int length = fpconv_dtoa(number, &buffer[0]);
        names.emplace(&buffer[0], static_cast<size_t>(length));
      }
    } else if (param.isArray()) {
      AqlValueMaterializer materializer(vopts);
      VPackSlice s = materializer.slice(param);

      for (VPackSlice v : VPackArrayIterator(s)) {
        if (v.isString()) {
          names.emplace(v.copyString());
        } else {
          aql::functions::registerInvalidArgumentWarning(expressionContext,
                                                         functionName);
        }
      }
    }
  }
}

/// @brief Helper function to unset or keep all given names in the value.
///        Recursively iterates over sub-object and unsets or keeps their values
///        as well
void unsetOrKeep(transaction::Methods* trx, VPackSlice const& value,
                 containers::FlatHashSet<std::string> const& names,
                 bool unset,  // true means unset, false means keep
                 bool recursive, VPackBuilder& result) {
  TRI_ASSERT(value.isObject());
  VPackObjectBuilder b(&result);  // Close the object after this function
  for (auto const& entry : VPackObjectIterator(value, false)) {
    TRI_ASSERT(entry.key.isString());
    std::string_view key = entry.key.stringView();
    if ((names.find(key) == names.end()) == unset) {
      // not found and unset or found and keep
      if (recursive && entry.value.isObject()) {
        result.add(entry.key);  // Add the key
        unsetOrKeep(trx, entry.value, names, unset, recursive,
                    result);  // Adds the object
      } else {
        if (entry.value.isCustom()) {
          result.add(key, VPackValue(trx->extractIdString(value)));
        } else {
          result.add(key, entry.value);
        }
      }
    }
  }
}

/// @brief Helper function to merge given parameters
///        Works for an array of objects as first parameter or arbitrary many
///        object parameters
AqlValue mergeParameters(ExpressionContext* expressionContext,
                         aql::functions::VPackFunctionParametersView parameters,
                         char const* funcName, bool recursive) {
  size_t const n = parameters.size();

  if (n == 0) {
    return AqlValue(AqlValueHintEmptyObject());
  }

  auto& vopts = expressionContext->trx().vpackOptions();

  // use the first argument as the preliminary result
  AqlValue const& initial =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValueMaterializer materializer(&vopts);
  VPackSlice initialSlice = materializer.slice(initial);

  VPackBuilder builder;

  if (initial.isArray() && n == 1) {
    // special case: a single array parameter
    // Create an empty document as start point
    if (!recursive) {
      // Fast path for large arrays
      containers::FlatHashMap<std::string_view, VPackSlice> attributes;

      // first we construct a map holding the latest value for a key
      for (VPackSlice it : VPackArrayIterator(initialSlice)) {
        if (!it.isObject()) {
          aql::functions::registerInvalidArgumentWarning(expressionContext,
                                                         funcName);
          return AqlValue(AqlValueHintNull());
        }
        for (auto const& [key, value] :
             VPackObjectIterator(it, /*useSequentialIteration*/ true)) {
          attributes[key.stringView()] = value;
        }
      }

      // then we output the object
      {
        VPackObjectBuilder ob(&builder);
        for (auto const& [k, v] : attributes) {
          builder.add(k, v);
        }
      }

    } else {
      // slow path for recursive merge
      builder.openObject();
      builder.close();
      // merge in all other arguments
      for (VPackSlice it : VPackArrayIterator(initialSlice)) {
        if (!it.isObject()) {
          aql::functions::registerInvalidArgumentWarning(expressionContext,
                                                         funcName);
          return AqlValue(AqlValueHintNull());
        }
        builder = velocypack::Collection::merge(builder.slice(), it,
                                                /*mergeObjects*/ recursive,
                                                /*nullMeansRemove*/ false);
      }
    }

    return AqlValue(builder.slice(), builder.size());
  }

  if (!initial.isObject()) {
    aql::functions::registerInvalidArgumentWarning(expressionContext, funcName);
    return AqlValue(AqlValueHintNull());
  }

  // merge in all other arguments
  for (size_t i = 1; i < n; ++i) {
    AqlValue const& param =
        aql::functions::extractFunctionParameterValue(parameters, i);

    if (!param.isObject()) {
      aql::functions::registerInvalidArgumentWarning(expressionContext,
                                                     funcName);
      return AqlValue(AqlValueHintNull());
    }

    AqlValueMaterializer materializer(&vopts);
    VPackSlice slice = materializer.slice(param);

    builder = velocypack::Collection::merge(initialSlice, slice,
                                            /*mergeObjects*/ recursive,
                                            /*nullMeansRemove*/ false);
    initialSlice = builder.slice();
  }
  if (n == 1) {
    // only one parameter. now add original document
    builder.add(initialSlice);
  }
  return AqlValue(builder.slice(), builder.size());
}

}  // namespace

/// @brief function UNSET
AqlValue functions::Unset(ExpressionContext* expressionContext, AstNode const&,
                          VPackFunctionParametersView parameters) {
  static char const* AFN = "UNSET";

  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();

  if (!value.isObject()) {
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  containers::FlatHashSet<std::string> names;
  extractKeys(names, expressionContext, vopts, parameters, 1, AFN);

  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(value);
  transaction::BuilderLeaser builder(trx);
  unsetOrKeep(trx, slice, names, true, false, *builder.get());
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function UNSET_RECURSIVE
AqlValue functions::UnsetRecursive(ExpressionContext* expressionContext,
                                   AstNode const&,
                                   VPackFunctionParametersView parameters) {
  static char const* AFN = "UNSET_RECURSIVE";

  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isObject()) {
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();

  containers::FlatHashSet<std::string> names;
  extractKeys(names, expressionContext, vopts, parameters, 1, AFN);

  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(value);
  transaction::BuilderLeaser builder(trx);
  unsetOrKeep(trx, slice, names, true, true, *builder.get());
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function KEEP
AqlValue functions::Keep(ExpressionContext* expressionContext, AstNode const&,
                         VPackFunctionParametersView parameters) {
  static char const* AFN = "KEEP";

  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isObject()) {
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();

  containers::FlatHashSet<std::string> names;
  extractKeys(names, expressionContext, vopts, parameters, 1, AFN);

  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(value);
  transaction::BuilderLeaser builder(trx);
  unsetOrKeep(trx, slice, names, false, false, *builder.get());
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function KEEP_RECURSIVE
AqlValue functions::KeepRecursive(ExpressionContext* expressionContext,
                                  AstNode const&,
                                  VPackFunctionParametersView parameters) {
  static char const* AFN = "KEEP_RECURSIVE";

  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isObject()) {
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();

  containers::FlatHashSet<std::string> names;
  extractKeys(names, expressionContext, vopts, parameters, 1, AFN);

  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(value);
  transaction::BuilderLeaser builder(trx);
  unsetOrKeep(trx, slice, names, false, true, *builder.get());
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function TRANSLATE
AqlValue functions::Translate(ExpressionContext* expressionContext,
                              AstNode const&,
                              VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "TRANSLATE";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& key =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue const& lookupDocument =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  if (!lookupDocument.isObject()) {
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(lookupDocument);
  TRI_ASSERT(slice.isObject());

  VPackSlice result;
  if (key.isString()) {
    result = slice.get(key.slice().copyString());
  } else {
    transaction::StringLeaser buffer(trx);
    velocypack::StringSink adapter(buffer.get());
    functions::stringify(vopts, adapter, key.slice());
    result = slice.get(*buffer);
  }

  if (!result.isNone()) {
    return AqlValue(result);
  }

  // attribute not found, now return the default value
  // we must create copy of it however
  AqlValue const& defaultValue =
      aql::functions::extractFunctionParameterValue(parameters, 2);
  if (defaultValue.isNone()) {
    return key.clone();
  }
  return defaultValue.clone();
}

/// @brief function MERGE
AqlValue functions::Merge(ExpressionContext* expressionContext, AstNode const&,
                          VPackFunctionParametersView parameters) {
  return mergeParameters(expressionContext, parameters, "MERGE", false);
}

/// @brief function MERGE_RECURSIVE
AqlValue functions::MergeRecursive(ExpressionContext* expressionContext,
                                   AstNode const&,
                                   VPackFunctionParametersView parameters) {
  return mergeParameters(expressionContext, parameters, "MERGE_RECURSIVE",
                         true);
}

/// @brief function HAS
AqlValue functions::Has(ExpressionContext* expressionContext, AstNode const&,
                        VPackFunctionParametersView parameters) {
  size_t const n = parameters.size();
  if (n < 2) {
    // no parameters
    return AqlValue(AqlValueHintBool(false));
  }

  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isObject()) {
    // not an object
    return AqlValue(AqlValueHintBool(false));
  }

  transaction::Methods* trx = &expressionContext->trx();
  AqlValue const& name =
      aql::functions::extractFunctionParameterValue(parameters, 1);
  if (!name.isString()) {
    auto const& vopts = trx->vpackOptions();
    transaction::StringLeaser buffer(trx);
    velocypack::StringSink adapter(buffer.get());
    appendAsString(vopts, adapter, name);
    return AqlValue(AqlValueHintBool(value.hasKey(*buffer)));
  }
  return AqlValue(AqlValueHintBool(value.hasKey(name.slice().stringView())));
}

/// @brief function ATTRIBUTES
AqlValue functions::Attributes(ExpressionContext* expressionContext,
                               AstNode const&,
                               VPackFunctionParametersView parameters) {
  size_t const n = parameters.size();

  if (n < 1) {
    // no parameters
    return AqlValue(AqlValueHintNull());
  }

  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  if (!value.isObject()) {
    // not an object
    registerWarning(expressionContext, "ATTRIBUTES",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  bool const removeInternal = getBooleanParameter(parameters, 1, false);
  bool const doSort = getBooleanParameter(parameters, 2, false);

  TRI_ASSERT(value.isObject());
  if (value.length() == 0) {
    return AqlValue(AqlValueHintEmptyArray());
  }

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();

  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(value);

  if (doSort) {
    std::set<std::string_view,
             basics::VelocyPackHelper::AttributeSorterUTF8StringView>
        keys;

    VPackCollection::keys(slice, keys);
    transaction::BuilderLeaser builder(trx);
    builder->openArray();
    for (auto const& it : keys) {
      TRI_ASSERT(!it.empty());
      if (removeInternal && it.starts_with('_')) {
        continue;
      }
      builder->add(VPackValue(it));
    }
    builder->close();

    return AqlValue(builder->slice(), builder->size());
  }

  std::unordered_set<std::string_view> keys;
  VPackCollection::keys(slice, keys);

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (auto const& it : keys) {
    if (removeInternal && it.starts_with('_')) {
      continue;
    }
    builder->add(VPackValue(it));
  }
  builder->close();
  return AqlValue(builder->slice(), builder->size());
}

/// @brief function VALUES
AqlValue functions::Values(ExpressionContext* expressionContext, AstNode const&,
                           VPackFunctionParametersView parameters) {
  size_t const n = parameters.size();

  if (n < 1) {
    // no parameters
    return AqlValue(AqlValueHintNull());
  }

  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  if (!value.isObject()) {
    // not an object
    registerWarning(expressionContext, "VALUES",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  bool const removeInternal = getBooleanParameter(parameters, 1, false);

  TRI_ASSERT(value.isObject());
  if (value.length() == 0) {
    return AqlValue(AqlValueHintEmptyArray());
  }

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();

  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(value);
  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (auto entry : VPackObjectIterator(slice, true)) {
    if (!entry.key.isString()) {
      // somehow invalid
      continue;
    }
    if (removeInternal) {
      VPackValueLength l;
      char const* p = entry.key.getStringUnchecked(l);
      if (l > 0 && *p == '_') {
        // skip attribute
        continue;
      }
    }
    if (entry.value.isCustom()) {
      builder->add(VPackValue(trx->extractIdString(slice)));
    } else {
      builder->add(entry.value);
    }
  }
  builder->close();

  return AqlValue(builder->slice(), builder->size());
}

AqlValue functions::Value(ExpressionContext* expressionContext,
                          AstNode const& node,
                          VPackFunctionParametersView parameters) {
  size_t const n = parameters.size();

  if (n < 2) {
    // no parameters
    registerWarning(expressionContext, getFunctionName(node).data(),
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
    return AqlValue{AqlValueHintNull()};
  }

  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  if (!value.isObject()) {
    // not an object
    registerWarning(expressionContext, getFunctionName(node).data(),
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue{AqlValueHintNull()};
  }

  AqlValue const& pathArg =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  if (!pathArg.isArray()) {
    registerWarning(expressionContext, getFunctionName(node).data(),
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue{AqlValueHintNull()};
  }

  auto& trx = expressionContext->trx();
  AqlValueMaterializer materializer{&trx.vpackOptions()};
  VPackSlice slice{materializer.slice(value)};
  VPackSlice const root{slice};

  auto visitor = [&slice]<typename T>(T value) {
    static_assert(std::is_same_v<T, std::string_view> ||
                  std::is_same_v<T, size_t>);

    if constexpr (std::is_same_v<T, std::string_view>) {
      if (!slice.isObject()) {
        return false;
      }
      slice = slice.get(value);
      return !slice.isNone();
    } else if (std::is_same_v<T, size_t>) {
      if (!slice.isArray() || slice.length() <= value) {
        return false;
      }
      slice = slice.at(value);
      return true;
    }
  };

  if (0 == pathArg.length()) {
    return AqlValue{AqlValueHintNull{}};
  }

  for (auto entry : velocypack::ArrayIterator{pathArg.slice()}) {
    bool ok = false;
    if (entry.isString()) {
      ok = visitor(entry.stringView());
    } else if (entry.isNumber()) {
      ok = visitor(entry.getNumber<size_t>());
    }

    if (!ok) {
      return AqlValue{AqlValueHintNull{}};
    }
  }

  if (slice.isCustom()) {
    // The only custom slice is `_id` field
    return AqlValue{trx.extractIdString(root)};
  }

  return AqlValue{slice};
}

/// @brief function MATCHES
AqlValue functions::Matches(ExpressionContext* expressionContext,
                            AstNode const&,
                            VPackFunctionParametersView parameters) {
  static char const* AFN = "MATCHES";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& docToFind =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!docToFind.isObject()) {
    return AqlValue(AqlValueHintBool(false));
  }

  AqlValue const& exampleDocs =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  bool retIdx = false;
  if (parameters.size() == 3) {
    retIdx = aql::functions::extractFunctionParameterValue(parameters, 2)
                 .toBoolean();
  }

  AqlValueMaterializer materializer(vopts);
  VPackSlice docSlice = materializer.slice(docToFind);

  TRI_ASSERT(docSlice.isObject());

  transaction::BuilderLeaser builder(trx);
  AqlValueMaterializer exampleMaterializer(vopts);
  VPackSlice examples = exampleMaterializer.slice(exampleDocs);

  if (!examples.isArray()) {
    builder->openArray();
    builder->add(examples);
    builder->close();
    examples = builder->slice();
  }

  auto options = trx->transactionContextPtr()->getVPackOptions();

  bool foundMatch;
  int32_t idx = -1;

  for (auto example : VPackArrayIterator(examples)) {
    idx++;

    if (!example.isObject()) {
      registerWarning(expressionContext, AFN,
                      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      continue;
    }

    foundMatch = true;

    TRI_ASSERT(example.isObject());
    TRI_ASSERT(docSlice.isObject());
    for (auto it : VPackObjectIterator(example, true)) {
      VPackSlice keySlice = docSlice.get(it.key.stringView());

      if (it.value.isNull() && keySlice.isNone()) {
        continue;
      }

      if (keySlice.isNone() ||
          // compare inner content
          !basics::VelocyPackHelper::equal(keySlice, it.value, false, options,
                                           &docSlice, &example)) {
        foundMatch = false;
        break;
      }
    }

    if (foundMatch) {
      if (retIdx) {
        return AqlValue(AqlValueHintInt(idx));
      } else {
        return AqlValue(AqlValueHintBool(true));
      }
    }
  }

  if (retIdx) {
    return AqlValue(AqlValueHintInt(-1));
  }

  return AqlValue(AqlValueHintBool(false));
}

/// @brief function ZIP
AqlValue functions::Zip(ExpressionContext* expressionContext, AstNode const&,
                        VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "ZIP";

  AqlValue const& keys =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue const& values =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  if (!keys.isArray() || !values.isArray() ||
      keys.length() != values.length()) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();

  AqlValueMaterializer keyMaterializer(vopts);
  VPackSlice keysSlice = keyMaterializer.slice(keys);

  AqlValueMaterializer valueMaterializer(vopts);
  VPackSlice valuesSlice = valueMaterializer.slice(values);

  transaction::BuilderLeaser builder(trx);
  builder->openObject();

  // Buffer will temporarily hold the keys
  containers::FlatHashSet<std::string> keysSeen;
  transaction::StringLeaser buffer(trx);
  velocypack::StringSink adapter(buffer.get());

  VPackArrayIterator keysIt(keysSlice);
  VPackArrayIterator valuesIt(valuesSlice);

  TRI_ASSERT(keysIt.size() == valuesIt.size());

  while (keysIt.valid()) {
    TRI_ASSERT(valuesIt.valid());

    // stringify key
    buffer->clear();
    functions::stringify(vopts, adapter, keysIt.value());

    if (keysSeen.emplace(buffer->data(), buffer->length()).second) {
      // non-duplicate key
      builder->add(*buffer, valuesIt.value());
    }

    keysIt.next();
    valuesIt.next();
  }

  builder->close();

  return AqlValue(builder->slice(), builder->size());
}

AqlValue functions::Entries(ExpressionContext* expressionContext,
                            AstNode const&,
                            VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "ENTRIES";

  AqlValue const& object =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!object.isObject()) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();

  AqlValueMaterializer objectMaterializer(vopts);
  VPackSlice objectSlice = objectMaterializer.slice(object);

  transaction::BuilderLeaser builder(trx);
  builder->openArray();

  for (auto [key, value] : VPackObjectIterator(objectSlice, true)) {
    VPackArrayBuilder pair(builder.get(), true);
    builder->add(key);
    builder->add(value);
  }

  builder->close();

  return AqlValue(builder->slice(), builder->size());
}

}  // namespace arangodb::aql
