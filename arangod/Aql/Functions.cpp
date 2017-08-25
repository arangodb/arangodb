////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Functions.h"

#include <velocypack/Collection.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Function.h"
#include "Aql/Query.h"
#include "Aql/RegexCache.h"
#include "Basics/Exceptions.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringBuffer.h"
#include "Basics/Utf8Helper.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/fpconv.h"
#include "Basics/tri-strings.h"
#include "Indexes/Index.h"
#include "Random/UniformCharacter.h"
#include "Ssl/SslInterface.h"
#include "Utils/CollectionNameResolver.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Transaction/Context.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

using namespace arangodb;
using namespace arangodb::aql;

/// @brief convert a number value into an AqlValue
static AqlValue NumberValue(transaction::Methods* trx, int value) {
  transaction::BuilderLeaser builder(trx);
  builder->add(VPackValue(value));
  return AqlValue(builder.get());
}

/// @brief convert a number value into an AqlValue
static AqlValue NumberValue(transaction::Methods* trx, double value, bool nullify) {
  if (std::isnan(value) || !std::isfinite(value) || value == HUGE_VAL || value == -HUGE_VAL) {
    if (nullify) {
      // convert to null
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    }
    // convert to 0
    return AqlValue(arangodb::basics::VelocyPackHelper::ZeroValue());
  }
  
  transaction::BuilderLeaser builder(trx);
  builder->add(VPackValue(value));
  return AqlValue(builder.get());
}

/// @brief validate the number of parameters
void Functions::ValidateParameters(VPackFunctionParameters const& parameters,
                                   char const* function, int minParams,
                                   int maxParams) {
  if (parameters.size() < static_cast<size_t>(minParams) || 
      parameters.size() > static_cast<size_t>(maxParams)) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, function, minParams, maxParams);
  }
}

void Functions::ValidateParameters(VPackFunctionParameters const& parameters,
                                   char const* function, int minParams) {
  return ValidateParameters(parameters, function, minParams,
                            static_cast<int>(Function::MaxArguments));
}

/// @brief extract a function parameter from the arguments
AqlValue Functions::ExtractFunctionParameterValue(
    transaction::Methods*, VPackFunctionParameters const& parameters,
    size_t position) {
  if (position >= parameters.size()) {
    // parameter out of range
    return AqlValue();
  }
  return parameters[position];
}

/// @brief extra a collection name from an AqlValue
std::string Functions::ExtractCollectionName(
    transaction::Methods* trx, VPackFunctionParameters const& parameters,
    size_t position) {
  AqlValue value =
      ExtractFunctionParameterValue(trx, parameters, position);

  std::string identifier;
  
  if (value.isString()) {
    // already a string
    identifier = value.slice().copyString();
  } else {
    AqlValueMaterializer materializer(trx);
    VPackSlice s = materializer.slice(value, true);
    VPackSlice id = s;

    if (s.isObject() && s.hasKey(StaticStrings::IdString)) {
      id = s.get(StaticStrings::IdString);
    } 
    if (id.isString()) {
      identifier = id.copyString();
    } else if (id.isCustom()) {
      identifier = trx->extractIdString(s);
    }
  }

  if (!identifier.empty()) {
    size_t pos = identifier.find('/');

    if (pos != std::string::npos) {
      return identifier.substr(0, pos);
    }

    return identifier;
  }

  return StaticStrings::Empty;
}

/// @brief register warning
static void RegisterWarning(arangodb::aql::Query* query,
                            char const* functionName, int code) {
  std::string msg;

  if (code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH) {
    msg = arangodb::basics::Exception::FillExceptionString(code, functionName);
  } else {
    msg.append("in function '");
    msg.append(functionName);
    msg.append("()': ");
    msg.append(TRI_errno_string(code));
  }

  query->registerWarning(code, msg.c_str());
}

/// @brief register usage of an invalid function argument
static void RegisterInvalidArgumentWarning(arangodb::aql::Query* query,
                                           char const* functionName) {
  RegisterWarning(query, functionName,
                  TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

/// @brief converts a value into a number value
static double ValueToNumber(VPackSlice const& slice, bool& isValid) {
  if (slice.isNull()) {
    isValid = true;
    return 0.0;
  }
  if (slice.isBoolean()) {
    isValid = true;
    return (slice.getBoolean() ? 1.0 : 0.0);
  }
  if (slice.isNumber()) {
    isValid = true;
    return slice.getNumericValue<double>();
  }
  if (slice.isString()) {
    std::string const str = slice.copyString();
    try {
      if (str.empty()) {
        isValid = true;
        return 0.0;
      }
      size_t behind = 0;
      double value = std::stod(str, &behind);
      while (behind < str.size()) {
        char c = str[behind];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '\f') {
          isValid = false;
          return 0.0;
        }
        ++behind;
      }
      isValid = true;
      return value;
    } catch (...) {
      size_t behind = 0;
      while (behind < str.size()) {
        char c = str[behind];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '\f') {
          isValid = false;
          return 0.0;
        }
        ++behind;
      }
      // A string only containing whitespae-characters is valid and should return 0.0
      // It throws in std::stod
      isValid = true;
      return 0.0;
    }
  }
  if (slice.isArray()) {
    VPackValueLength const n = slice.length();
    if (n == 0) {
      isValid = true;
      return 0.0;
    }
    if (n == 1) {
      return ValueToNumber(slice.at(0), isValid);
    }
  }

  // All other values are invalid
  isValid = false;
  return 0.0;
}

/// @brief extract a boolean parameter from an array
static bool GetBooleanParameter(transaction::Methods* trx,
                                VPackFunctionParameters const& parameters,
                                size_t startParameter, bool defaultValue) {
  size_t const n = parameters.size();

  if (startParameter >= n) {
    return defaultValue;
  }

  return parameters[startParameter].toBoolean();
}

/// @brief extract attribute names from the arguments
void Functions::ExtractKeys(std::unordered_set<std::string>& names,
                            arangodb::aql::Query* query,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters,
                            size_t startParameter, char const* functionName) {
  size_t const n = parameters.size();

  for (size_t i = startParameter; i < n; ++i) {
    AqlValue param = ExtractFunctionParameterValue(trx, parameters, i);

    if (param.isString()) {
      names.emplace(param.slice().copyString());
    } else if (param.isNumber()) {
      double number = param.toDouble(trx);

      if (std::isnan(number) || number == HUGE_VAL || number == -HUGE_VAL) {
        names.emplace("null");
      } else {
        char buffer[24];
        int length = fpconv_dtoa(number, &buffer[0]);
        names.emplace(std::string(&buffer[0], static_cast<size_t>(length)));
      }
    } else if (param.isArray()) {
      AqlValueMaterializer materializer(trx);
      VPackSlice s = materializer.slice(param, false);

      for (auto const& v : VPackArrayIterator(s)) {
        if (v.isString()) {
          names.emplace(v.copyString());
        } else {
          RegisterInvalidArgumentWarning(query, functionName);
        }
      }
    }
  }
}

/// @brief append the VelocyPack value to a string buffer
///        Note: Backwards compatibility. Is different than Slice.toJson()
void Functions::Stringify(transaction::Methods* trx,
                          arangodb::basics::VPackStringBufferAdapter& buffer,
                          VPackSlice const& slice) {
  if (slice.isNull()) {
    // null is the empty string
    return;
  }

  if (slice.isString()) {
    // dumping adds additional ''
    VPackValueLength length;
    char const* p = slice.getString(length);
    buffer.append(p, length);
    return;
  }
   
  VPackOptions* options = trx->transactionContextPtr()->getVPackOptionsForDump();
  VPackOptions adjustedOptions = *options;
  adjustedOptions.escapeUnicode = false;
  adjustedOptions.escapeForwardSlashes = false;
  VPackDumper dumper(&buffer, &adjustedOptions);
  dumper.dump(slice);
}

/// @brief append the VelocyPack value to a string buffer
///        Note: Backwards compatibility. Is different than Slice.toJson()
static void AppendAsString(transaction::Methods* trx,
                           arangodb::basics::VPackStringBufferAdapter& buffer,
                           AqlValue const& value) {
  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);

  Functions::Stringify(trx, buffer, slice);
}

/// @brief Checks if the given list contains the element
static bool ListContainsElement(transaction::Methods* trx,
                                VPackOptions const* options,
                                AqlValue const& list,
                                AqlValue const& testee, size_t& index) {
  TRI_ASSERT(list.isArray());
  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(list, false);
  
  AqlValueMaterializer testeeMaterializer(trx);
  VPackSlice testeeSlice = testeeMaterializer.slice(testee, false);

  VPackArrayIterator it(slice);
  while (it.valid()) {
    if (arangodb::basics::VelocyPackHelper::compare(testeeSlice, it.value(), false, options) == 0) {
      index = static_cast<size_t>(it.index());
      return true;
    }
    it.next();
  }
  return false;
}

/// @brief Checks if the given list contains the element
/// DEPRECATED
static bool ListContainsElement(VPackOptions const* options,
                                VPackSlice const& list,
                                VPackSlice const& testee, size_t& index) {
  TRI_ASSERT(list.isArray());
  for (size_t i = 0; i < static_cast<size_t>(list.length()); ++i) {
    if (arangodb::basics::VelocyPackHelper::compare(testee, list.at(i),
                                                    false, options) == 0) {
      index = i;
      return true;
    }
  }
  return false;
}

static bool ListContainsElement(VPackOptions const* options,
                                VPackSlice const& list,
                                VPackSlice const& testee) {
  size_t unused;
  return ListContainsElement(options, list, testee, unused);
}

/// @brief Computes the Variance of the given list.
///        If successful value will contain the variance and count
///        will contain the number of elements.
///        If not successful value and count contain garbage.
static bool Variance(transaction::Methods* trx,
                     AqlValue const& values, double& value, size_t& count) {
  TRI_ASSERT(values.isArray());
  value = 0.0;
  count = 0;
  bool unused = false;
  double mean = 0.0;
  
  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(values, false);

  for (auto const& element : VPackArrayIterator(slice)) {
    if (!element.isNull()) {
      if (!element.isNumber()) {
        return false;
      }
      double current = ValueToNumber(element, unused);
      count++;
      double delta = current - mean;
      mean += delta / count;
      value += delta * (current - mean);
    }
  }
  return true;
}

/// @brief Sorts the given list of Numbers in ASC order.
///        Removes all null entries.
///        Returns false if the list contains non-number values.
static bool SortNumberList(transaction::Methods* trx,
                           AqlValue const& values,
                           std::vector<double>& result) {
  TRI_ASSERT(values.isArray());
  TRI_ASSERT(result.empty());
  bool unused;
  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(values, false);

  for (auto const& element : VPackArrayIterator(slice)) {
    if (!element.isNull()) {
      if (!element.isNumber()) {
        return false;
      }
      result.emplace_back(ValueToNumber(element, unused));
    }
  }
  std::sort(result.begin(), result.end());
  return true;
}

/// @brief Helper function to unset or keep all given names in the value.
///        Recursively iterates over sub-object and unsets or keeps their values
///        as well
static void UnsetOrKeep(transaction::Methods* trx,
                        VPackSlice const& value,
                        std::unordered_set<std::string> const& names,
                        bool unset,  // true means unset, false means keep
                        bool recursive, VPackBuilder& result) {
  TRI_ASSERT(value.isObject());
  VPackObjectBuilder b(&result); // Close the object after this function
  for (auto const& entry : VPackObjectIterator(value, false)) {
    TRI_ASSERT(entry.key.isString());
    std::string key = entry.key.copyString();
    if ((names.find(key) == names.end()) == unset) {
      // not found and unset or found and keep 
      if (recursive && entry.value.isObject()) {
        result.add(entry.key); // Add the key
        UnsetOrKeep(trx, entry.value, names, unset, recursive, result); // Adds the object
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

/// @brief Helper function to get a document by it's identifier
///        Lazy Locks the collection if necessary.
static void GetDocumentByIdentifier(transaction::Methods* trx,
                                    std::string& collectionName,
                                    std::string const& identifier,
                                    bool ignoreError,
                                    VPackBuilder& result) {
  transaction::BuilderLeaser searchBuilder(trx);

  size_t pos = identifier.find('/');
  if (pos == std::string::npos) {
    searchBuilder->add(VPackValue(identifier));
  } else {
    if (collectionName.empty()) {
      searchBuilder->add(VPackValue(identifier.substr(pos + 1)));
      collectionName = identifier.substr(0, pos);
    } else if (identifier.substr(0, pos) != collectionName) {
      // Requesting an _id that cannot be stored in this collection
      if (ignoreError) {
        return;
      }
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST);
    } else {
      searchBuilder->add(VPackValue(identifier.substr(pos + 1)));
    }
  }
 
  Result res;
  try {
    res = trx->documentFastPath(collectionName, nullptr, searchBuilder->slice(), result,
                                true);
  } catch (arangodb::basics::Exception const& ex) {
    res.reset(ex.code());
  }

  if (!res.ok()) {
    if (ignoreError) {
      if (res.errorNumber() == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND || 
          res.errorNumber() == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND ||
          res.errorNumber() == TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST) {
        return;
      }
    }
    if (res.errorNumber() == TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION) {
      // special error message to indicate which collection was undeclared
      THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage() + ": " + collectionName + " [" + AccessMode::typeString(AccessMode::Type::READ) + "]");
    }
    THROW_ARANGO_EXCEPTION(res);
  }
}

/// @brief Helper function to merge given parameters
///        Works for an array of objects as first parameter or arbitrary many
///        object parameters
AqlValue Functions::MergeParameters(arangodb::aql::Query* query,
                                 transaction::Methods* trx,
                                 VPackFunctionParameters const& parameters,
                                 char const* funcName,
                                 bool recursive) {
  size_t const n = parameters.size();

  if (n == 0) {
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyObjectValue());
  }

  // use the first argument as the preliminary result
  AqlValue initial = ExtractFunctionParameterValue(trx, parameters, 0);
  AqlValueMaterializer materializer(trx);
  VPackSlice initialSlice = materializer.slice(initial, true);
  
  VPackBuilder builder;

  if (initial.isArray() && n == 1) {
    // special case: a single array parameter
    // Create an empty document as start point
    builder.openObject();
    builder.close();
    // merge in all other arguments
    for (auto const& it : VPackArrayIterator(initialSlice)) {
      if (!it.isObject()) {
        RegisterInvalidArgumentWarning(query, funcName);
        return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
      }
      builder = arangodb::basics::VelocyPackHelper::merge(builder.slice(), it, false,
                                                          recursive);
    }
    return AqlValue(builder);
  }

  if (!initial.isObject()) {
    RegisterInvalidArgumentWarning(query, funcName);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  // merge in all other arguments
  for (size_t i = 1; i < n; ++i) {
    AqlValue param = ExtractFunctionParameterValue(trx, parameters, i);

    if (!param.isObject()) {
      RegisterInvalidArgumentWarning(query, funcName);
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    }
    
    AqlValueMaterializer materializer(trx);
    VPackSlice slice = materializer.slice(param, false);

    builder = arangodb::basics::VelocyPackHelper::merge(initialSlice, slice, false,
                                                        recursive);
    initialSlice = builder.slice();
  }
  if (n == 1) {
    // only one parameter. now add original document
    builder.add(initialSlice);
  }
  return AqlValue(builder);
}

/// @brief internal recursive flatten helper
static void FlattenList(VPackSlice const& array, size_t maxDepth,
                        size_t curDepth, VPackBuilder& result) {
  TRI_ASSERT(result.isOpenArray());
  for (auto const& tmp : VPackArrayIterator(array)) {
    if (tmp.isArray() && curDepth < maxDepth) {
      FlattenList(tmp, maxDepth, curDepth + 1, result);
    } else {
      // Copy the content of tmp into the result
      result.add(tmp);
    }
  }
}

/// @brief function IS_NULL
AqlValue Functions::IsNull(arangodb::aql::Query* query,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(trx, parameters, 0);
  return AqlValue(a.isNull(true));
}

/// @brief function IS_BOOL
AqlValue Functions::IsBool(arangodb::aql::Query* query,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(trx, parameters, 0);
  return AqlValue(a.isBoolean());
}

/// @brief function IS_NUMBER
AqlValue Functions::IsNumber(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(trx, parameters, 0);
  return AqlValue(a.isNumber());
}

/// @brief function IS_STRING
AqlValue Functions::IsString(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(trx, parameters, 0);
  return AqlValue(a.isString());
}

/// @brief function IS_ARRAY
AqlValue Functions::IsArray(arangodb::aql::Query* query,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(trx, parameters, 0);
  return AqlValue(a.isArray());
}

/// @brief function IS_OBJECT
AqlValue Functions::IsObject(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(trx, parameters, 0);
  return AqlValue(a.isObject());
}

/// @brief function TYPENAME
AqlValue Functions::Typename(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (value.isObject()) {
    return AqlValue(TRI_CHAR_LENGTH_PAIR("object"));
  }
  if (value.isArray()) {
    return AqlValue(TRI_CHAR_LENGTH_PAIR("array"));
  }
  if (value.isString()) {
    return AqlValue(TRI_CHAR_LENGTH_PAIR("string"));
  }
  if (value.isNumber()) {
    return AqlValue(TRI_CHAR_LENGTH_PAIR("number"));
  }
  if (value.isBoolean()) {
    return AqlValue(TRI_CHAR_LENGTH_PAIR("bool"));
  }
  return AqlValue(TRI_CHAR_LENGTH_PAIR("null"));
}

/// @brief function TO_NUMBER
AqlValue Functions::ToNumber(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(trx, parameters, 0);
  bool failed;
  double value = a.toDouble(trx, failed);

  if (failed) {
    return AqlValue(arangodb::basics::VelocyPackHelper::ZeroValue());
  }
  
  transaction::BuilderLeaser builder(trx);
  builder->add(VPackValue(value));
  return AqlValue(builder.get());
}

/// @brief function TO_STRING
AqlValue Functions::ToString(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  AppendAsString(trx, adapter, value);
  transaction::BuilderLeaser builder(trx);
  return AqlValue(buffer->begin(), buffer->length());
}

/// @brief function TO_BOOL
AqlValue Functions::ToBool(arangodb::aql::Query* query,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(trx, parameters, 0);
  return AqlValue(a.toBoolean());
}

/// @brief function TO_ARRAY
AqlValue Functions::ToArray(arangodb::aql::Query* query,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (value.isArray()) {
    // return copy of the original array
    return value.clone();
  }

  if (value.isNull(true)) {
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  if (value.isBoolean() || value.isNumber() || value.isString()) {
    // return array with single member
    builder->add(value.slice());
  } else if (value.isObject()) {
    AqlValueMaterializer materializer(trx);
    VPackSlice slice = materializer.slice(value, false);
    // return an array with the attribute values
    for (auto const& it : VPackObjectIterator(slice, true)) {
      if (it.value.isCustom()) {
        builder->add(VPackValue(trx->extractIdString(slice)));
      } else {
        builder->add(it.value);
      }
    }
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function LENGTH
AqlValue Functions::Length(arangodb::aql::Query* query,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {

  ValidateParameters(parameters, "LENGTH", 1, 1);

  transaction::BuilderLeaser builder(trx);

  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  if (value.isArray()) {
    // shortcut!
    builder->add(VPackValue(value.length()));
    return AqlValue(builder->slice());
  }

  size_t length = 0;
  if (value.isNull(true)) {
    length = 0;
  } else if (value.isBoolean()) {
    if (value.toBoolean()) {
      length = 1;
    } else {
      length = 0;
    }
  } else if (value.isNumber()) {
    double tmp = value.toDouble(trx);
    if (std::isnan(tmp) || !std::isfinite(tmp)) {
      length = 0;
    } else {
      char buffer[24];
      length = static_cast<size_t>(fpconv_dtoa(tmp, buffer));
    }
  } else if (value.isString()) {
    VPackValueLength l;
    char const* p = value.slice().getString(l);
    length = TRI_CharLengthUtf8String(p, l);
  } else if (value.isObject()) {
    length = static_cast<size_t>(value.length());
  }
  builder->add(VPackValue(static_cast<uint64_t>(length)));
  return AqlValue(builder.get());
}

/// @brief function FIRST
AqlValue Functions::First(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "FIRST", 1, 1);
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "FIRST", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  if (value.length() == 0) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  bool mustDestroy;
  return value.at(trx, 0, mustDestroy, true);
}

/// @brief function LAST
AqlValue Functions::Last(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "LAST", 1, 1);
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "LAST", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  VPackValueLength const n = value.length();

  if (n == 0) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  bool mustDestroy;
  return value.at(trx, n - 1, mustDestroy, true);
}

/// @brief function NTH
AqlValue Functions::Nth(arangodb::aql::Query* query,
                        transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "NTH", 2, 2);
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "NTH", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  VPackValueLength const n = value.length();

  if (n == 0) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  AqlValue position = ExtractFunctionParameterValue(trx, parameters, 1);
  int64_t index = position.toInt64(trx);

  if (index < 0 || index >= static_cast<int64_t>(n)) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  bool mustDestroy;
  return value.at(trx, index, mustDestroy, true);
}

/// @brief function CONTAINS
AqlValue Functions::Contains(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "CONTAINS", 2, 3);

  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  AqlValue search = ExtractFunctionParameterValue(trx, parameters, 1);
  AqlValue returnIndex = ExtractFunctionParameterValue(trx, parameters, 2);
  
  bool const willReturnIndex = returnIndex.toBoolean();

  int result = -1; // default is "not found"
  {
    transaction::StringBufferLeaser buffer(trx);
    arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

    AppendAsString(trx, adapter, value);
    size_t const valueLength = buffer->length();
  
    size_t const searchOffset = buffer->length();
    AppendAsString(trx, adapter, search);
    size_t const searchLength = buffer->length() - valueLength;

    if (searchLength > 0) {
      char const* found = static_cast<char const*>(memmem(buffer->c_str(), valueLength, buffer->c_str() + searchOffset, searchLength));

      if (found != nullptr) {
        if (willReturnIndex) {
          // find offset into string
          int bytePosition = static_cast<int>(found - buffer->c_str());
          char const* p = buffer->c_str();
          int pos = 0;
          while (pos < bytePosition) {
            unsigned char c = static_cast<unsigned char>(*p);
            if (c < 128) {
              ++pos;
            } else if (c < 224) {
              pos += 2;
            } else if (c < 240) {
              pos += 3;
            } else if (c < 248) {
              pos += 4;
            }
          }
          result = pos;
        } else {
          // fake result position, but it does not matter as it will
          // only be compared to -1 later
          result = 0;
        }
      }
    }
  }

  if (willReturnIndex) {
    // return numeric value
    return NumberValue(trx, result);
  }

  // return boolean
  return AqlValue(result != -1);
}

/// @brief function CONCAT
AqlValue Functions::Concat(arangodb::aql::Query* query,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  size_t const n = parameters.size();

  if (n == 1) {
    AqlValue member = ExtractFunctionParameterValue(trx, parameters, 0);
    if (member.isArray()) {
      AqlValueMaterializer materializer(trx);
      VPackSlice slice = materializer.slice(member, false);

      for (auto const& it : VPackArrayIterator(slice)) {
        if (it.isNull()) {
          continue;
        }
        // convert member to a string and append
        AppendAsString(trx, adapter, AqlValue(it.begin()));
      }
      return AqlValue(buffer->c_str(), buffer->length());
    }
  }

  for (size_t i = 0; i < n; ++i) {
    AqlValue member = ExtractFunctionParameterValue(trx, parameters, i);

    if (member.isNull(true)) {
      continue;
    }

    // convert member to a string and append
    AppendAsString(trx, adapter, member);
  }

  return AqlValue(buffer->c_str(), buffer->length());
}

/// @brief function CONCAT_SEPARATOR
AqlValue Functions::ConcatSeparator(arangodb::aql::Query* query,
                                    transaction::Methods* trx,
                                    VPackFunctionParameters const& parameters) {
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  bool found = false;
  size_t const n = parameters.size();
    
  AqlValue separator = ExtractFunctionParameterValue(trx, parameters, 0);
  AppendAsString(trx, adapter, separator);
  std::string const s(buffer->c_str(), buffer->length());

  buffer->clear();

  if (n == 2) {
    AqlValue member = ExtractFunctionParameterValue(trx, parameters, 1);

    if (member.isArray()) {
      // reserve *some* space
      buffer->reserve((s.size() + 10) * member.length());

      AqlValueMaterializer materializer(trx);
      VPackSlice slice = materializer.slice(member, false);

      for (auto const& it : VPackArrayIterator(slice)) {
        if (it.isNull()) {
          continue;
        }
        if (found) {
          buffer->appendText(s);
        }
        // convert member to a string and append
        AppendAsString(trx, adapter, AqlValue(it.begin()));
        found = true;
      }
      return AqlValue(buffer->c_str(), buffer->length());
    }
  }

  // reserve *some* space
  buffer->reserve((s.size() + 10) * n);
  for (size_t i = 1; i < n; ++i) {
    AqlValue member = ExtractFunctionParameterValue(trx, parameters, i);

    if (member.isNull(true)) {
      continue;
    }
    if (found) {
      buffer->appendText(s);
    }

    // convert member to a string and append
    AppendAsString(trx, adapter, member);
    found = true;
  }

  return AqlValue(buffer->c_str(), buffer->length());
}

/// @brief function CHAR_LENGTH
AqlValue Functions::CharLength(arangodb::aql::Query* query,
                                    transaction::Methods* trx,
                                    VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "CHAR_LENGTH", 1, 1);

  transaction::BuilderLeaser builder(trx);

  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  size_t length = 0;

  if (value.isArray() || value.isObject()) {
    AqlValueMaterializer materializer(trx);
    VPackSlice slice = materializer.slice(value, false);

    transaction::StringBufferLeaser buffer(trx);
    arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

    VPackDumper dumper(&adapter, trx->transactionContextPtr()->getVPackOptions());
    dumper.dump(slice);

    length = buffer->length();

  } else if (value.isNull(true)) {
    length = 0;

  } else if (value.isBoolean()) {
    if (value.toBoolean()) {
      length = 4;
    } else {
      length = 5;
    }

  } else if (value.isNumber()) {
    double tmp = value.toDouble(trx);
    if (std::isnan(tmp) || !std::isfinite(tmp)) {
      length = 0;
    } else {
      char buffer[24];
      length = static_cast<size_t>(fpconv_dtoa(tmp, buffer));
    }

  } else if (value.isString()) {
    VPackValueLength l;
    char const* p = value.slice().getString(l);
    length = TRI_CharLengthUtf8String(p, l);
  }

  builder->add(VPackValue(static_cast<uint64_t>(length)));
  return AqlValue(builder.get());
}

// #include "unicode/utypes.h"
// #include "unicode/uchar.h"
// #include "unicode/locid.h"
// #include "unicode/ustring.h"
// #include "unicode/ucnv.h"
#include "unicode/unistr.h"


/// @brief function LOWER
AqlValue Functions::Lower(arangodb::aql::Query* query,
                                    transaction::Methods* trx,
                                    VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "LOWER", 1, 1);

  std::string utf8;
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  AppendAsString(trx, adapter, value);

  UnicodeString s(buffer->c_str(), static_cast<int32_t>(buffer->length()));
  s.toLower(NULL);
  s.toUTF8String(utf8);

  return AqlValue(utf8.c_str(), utf8.length());
}

/// @brief function UPPER
AqlValue Functions::Upper(arangodb::aql::Query* query,
                                    transaction::Methods* trx,
                                    VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "UPPER", 1, 1);

  std::string utf8;
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  AppendAsString(trx, adapter, value);

  UnicodeString s(buffer->c_str(), static_cast<int32_t>(buffer->length()));
  s.toUpper(NULL);
  s.toUTF8String(utf8);

  return AqlValue(utf8.c_str(), utf8.length());
}

/// @brief function LIKE
AqlValue Functions::Like(arangodb::aql::Query* query,
                         transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "LIKE", 2, 3);
  bool const caseInsensitive = GetBooleanParameter(trx, parameters, 2, false);
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  // build pattern from parameter #1
  AqlValue regex = ExtractFunctionParameterValue(trx, parameters, 1);
  AppendAsString(trx, adapter, regex);
  
  // the matcher is owned by the query!
  ::RegexMatcher* matcher = query->regexCache()->buildLikeMatcher(buffer->c_str(), buffer->length(), caseInsensitive);

  if (matcher == nullptr) {
    // compiling regular expression failed
    RegisterWarning(query, "LIKE", TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  // extract value
  buffer->clear();
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  AppendAsString(trx, adapter, value);

  bool error = false;
  bool const result = arangodb::basics::Utf8Helper::DefaultUtf8Helper.matches(
      matcher, buffer->c_str(), buffer->length(), false, error);

  if (error) {
    // compiling regular expression failed
    RegisterWarning(query, "LIKE", TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  } 
  
  return AqlValue(result);
}

/// @brief function REGEX_TEST
AqlValue Functions::RegexTest(arangodb::aql::Query* query,
                              transaction::Methods* trx,
                              VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "REGEX_TEST", 2, 3);
  bool const caseInsensitive = GetBooleanParameter(trx, parameters, 2, false);
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  // build pattern from parameter #1
  AqlValue regex = ExtractFunctionParameterValue(trx, parameters, 1);
  AppendAsString(trx, adapter, regex);

  // the matcher is owned by the query!
  ::RegexMatcher* matcher = query->regexCache()->buildRegexMatcher(buffer->c_str(), buffer->length(), caseInsensitive);

  if (matcher == nullptr) {
    // compiling regular expression failed
    RegisterWarning(query, "REGEX_TEST", TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  // extract value
  buffer->clear();
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  AppendAsString(trx, adapter, value);

  bool error = false;
  bool const result = arangodb::basics::Utf8Helper::DefaultUtf8Helper.matches(
      matcher, buffer->c_str(), buffer->length(), true, error);

  if (error) {
    // compiling regular expression failed
    RegisterWarning(query, "REGEX_TEST", TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  } 
  
  return AqlValue(result);
}

/// @brief function REGEX_REPLACE
AqlValue Functions::RegexReplace(arangodb::aql::Query* query,
                                 transaction::Methods* trx,
                                 VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "REGEX_REPLACE", 3, 4);
  bool const caseInsensitive = GetBooleanParameter(trx, parameters, 3, false);
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  // build pattern from parameter #1
  AqlValue regex = ExtractFunctionParameterValue(trx, parameters, 1);
  AppendAsString(trx, adapter, regex);

  // the matcher is owned by the query!
  ::RegexMatcher* matcher = query->regexCache()->buildRegexMatcher(buffer->c_str(), buffer->length(), caseInsensitive);

  if (matcher == nullptr) {
    // compiling regular expression failed
    RegisterWarning(query, "REGEX_REPLACE", TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  // extract value
  buffer->clear();
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  AppendAsString(trx, adapter, value);

  size_t const split = buffer->length();
  AqlValue replace = ExtractFunctionParameterValue(trx, parameters, 2);
  AppendAsString(trx, adapter, replace);

  bool error = false;
  std::string result = arangodb::basics::Utf8Helper::DefaultUtf8Helper.replace(
      matcher, buffer->c_str(), split, buffer->c_str() + split, buffer->length() - split, false, error);

  if (error) {
    // compiling regular expression failed
    RegisterWarning(query, "REGEX_REPLACE", TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  } 
  
  return AqlValue(result);
}

/// @brief function PASSTHRU
AqlValue Functions::Passthru(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  if (parameters.empty()) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  return ExtractFunctionParameterValue(trx, parameters, 0).clone();
}

/// @brief function UNSET
AqlValue Functions::Unset(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "UNSET", 2);
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isObject()) {
    RegisterInvalidArgumentWarning(query, "UNSET");
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  std::unordered_set<std::string> names;
  ExtractKeys(names, query, trx, parameters, 1, "UNSET");

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);
  transaction::BuilderLeaser builder(trx);
  UnsetOrKeep(trx, slice, names, true, false, *builder.get());
  return AqlValue(builder.get());
}

/// @brief function UNSET_RECURSIVE
AqlValue Functions::UnsetRecursive(arangodb::aql::Query* query,
                                   transaction::Methods* trx,
                                   VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "UNSET_RECURSIVE", 2);
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isObject()) {
    RegisterInvalidArgumentWarning(query, "UNSET_RECURSIVE");
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  std::unordered_set<std::string> names;
  ExtractKeys(names, query, trx, parameters, 1, "UNSET_RECURSIVE");

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);
  transaction::BuilderLeaser builder(trx);
  UnsetOrKeep(trx, slice, names, true, true, *builder.get());
  return AqlValue(builder.get());
}

/// @brief function KEEP
AqlValue Functions::Keep(arangodb::aql::Query* query,
                         transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "KEEP", 2);
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isObject()) {
    RegisterInvalidArgumentWarning(query, "KEEP");
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  std::unordered_set<std::string> names;

  ExtractKeys(names, query, trx, parameters, 1, "KEEP");

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);
  transaction::BuilderLeaser builder(trx);
  UnsetOrKeep(trx, slice, names, false, false, *builder.get());
  return AqlValue(builder.get());
}

/// @brief function MERGE
AqlValue Functions::Merge(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  return MergeParameters(query, trx, parameters, "MERGE", false);
}

/// @brief function MERGE_RECURSIVE
AqlValue Functions::MergeRecursive(arangodb::aql::Query* query,
                                   transaction::Methods* trx,
                                   VPackFunctionParameters const& parameters) {
  return MergeParameters(query, trx, parameters, "MERGE_RECURSIVE", true);
}

/// @brief function HAS
AqlValue Functions::Has(arangodb::aql::Query* query,
                        transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n < 2) {
    // no parameters
    return AqlValue(false);
  }

  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isObject()) {
    // not an object
    return AqlValue(false);
  }

  AqlValue name = ExtractFunctionParameterValue(trx, parameters, 1);
  std::string p;
  if (!name.isString()) {
    transaction::StringBufferLeaser buffer(trx);
    arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());
    AppendAsString(trx, adapter, name);
    p = std::string(buffer->c_str(), buffer->length());
  } else {
    p = name.slice().copyString();
  }

  return AqlValue(value.hasKey(trx, p));
}

/// @brief function ATTRIBUTES
AqlValue Functions::Attributes(arangodb::aql::Query* query,
                               transaction::Methods* trx,
                               VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 1) {
    // no parameters
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  if (!value.isObject()) {
    // not an object
    RegisterWarning(query, "ATTRIBUTES",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  bool const removeInternal = GetBooleanParameter(trx, parameters, 1, false);
  bool const doSort = GetBooleanParameter(trx, parameters, 2, false);

  TRI_ASSERT(value.isObject());
  if (value.length() == 0) {
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);

  if (doSort) {
    std::set<std::string, arangodb::basics::VelocyPackHelper::AttributeSorterUTF8>
        keys;

    VPackCollection::keys(slice, keys);
    VPackBuilder result;
    result.openArray();
    for (auto const& it : keys) {
      TRI_ASSERT(!it.empty());
      if (removeInternal && !it.empty() && it.at(0) == '_') {
        continue;
      }
      result.add(VPackValue(it));
    }
    result.close();

    return AqlValue(result);
  } 
   
  std::unordered_set<std::string> keys;
  VPackCollection::keys(slice, keys);

  VPackBuilder result;
  result.openArray();
  for (auto const& it : keys) {
    if (removeInternal && !it.empty() && it.at(0) == '_') {
      continue;
    }
    result.add(VPackValue(it));
  }
  result.close();
  return AqlValue(result);
}

/// @brief function VALUES
AqlValue Functions::Values(arangodb::aql::Query* query,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 1) {
    // no parameters
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  if (!value.isObject()) {
    // not an object
    RegisterWarning(query, "VALUES",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  bool const removeInternal = GetBooleanParameter(trx, parameters, 1, false);

  TRI_ASSERT(value.isObject());
  if (value.length() == 0) {
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);
  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (auto const& entry : VPackObjectIterator(slice, true)) {
    if (!entry.key.isString()) {
      // somehow invalid
      continue;
    }
    if (removeInternal) {
      VPackValueLength l;
      char const* p = entry.key.getString(l);
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

  return AqlValue(builder.get());
}

/// @brief function MIN
AqlValue Functions::Min(arangodb::aql::Query* query,
                        transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "MIN", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);

  VPackSlice minValue;
  auto options = trx->transactionContextPtr()->getVPackOptions();
  for (auto const& it : VPackArrayIterator(slice)) {
    if (it.isNull()) {
      continue;
    }
    if (minValue.isNone() || arangodb::basics::VelocyPackHelper::compare(it, minValue, true, options) < 0) {
      minValue = it;
    }
  }
  if (minValue.isNone()) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
  return AqlValue(minValue);
}

/// @brief function MAX
AqlValue Functions::Max(arangodb::aql::Query* query,
                        transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "MAX", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);
  VPackSlice maxValue;
  auto options = trx->transactionContextPtr()->getVPackOptions();
  for (auto const& it : VPackArrayIterator(slice)) {
    if (maxValue.isNone() || arangodb::basics::VelocyPackHelper::compare(it, maxValue, true, options) > 0) {
      maxValue = it;
    }
  }
  if (maxValue.isNone()) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
  return AqlValue(maxValue);
}

/// @brief function SUM
AqlValue Functions::Sum(arangodb::aql::Query* query,
                        transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "SUM", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);
  double sum = 0.0;
  for (auto const& it : VPackArrayIterator(slice)) {
    if (it.isNull()) {
      continue;
    }
    if (!it.isNumber()) {
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    }
    double const number = it.getNumericValue<double>();

    if (!std::isnan(number) && number != HUGE_VAL && number != -HUGE_VAL) {
      sum += number;
    }
  }

  return NumberValue(trx, sum, false);
}

/// @brief function AVERAGE
AqlValue Functions::Average(arangodb::aql::Query* query,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "AVERAGE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
  
  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);

  double sum = 0.0;
  size_t count = 0;
  for (auto const& v : VPackArrayIterator(slice)) {
    if (v.isNull()) {
      continue;
    }
    if (!v.isNumber()) {
      RegisterWarning(query, "AVERAGE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    }

    // got a numeric value
    double const number = v.getNumericValue<double>();

    if (!std::isnan(number) && number != HUGE_VAL && number != -HUGE_VAL) {
      sum += number;
      ++count;
    }
  }

  if (count > 0 && !std::isnan(sum) && sum != HUGE_VAL && sum != -HUGE_VAL) {
    return NumberValue(trx, sum / static_cast<size_t>(count), false);
  } 

  return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
}

/// @brief function SLEEP
AqlValue Functions::Sleep(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isNumber() || value.toDouble(trx) < 0) {
    RegisterWarning(query, "SLEEP",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
  
  double const until = TRI_microtime() + value.toDouble(trx);

  while (TRI_microtime() < until) {
    usleep(25000);

    if (query->killed()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
    } else if (application_features::ApplicationServer::isStopping()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }
  }
  return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
}

/// @brief function RANDOM_TOKEN
AqlValue Functions::RandomToken(arangodb::aql::Query* query,
                                transaction::Methods* trx,
                                VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  int64_t const length = value.toInt64(trx);
  if (length <= 0 || length > 65536) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "RANDOM_TOKEN");
  }

  UniformCharacter JSNumGenerator("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
  return AqlValue(JSNumGenerator.random(static_cast<size_t>(length)));
}

/// @brief function MD5
AqlValue Functions::Md5(arangodb::aql::Query* query,
                        transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  AppendAsString(trx, adapter, value);

  // create md5
  char hash[17];
  char* p = &hash[0];
  size_t length;

  arangodb::rest::SslInterface::sslMD5(buffer->c_str(), buffer->length(), p,
                                       length);

  // as hex
  char hex[33];
  p = &hex[0];

  arangodb::rest::SslInterface::sslHEX(hash, 16, p, length);

  return AqlValue(&hex[0], 32);
}

/// @brief function SHA1
AqlValue Functions::Sha1(arangodb::aql::Query* query,
                         transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  AppendAsString(trx, adapter, value);

  // create sha1
  char hash[21];
  char* p = &hash[0];
  size_t length;

  arangodb::rest::SslInterface::sslSHA1(buffer->c_str(), buffer->length(), p,
                                        length);

  // as hex
  char hex[41];
  p = &hex[0];

  arangodb::rest::SslInterface::sslHEX(hash, 20, p, length);

  return AqlValue(&hex[0], 40);
}

/// @brief function HASH
AqlValue Functions::Hash(arangodb::aql::Query* query,
                         transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  // throw away the top bytes so the hash value can safely be used
  // without precision loss when storing in JavaScript etc.
  uint64_t hash = value.hash(trx) & 0x0007ffffffffffffULL;

  return AqlValue(hash);
}

/// @brief function UNIQUE
AqlValue Functions::Unique(arangodb::aql::Query* query,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "UNIQUE", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "UNIQUE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);

  auto options = trx->transactionContextPtr()->getVPackOptions();
  std::unordered_set<VPackSlice, arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      values(512, arangodb::basics::VelocyPackHelper::VPackHash(),
             arangodb::basics::VelocyPackHelper::VPackEqual(options));

  for (auto const& s : VPackArrayIterator(slice)) {
    if (!s.isNone()) {
      values.emplace(s);
    }
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (auto const& it : values) {
    builder->add(it);
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function SORTED_UNIQUE
AqlValue Functions::SortedUnique(arangodb::aql::Query* query,
                                 transaction::Methods* trx,
                                 VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "SORTED_UNIQUE", 1, 1);
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    // this is an internal function - do NOT issue a warning here
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
  
  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);

  arangodb::basics::VelocyPackHelper::VPackLess<true> less(trx->transactionContext()->getVPackOptions(), &slice, &slice);
  std::set<VPackSlice, arangodb::basics::VelocyPackHelper::VPackLess<true>> values(less);
  for (auto const& it : VPackArrayIterator(slice)) {
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
  return AqlValue(builder.get());
}

/// @brief function UNION
AqlValue Functions::Union(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "UNION", 2);

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  size_t const n = parameters.size();
  for (size_t i = 0; i < n; ++i) {
    AqlValue value = ExtractFunctionParameterValue(trx, parameters, i);

    if (!value.isArray()) {
      // not an array
      RegisterInvalidArgumentWarning(query, "UNION");
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    }

    TRI_IF_FAILURE("AqlFunctions::OutOfMemory1") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    AqlValueMaterializer materializer(trx);
    VPackSlice slice = materializer.slice(value, false);

    // this passes ownership for the JSON contens into result
    for (auto const& it : VPackArrayIterator(slice)) {
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

  return AqlValue(builder.get());
}

/// @brief function UNION_DISTINCT
AqlValue Functions::UnionDistinct(arangodb::aql::Query* query,
                                  transaction::Methods* trx,
                                  VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "UNION_DISTINCT", 2);
  size_t const n = parameters.size();

  auto options = trx->transactionContextPtr()->getVPackOptions();
  std::unordered_set<VPackSlice, arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      values(512, arangodb::basics::VelocyPackHelper::VPackHash(),
             arangodb::basics::VelocyPackHelper::VPackEqual(options));

  std::vector<AqlValueMaterializer> materializers;
  materializers.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    AqlValue value = ExtractFunctionParameterValue(trx, parameters, i);

    if (!value.isArray()) {
      // not an array
      RegisterInvalidArgumentWarning(query, "UNION_DISTINCT");
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    }

    materializers.emplace_back(trx);
    VPackSlice slice = materializers.back().slice(value, false);

    for (auto const& v : VPackArrayIterator(slice)) {
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

  return AqlValue(builder.get());
}

/// @brief function INTERSECTION
AqlValue Functions::Intersection(arangodb::aql::Query* query,
                                 transaction::Methods* trx,
                                 VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "INTERSECTION", 2);

  auto options = trx->transactionContextPtr()->getVPackOptions();
  std::unordered_map<VPackSlice, size_t,
                     arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      values(512, arangodb::basics::VelocyPackHelper::VPackHash(),
             arangodb::basics::VelocyPackHelper::VPackEqual(options));

  size_t const n = parameters.size();
  std::vector<AqlValueMaterializer> materializers;
  materializers.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    AqlValue value = ExtractFunctionParameterValue(trx, parameters, i);

    if (!value.isArray()) {
      // not an array
      RegisterWarning(query, "INTERSECTION", TRI_ERROR_QUERY_ARRAY_EXPECTED);
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    }
    
    materializers.emplace_back(trx);
    VPackSlice slice = materializers.back().slice(value, false);

    for (auto const& it : VPackArrayIterator(slice)) {
      if (i == 0) {
        // round one

        TRI_IF_FAILURE("AqlFunctions::OutOfMemory1") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        values.emplace(it, 1);
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
  return AqlValue(builder.get());
}

/// @brief function OUTERSECTION
AqlValue Functions::Outersection(arangodb::aql::Query* query,
                                 transaction::Methods* trx,
                                 VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "OUTERSECTION", 2);

  auto options = trx->transactionContextPtr()->getVPackOptions();
  std::unordered_map<VPackSlice, size_t,
                     arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      values(512, arangodb::basics::VelocyPackHelper::VPackHash(),
             arangodb::basics::VelocyPackHelper::VPackEqual(options));

  size_t const n = parameters.size();
  std::vector<AqlValueMaterializer> materializers;
  materializers.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    AqlValue value = ExtractFunctionParameterValue(trx, parameters, i);

    if (!value.isArray()) {
      // not an array
      RegisterWarning(query, "OUTERSECTION", TRI_ERROR_QUERY_ARRAY_EXPECTED);
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    }
    
    materializers.emplace_back(trx);
    VPackSlice slice = materializers.back().slice(value, false);

    for (auto const& it : VPackArrayIterator(slice)) {
      // check if we have seen the same element before
      auto found = values.find(it);
      if (found != values.end()) {
        // already seen
        TRI_ASSERT((*found).second > 0);
        ++(found->second);
      } else {
        values.emplace(it, 1);
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
  return AqlValue(builder.get());
}

/// @brief function DISTANCE
AqlValue Functions::Distance(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "DISTANCE", 4, 4);

  AqlValue lat1 = ExtractFunctionParameterValue(trx, parameters, 0);
  AqlValue lon1 = ExtractFunctionParameterValue(trx, parameters, 1);
  AqlValue lat2 = ExtractFunctionParameterValue(trx, parameters, 2);
  AqlValue lon2 = ExtractFunctionParameterValue(trx, parameters, 3);

  // non-numeric input...
  if (!lat1.isNumber() || !lon1.isNumber() || !lat2.isNumber() || !lon2.isNumber()) {
    RegisterWarning(query, "DISTANCE",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  bool failed;
  bool error = false;
  double lat1Value = lat1.toDouble(trx, failed);
  error |= failed;
  double lon1Value = lon1.toDouble(trx, failed);
  error |= failed;
  double lat2Value = lat2.toDouble(trx, failed);
  error |= failed;
  double lon2Value = lon2.toDouble(trx, failed);
  error |= failed;
    
  if (error) {
    RegisterWarning(query, "DISTANCE",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  auto toRadians = [](double degrees) -> double {
    return degrees * (std::acos(-1.0) / 180.0);
  };

  double p1 = toRadians(lat1Value); 
  double p2 = toRadians(lat2Value); 
  double d1 = toRadians(lat2Value - lat1Value);
  double d2 = toRadians(lon2Value - lon1Value);

  double a = std::sin(d1 / 2.0) * std::sin(d1 / 2.0) +
             std::cos(p1) * std::cos(p2) *
             std::sin(d2 / 2.0) * std::sin(d2 / 2.0);

  double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
  double const EARTHRADIAN = 6371000.0; // metres

  return NumberValue(trx, EARTHRADIAN * c, true);
}

/// @brief function FLATTEN
AqlValue Functions::Flatten(arangodb::aql::Query* query,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "FLATTEN", 1, 2);

  AqlValue list = ExtractFunctionParameterValue(trx, parameters, 0);
  if (!list.isArray()) {
    RegisterWarning(query, "FLATTEN", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  size_t maxDepth = 1;
  if (parameters.size() == 2) {
    AqlValue maxDepthValue = ExtractFunctionParameterValue(trx, parameters, 1);
    bool failed;
    double tmpMaxDepth = maxDepthValue.toDouble(trx, failed);
    if (failed || tmpMaxDepth < 1) {
      maxDepth = 1;
    } else {
      maxDepth = static_cast<size_t>(tmpMaxDepth);
    }
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice listSlice = materializer.slice(list, false);

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  FlattenList(listSlice, maxDepth, 0, *builder.get());
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function ZIP
AqlValue Functions::Zip(arangodb::aql::Query* query,
                        transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "ZIP", 2, 2);

  AqlValue keys = ExtractFunctionParameterValue(trx, parameters, 0);
  AqlValue values = ExtractFunctionParameterValue(trx, parameters, 1);

  if (!keys.isArray() || !values.isArray() ||
      keys.length() != values.length()) {
    RegisterWarning(query, "ZIP",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  VPackValueLength n = keys.length();

  AqlValueMaterializer keyMaterializer(trx);
  VPackSlice keysSlice = keyMaterializer.slice(keys, false);
  
  AqlValueMaterializer valueMaterializer(trx);
  VPackSlice valuesSlice = valueMaterializer.slice(values, false);

  transaction::BuilderLeaser builder(trx);
  builder->openObject();

  // Buffer will temporarily hold the keys
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());
  for (VPackValueLength i = 0; i < n; ++i) {
    buffer->reset();
    Stringify(trx, adapter, keysSlice.at(i));
    builder->add(buffer->c_str(), buffer->length(), valuesSlice.at(i));
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function JSON_STRINGIFY
AqlValue Functions::JsonStringify(arangodb::aql::Query* query,
                                  transaction::Methods* trx,
                                  VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "JSON_STRINGIFY", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);
    
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  VPackDumper dumper(&adapter, trx->transactionContextPtr()->getVPackOptions());
  dumper.dump(slice);
    
  return AqlValue(buffer->begin(), buffer->length());
}

/// @brief function JSON_PARSE
AqlValue Functions::JsonParse(arangodb::aql::Query* query,
                              transaction::Methods* trx,
                              VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "JSON_PARSE", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);
   
  if (!slice.isString()) { 
    RegisterWarning(query, "JSON_PARSE",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  VPackValueLength l;
  char const* p = slice.getString(l);

  try {
    std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
    return AqlValue(*builder);
  } catch (...) {
    RegisterWarning(query, "JSON_PARSE",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
}

/// @brief function PARSE_IDENTIFIER
AqlValue Functions::ParseIdentifier(
    arangodb::aql::Query* query, transaction::Methods* trx,
    VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "PARSE_IDENTIFIER", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  std::string identifier;
  if (value.isObject() && value.hasKey(trx, StaticStrings::IdString)) {
    bool localMustDestroy;
    AqlValue s = value.get(trx, StaticStrings::IdString, localMustDestroy, false);
    AqlValueGuard guard(s, localMustDestroy);

    if (s.isString()) {
      identifier = s.slice().copyString();
    }
  } else if (value.isString()) {
    identifier = value.slice().copyString();
  }

  if (identifier.empty()) {
    RegisterWarning(query, "PARSE_IDENTIFIER",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  std::vector<std::string> parts =
      arangodb::basics::StringUtils::split(identifier, "/");

  if (parts.size() != 2) {
    RegisterWarning(query, "PARSE_IDENTIFIER",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  transaction::BuilderLeaser builder(trx);
  builder->openObject();
  builder->add("collection", VPackValue(parts[0]));
  builder->add("key", VPackValue(parts[1]));
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function Slice
AqlValue Functions::Slice(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "SLICE", 2, 3);

  AqlValue baseArray = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!baseArray.isArray()) {
    RegisterWarning(query, "SLICE",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
 
  // determine lower bound 
  AqlValue fromValue = ExtractFunctionParameterValue(trx, parameters, 1);
  int64_t from = fromValue.toInt64(trx);
  if (from < 0) {
    from = baseArray.length() + from;
    if (from < 0) {
      from = 0;
    }
  }
  
  // determine upper bound
  AqlValue toValue = ExtractFunctionParameterValue(trx, parameters, 2);
  int64_t to;
  if (toValue.isNull(true)) {
    to = baseArray.length();
  } else {
    to = toValue.toInt64(trx);
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

  AqlValueMaterializer materializer(trx);
  VPackSlice arraySlice = materializer.slice(baseArray, false);

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
  return AqlValue(builder.get());
}

/// @brief function Minus
AqlValue Functions::Minus(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "MINUS", 2);

  AqlValue baseArray = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!baseArray.isArray()) {
    RegisterWarning(query, "MINUS",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  auto options = trx->transactionContextPtr()->getVPackOptions();
  std::unordered_map<VPackSlice, size_t,
                     arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      contains(512, arangodb::basics::VelocyPackHelper::VPackHash(),
               arangodb::basics::VelocyPackHelper::VPackEqual(options));

  // Fill the original map
  AqlValueMaterializer materializer(trx);
  VPackSlice arraySlice = materializer.slice(baseArray, false);
  
  VPackArrayIterator it(arraySlice);
  while (it.valid()) {
    contains.emplace(it.value(), it.index());
    it.next();
  }

  // Iterate through all following parameters and delete found elements from the
  // map
  for (size_t k = 1; k < parameters.size(); ++k) {
    AqlValue next = ExtractFunctionParameterValue(trx, parameters, k);
    if (!next.isArray()) {
      RegisterWarning(query, "MINUS",
                      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    }
  
    AqlValueMaterializer materializer(trx);
    VPackSlice arraySlice = materializer.slice(next, false);

    for (auto const& search : VPackArrayIterator(arraySlice)) {
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
  return AqlValue(builder.get());
}

/// @brief function Document
AqlValue Functions::Document(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "DOCUMENT", 1, 2);

  if (parameters.size() == 1) {
    AqlValue id = ExtractFunctionParameterValue(trx, parameters, 0);
    transaction::BuilderLeaser builder(trx);
    if (id.isString()) {
      std::string identifier(id.slice().copyString());
      std::string colName;
      GetDocumentByIdentifier(trx, colName, identifier, true, *builder.get());
      if (builder->isEmpty()) {
        // not found
        return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
      }
      return AqlValue(builder.get());
    } 
    if (id.isArray()) {
      AqlValueMaterializer materializer(trx);
      VPackSlice idSlice = materializer.slice(id, false);
      builder->openArray();
      for (auto const& next : VPackArrayIterator(idSlice)) {
        if (next.isString()) {
          std::string identifier = next.copyString();
          std::string colName;
          GetDocumentByIdentifier(trx, colName, identifier, true, *builder.get());
        }
      }
      builder->close();
      return AqlValue(builder.get());
    } 
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  AqlValue collectionValue = ExtractFunctionParameterValue(trx, parameters, 0);
  if (!collectionValue.isString()) {
    RegisterWarning(query, "DOCUMENT",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
  std::string collectionName(collectionValue.slice().copyString());

  AqlValue id = ExtractFunctionParameterValue(trx, parameters, 1);
  if (id.isString()) {
    transaction::BuilderLeaser builder(trx);
    std::string identifier(id.slice().copyString());
    GetDocumentByIdentifier(trx, collectionName, identifier, true, *builder.get());
    if (builder->isEmpty()) {
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    }
    return AqlValue(builder.get());
  }
   
  if (id.isArray()) {
    transaction::BuilderLeaser builder(trx);
    builder->openArray();

    AqlValueMaterializer materializer(trx);
    VPackSlice idSlice = materializer.slice(id, false);
    for (auto const& next : VPackArrayIterator(idSlice)) {
      if (next.isString()) {
        std::string identifier(next.copyString());
        GetDocumentByIdentifier(trx, collectionName, identifier, true, *builder.get());
      }
    }

    builder->close();
    return AqlValue(builder.get());
  }

  // Id has invalid format
  return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
}

/// @brief function ROUND
AqlValue Functions::Round(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "ROUND", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  double input = value.toDouble(trx);

  // Rounds down for < x.4999 and up for > x.50000
  return NumberValue(trx, std::floor(input + 0.5), true);  
}

/// @brief function ABS
AqlValue Functions::Abs(arangodb::aql::Query* query,
                        transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "ABS", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  double input = value.toDouble(trx);
  return NumberValue(trx, std::abs(input), true);  
}

/// @brief function CEIL
AqlValue Functions::Ceil(arangodb::aql::Query* query,
                         transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "CEIL", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  double input = value.toDouble(trx);
  return NumberValue(trx, std::ceil(input), true);  
}

/// @brief function FLOOR
AqlValue Functions::Floor(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "FLOOR", 1, 1);
  
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  double input = value.toDouble(trx);
  return NumberValue(trx, std::floor(input), true);  
}

/// @brief function SQRT
AqlValue Functions::Sqrt(arangodb::aql::Query* query,
                         transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "SQRT", 1, 1);
  
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  
  double input = value.toDouble(trx);
  return NumberValue(trx, std::sqrt(input), true);  
}

/// @brief function POW
AqlValue Functions::Pow(arangodb::aql::Query* query,
                        transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "POW", 2, 2);

  AqlValue baseValue = ExtractFunctionParameterValue(trx, parameters, 0);
  AqlValue expValue = ExtractFunctionParameterValue(trx, parameters, 1);

  double base = baseValue.toDouble(trx);
  double exp = expValue.toDouble(trx);

  return NumberValue(trx, std::pow(base, exp), true);
}

/// @brief function LOG
AqlValue Functions::Log(arangodb::aql::Query* query,
                        transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "LOG", 1, 1);
  
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  
  double input = value.toDouble(trx);
  return NumberValue(trx, std::log(input), true);  
}

/// @brief function LOG2
AqlValue Functions::Log2(arangodb::aql::Query* query,
                         transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "LOG2", 1, 1);
  
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  
  double input = value.toDouble(trx);
  return NumberValue(trx, std::log2(input), true);  
}

/// @brief function LOG10
AqlValue Functions::Log10(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "LOG10", 1, 1);
  
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  
  double input = value.toDouble(trx);
  return NumberValue(trx, std::log10(input), true);  
}

/// @brief function EXP
AqlValue Functions::Exp(arangodb::aql::Query* query,
                        transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "EXP", 1, 1);
  
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  
  double input = value.toDouble(trx);
  return NumberValue(trx, std::exp(input), true);  
}

/// @brief function EXP2
AqlValue Functions::Exp2(arangodb::aql::Query* query,
                         transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "EXP2", 1, 1);
  
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  
  double input = value.toDouble(trx);
  return NumberValue(trx, std::exp2(input), true);  
}

/// @brief function SIN
AqlValue Functions::Sin(arangodb::aql::Query* query,
                        transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "SIN", 1, 1);
  
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  
  double input = value.toDouble(trx);
  return NumberValue(trx, std::sin(input), true);  
}

/// @brief function COS
AqlValue Functions::Cos(arangodb::aql::Query* query,
                        transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "COS", 1, 1);
  
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  
  double input = value.toDouble(trx);
  return NumberValue(trx, std::cos(input), true);  
}

/// @brief function TAN
AqlValue Functions::Tan(arangodb::aql::Query* query,
                        transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "TAN", 1, 1);
  
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  
  double input = value.toDouble(trx);
  return NumberValue(trx, std::tan(input), true);  
}

/// @brief function ASIN
AqlValue Functions::Asin(arangodb::aql::Query* query,
                         transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "ASIN", 1, 1);
  
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  
  double input = value.toDouble(trx);
  return NumberValue(trx, std::asin(input), true);  
}

/// @brief function ACOS
AqlValue Functions::Acos(arangodb::aql::Query* query,
                         transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "ACOS", 1, 1);
  
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  
  double input = value.toDouble(trx);
  return NumberValue(trx, std::acos(input), true);  
}

/// @brief function ATAN
AqlValue Functions::Atan(arangodb::aql::Query* query,
                         transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "ATAN", 1, 1);
  
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  
  double input = value.toDouble(trx);
  return NumberValue(trx, std::atan(input), true);  
}

/// @brief function ATAN2
AqlValue Functions::Atan2(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "ATAN2", 2, 2);
  
  AqlValue value1 = ExtractFunctionParameterValue(trx, parameters, 0);
  AqlValue value2 = ExtractFunctionParameterValue(trx, parameters, 1);
  
  double input1 = value1.toDouble(trx);
  double input2 = value2.toDouble(trx);
  return NumberValue(trx, std::atan2(input1, input2), true);  
}

/// @brief function RADIANS
AqlValue Functions::Radians(arangodb::aql::Query* query,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "RADIANS", 1, 1);
  
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  
  double degrees = value.toDouble(trx);
  // acos(-1) == PI
  return NumberValue(trx, degrees * (std::acos(-1.0) / 180.0), true);
}

/// @brief function DEGREES
AqlValue Functions::Degrees(arangodb::aql::Query* query,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "DEGREES", 1, 1);
  
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  
  double radians = value.toDouble(trx);
  // acos(-1) == PI
  return NumberValue(trx, radians * (180.0 / std::acos(-1.0)), true);
}

/// @brief function PI
AqlValue Functions::Pi(arangodb::aql::Query* query,
                       transaction::Methods* trx,
                       VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "PI", 0, 0);
  
  // acos(-1) == PI
  return NumberValue(trx, std::acos(-1.0), true);
}

/// @brief function RAND
AqlValue Functions::Rand(arangodb::aql::Query* query,
                         transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "RAND", 0, 0);

  // This random functionality is not too good yet...
  return NumberValue(trx, static_cast<double>(std::rand()) / RAND_MAX, true);
}

/// @brief function FIRST_DOCUMENT
AqlValue Functions::FirstDocument(arangodb::aql::Query* query,
                                  transaction::Methods* trx,
                                  VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  for (size_t i = 0; i < n; ++i) {
    AqlValue a = ExtractFunctionParameterValue(trx, parameters, i);
    if (a.isObject()) {
      return a.clone();
    }
  }

  return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
}

/// @brief function FIRST_LIST
AqlValue Functions::FirstList(arangodb::aql::Query* query,
                              transaction::Methods* trx,
                              VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  for (size_t i = 0; i < n; ++i) {
    AqlValue a = ExtractFunctionParameterValue(trx, parameters, i);
    if (a.isArray()) {
      return a.clone();
    }
  }

  return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
}

/// @brief function PUSH
AqlValue Functions::Push(arangodb::aql::Query* query,
                         transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "PUSH", 2, 3);
  
  AqlValue list = ExtractFunctionParameterValue(trx, parameters, 0);
  AqlValue toPush = ExtractFunctionParameterValue(trx, parameters, 1);

  AqlValueMaterializer toPushMaterializer(trx);
  VPackSlice p = toPushMaterializer.slice(toPush, false);

  if (list.isNull(true)) {
    transaction::BuilderLeaser builder(trx);
    builder->openArray();
    builder->add(p);
    builder->close();
    return AqlValue(builder.get());
  } 
  
  if (!list.isArray()) {
    RegisterInvalidArgumentWarning(query, "PUSH");
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  AqlValueMaterializer materializer(trx);
  VPackSlice l = materializer.slice(list, false);

  for (auto const& it : VPackArrayIterator(l)) {
    builder->add(it);
  }
  if (parameters.size() == 3) {
    auto options = trx->transactionContextPtr()->getVPackOptions();
    AqlValue unique = ExtractFunctionParameterValue(trx, parameters, 2);
    if (!unique.toBoolean() || !ListContainsElement(options, l, p)) {
      builder->add(p);
    }
  } else {
    builder->add(p);
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function POP
AqlValue Functions::Pop(arangodb::aql::Query* query,
                        transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "POP", 1, 1);
  AqlValue list = ExtractFunctionParameterValue(trx, parameters, 0);

  if (list.isNull(true)) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  if (!list.isArray()) {
    RegisterWarning(query, "POP",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(list, false);

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  auto iterator = VPackArrayIterator(slice);
  while (iterator.valid() && !iterator.isLast()) {
    builder->add(iterator.value());
    iterator.next();
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function APPEND
AqlValue Functions::Append(arangodb::aql::Query* query,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "APPEND", 2, 3);
  AqlValue list = ExtractFunctionParameterValue(trx, parameters, 0);
  AqlValue toAppend = ExtractFunctionParameterValue(trx, parameters, 1);

  if (toAppend.isNull(true)) {
    return list.clone();
  }

  AqlValueMaterializer toAppendMaterializer(trx);
  VPackSlice t = toAppendMaterializer.slice(toAppend, false);
    
  if (t.isArray() && t.length() == 0) {
    return list.clone();
  }

  bool unique = false;
  if (parameters.size() == 3) {
    AqlValue a = ExtractFunctionParameterValue(trx, parameters, 2);
    unique = a.toBoolean();
  }
  
  AqlValueMaterializer materializer(trx);
  VPackSlice l = materializer.slice(list, false);
  
  if (l.isNull()) {
    return toAppend.clone();
  }
    
  if (!l.isArray()) {
    RegisterInvalidArgumentWarning(query, "APPEND");
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
  
  std::unordered_set<VPackSlice> added;

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
      
  for (auto const& it : VPackArrayIterator(l)) {
    if (unique) {
      if (added.find(it) == added.end()) {
        builder->add(it);
        added.emplace(it);
      }
    } else {
      builder->add(it);
    }
  }
    
  AqlValueMaterializer materializer2(trx);
  VPackSlice slice = materializer2.slice(toAppend, false);
  
  if (!slice.isArray()) {
    if (!unique || added.find(slice) == added.end()) {
      builder->add(slice);
    }
  } else {
    for (auto const& it : VPackArrayIterator(slice)) {
      if (unique) {
        if (added.find(it) == added.end()) {
          builder->add(it);
          added.emplace(it);
        }
      } else {
        builder->add(it);
      }
    }
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function UNSHIFT
AqlValue Functions::Unshift(arangodb::aql::Query* query,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "UNSHIFT", 2, 3);
  AqlValue list = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!list.isNull(true) && !list.isArray()) {
    RegisterInvalidArgumentWarning(query, "UNSHIFT");
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  AqlValue toAppend = ExtractFunctionParameterValue(trx, parameters, 1);
  bool unique = false;
  if (parameters.size() == 3) {
    AqlValue a = ExtractFunctionParameterValue(trx, parameters, 2);
    unique = a.toBoolean();
  }

  auto options = trx->transactionContextPtr()->getVPackOptions();
  size_t unused;
  if (unique && list.isArray() &&
      ListContainsElement(trx, options, list, toAppend, unused)) {
    // Short circuit, nothing to do return list
    return list.clone();
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice a = materializer.slice(toAppend, false);

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  builder->add(a);
    
  if (list.isArray()) {
    AqlValueMaterializer materializer(trx);
    VPackSlice v = materializer.slice(list, false);
    for (auto const& it : VPackArrayIterator(v)) {
      builder->add(it);
    }
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function SHIFT
AqlValue Functions::Shift(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "SHIFT", 1, 1);
  
  AqlValue list = ExtractFunctionParameterValue(trx, parameters, 0);
  if (list.isNull(true)) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  if (!list.isArray()) {
    RegisterInvalidArgumentWarning(query, "SHIFT");
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  
  if (list.length() > 0) {
    AqlValueMaterializer materializer(trx);
    VPackSlice l = materializer.slice(list, false);

    auto iterator = VPackArrayIterator(l);
    // This jumps over the first element
    iterator.next();
    while (iterator.valid()) {
      builder->add(iterator.value());
      iterator.next();
    }
  }
  builder->close();

  return AqlValue(builder.get());
}

/// @brief function REMOVE_VALUE
AqlValue Functions::RemoveValue(arangodb::aql::Query* query,
                                transaction::Methods* trx,
                                VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "REMOVE_VALUE", 2, 3);

  AqlValue list = ExtractFunctionParameterValue(trx, parameters, 0);

  if (list.isNull(true)) {
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }

  if (!list.isArray()) {
    RegisterInvalidArgumentWarning(query, "REMOVE_VALUE");
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  auto options = trx->transactionContextPtr()->getVPackOptions();
  
  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  bool useLimit = false;
  int64_t limit = list.length();

  if (parameters.size() == 3) {
    AqlValue limitValue = ExtractFunctionParameterValue(trx, parameters, 2);
    if (!limitValue.isNull(true)) {
      limit = limitValue.toInt64(trx);
      useLimit = true;
    }
  }
  
  AqlValue toRemove = ExtractFunctionParameterValue(trx, parameters, 1);
  AqlValueMaterializer toRemoveMaterializer(trx);
  VPackSlice r = toRemoveMaterializer.slice(toRemove, false);

  AqlValueMaterializer materializer(trx);
  VPackSlice v = materializer.slice(list, false);

  for (auto const& it : VPackArrayIterator(v)) {
    if (useLimit && limit == 0) {
      // Just copy
      builder->add(it);
      continue;
    }
    if (arangodb::basics::VelocyPackHelper::compare(r, it, false, options) == 0) {
      --limit;
      continue;
    }
    builder->add(it);
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function REMOVE_VALUES
AqlValue Functions::RemoveValues(arangodb::aql::Query* query,
                                 transaction::Methods* trx,
                                 VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "REMOVE_VALUES", 2, 2);
  
  AqlValue list = ExtractFunctionParameterValue(trx, parameters, 0);
  AqlValue values = ExtractFunctionParameterValue(trx, parameters, 1);

  if (values.isNull(true)) {
    return list.clone();
  }

  if (list.isNull(true)) {
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }

  if (!list.isArray() || !values.isArray()) {
    RegisterInvalidArgumentWarning(query, "REMOVE_VALUES");
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
  
  auto options = trx->transactionContextPtr()->getVPackOptions();
  AqlValueMaterializer valuesMaterializer(trx);
  VPackSlice v = valuesMaterializer.slice(values, false);

  AqlValueMaterializer listMaterializer(trx);
  VPackSlice l = listMaterializer.slice(list, false);

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (auto const& it : VPackArrayIterator(l)) {
    if (!ListContainsElement(options, v, it)) {
      builder->add(it);
    }
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function REMOVE_NTH
AqlValue Functions::RemoveNth(arangodb::aql::Query* query,
                              transaction::Methods* trx,
                              VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "REMOVE_NTH", 2, 2);

  AqlValue list = ExtractFunctionParameterValue(trx, parameters, 0);

  if (list.isNull(true)) {
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }

  if (!list.isArray()) {
    RegisterInvalidArgumentWarning(query, "REMOVE_NTH");
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  double const count = static_cast<double>(list.length());
  AqlValue position = ExtractFunctionParameterValue(trx, parameters, 1);
  double p = position.toDouble(trx);
  if (p >= count || p < -count) {
    // out of bounds
    return list.clone();
  }

  if (p < 0) {
    p += count;
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice v = materializer.slice(list, false);

  transaction::BuilderLeaser builder(trx);
  size_t target = static_cast<size_t>(p);
  size_t cur = 0;
  builder->openArray();
  for (auto const& it : VPackArrayIterator(v)) {
    if (cur != target) {
      builder->add(it);
    }
    cur++;
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function NOT_NULL
AqlValue Functions::NotNull(arangodb::aql::Query* query,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  for (size_t i = 0; i < n; ++i) {
    AqlValue element = ExtractFunctionParameterValue(trx, parameters, i);
    if (!element.isNull(true)) {
      return element.clone();
    }
  }
  return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
}

/// @brief function CURRENT_DATABASE
AqlValue Functions::CurrentDatabase(
    arangodb::aql::Query* query, transaction::Methods* trx,
    VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "CURRENT_DATABASE", 0, 0);

  transaction::BuilderLeaser builder(trx);
  builder->add(VPackValue(query->vocbase()->name()));
  return AqlValue(builder.get());
}

/// @brief function COLLECTION_COUNT
AqlValue Functions::CollectionCount(
    arangodb::aql::Query* query, transaction::Methods* trx,
    VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "COLLECTION_COUNT", 1, 1);

  AqlValue element = ExtractFunctionParameterValue(trx, parameters, 0);
  if (!element.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "COLLECTION_COUNT");
  }

  std::string const collectionName(element.slice().copyString());

  auto resolver = trx->resolver();
  TRI_voc_cid_t cid = resolver->getCollectionIdLocal(collectionName);
  trx->addCollectionAtRuntime(cid, collectionName);
  auto collection = trx->documentCollection(cid);

  if (collection == nullptr) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                  "'%s'", collectionName.c_str());
  }

  transaction::BuilderLeaser builder(trx);
  builder->add(VPackValue(collection->numberDocuments(trx)));
  return AqlValue(builder.get());
}

/// @brief function VARIANCE_SAMPLE
AqlValue Functions::VarianceSample(
    arangodb::aql::Query* query, transaction::Methods* trx,
    VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "VARIANCE_SAMPLE", 1, 1);

  AqlValue list = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!list.isArray()) {
    RegisterWarning(query, "VARIANCE_SAMPLE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  double value = 0.0;
  size_t count = 0;

  if (!Variance(trx, list, value, count)) {
    RegisterWarning(query, "VARIANCE_SAMPLE",
                    TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  if (count < 2) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  return NumberValue(trx, value / (count - 1), true);
}

/// @brief function VARIANCE_POPULATION
AqlValue Functions::VariancePopulation(
    arangodb::aql::Query* query, transaction::Methods* trx,
    VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "VARIANCE_POPULATION", 1, 1);

  AqlValue list = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!list.isArray()) {
    RegisterWarning(query, "VARIANCE_POPULATION",
                    TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  double value = 0.0;
  size_t count = 0;

  if (!Variance(trx, list, value, count)) {
    RegisterWarning(query, "VARIANCE_POPULATION",
                    TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  if (count < 1) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  return NumberValue(trx, value / count, true);
}

/// @brief function STDDEV_SAMPLE
AqlValue Functions::StdDevSample(
    arangodb::aql::Query* query, transaction::Methods* trx,
    VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "STDDEV_SAMPLE", 1, 1);

  AqlValue list = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!list.isArray()) {
    RegisterWarning(query, "STDDEV_SAMPLE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  double value = 0.0;
  size_t count = 0;

  if (!Variance(trx, list, value, count)) {
    RegisterWarning(query, "STDDEV_SAMPLE",
                    TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  if (count < 2) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  return NumberValue(trx, std::sqrt(value / (count - 1)), true);
}

/// @brief function STDDEV_POPULATION
AqlValue Functions::StdDevPopulation(
    arangodb::aql::Query* query, transaction::Methods* trx,
    VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "STDDEV_POPULATION", 1, 1);

  AqlValue list = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!list.isArray()) {
    RegisterWarning(query, "STDDEV_POPULATION", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  double value = 0.0;
  size_t count = 0;

  if (!Variance(trx, list, value, count)) {
    RegisterWarning(query, "STDDEV_POPULATION",
                    TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  if (count < 1) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  return NumberValue(trx, std::sqrt(value / count), true);
}

/// @brief function MEDIAN
AqlValue Functions::Median(arangodb::aql::Query* query,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "MEDIAN", 1, 1);

  AqlValue list = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!list.isArray()) {
    RegisterWarning(query, "MEDIAN", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  std::vector<double> values;
  if (!SortNumberList(trx, list, values)) {
    RegisterWarning(query, "MEDIAN", TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  if (values.empty()) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
  size_t const l = values.size();
  size_t midpoint = l / 2;

  if (l % 2 == 0) {
    return NumberValue(trx, (values[midpoint - 1] + values[midpoint]) / 2, true);
  }
  return NumberValue(trx, values[midpoint], true);
}

/// @brief function PERCENTILE
AqlValue Functions::Percentile(arangodb::aql::Query* query,
                               transaction::Methods* trx,
                               VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "PERCENTILE", 2, 3);

  AqlValue list = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!list.isArray()) {
    RegisterWarning(query, "PERCENTILE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  AqlValue border = ExtractFunctionParameterValue(trx, parameters, 1);

  if (!border.isNumber()) {
    RegisterWarning(query, "PERCENTILE",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  bool unused = false;
  double p = border.toDouble(trx, unused);
  if (p <= 0.0 || p > 100.0) {
    RegisterWarning(query, "PERCENTILE",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  bool useInterpolation = false;

  if (parameters.size() == 3) {
    AqlValue methodValue = ExtractFunctionParameterValue(trx, parameters, 2);
    if (!methodValue.isString()) {
      RegisterWarning(query, "PERCENTILE",
                      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    }
    std::string method = methodValue.slice().copyString();
    if (method == "interpolation") {
      useInterpolation = true;
    } else if (method == "rank") {
      useInterpolation = false;
    } else {
      RegisterWarning(query, "PERCENTILE",
                      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    }
  }

  std::vector<double> values;
  if (!SortNumberList(trx, list, values)) {
    RegisterWarning(query, "PERCENTILE",
                    TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  if (values.empty()) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  size_t l = values.size();
  if (l == 1) {
    return NumberValue(trx, values[0], true);
  }

  TRI_ASSERT(l > 1);

  if (useInterpolation) {
    double const idx = p * (l + 1) / 100.0;
    double const pos = floor(idx);

    if (pos >= l) {
      return NumberValue(trx, values[l - 1], true);
    } 
    if (pos <= 0) {
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    } 
    
    double const delta = idx - pos;
    return NumberValue(trx, delta * (values[static_cast<size_t>(pos)] -
                                     values[static_cast<size_t>(pos) - 1]) +
                                  values[static_cast<size_t>(pos) - 1], true);
  }

  double const idx = p * l / 100.0;
  double const pos = ceil(idx);
  if (pos >= l) {
    return NumberValue(trx, values[l - 1], true);
  } 
  if (pos <= 0) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  } 
    
  return NumberValue(trx, values[static_cast<size_t>(pos) - 1], true);
}

/// @brief function RANGE
AqlValue Functions::Range(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "RANGE", 2, 3);

  AqlValue left = ExtractFunctionParameterValue(trx, parameters, 0);
  AqlValue right = ExtractFunctionParameterValue(trx, parameters, 1);

  double from = left.toDouble(trx);
  double to = right.toDouble(trx);

  if (parameters.size() < 3) {
    return AqlValue(left.toInt64(trx), right.toInt64(trx));
  }

  AqlValue stepValue = ExtractFunctionParameterValue(trx, parameters, 2);
  if (stepValue.isNull(true)) {
    // no step specified. return a real range object
    return AqlValue(left.toInt64(trx), right.toInt64(trx));
  } 
  
  double step = stepValue.toDouble(trx);

  if (step == 0.0 || (from < to && step < 0.0) || (from > to && step > 0.0)) {
    RegisterWarning(query, "RANGE",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  if (step < 0.0 && to <= from) {
    for (; from >= to; from += step) {
      builder->add(VPackValue(from));
    }
  } else {
    for (; from <= to; from += step) {
      builder->add(VPackValue(from));
    }
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function POSITION
AqlValue Functions::Position(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "POSITION", 2, 3);

  AqlValue list = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!list.isArray()) {
    RegisterWarning(query, "POSITION", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  bool returnIndex = false;
  if (parameters.size() == 3) {
    AqlValue a = ExtractFunctionParameterValue(trx, parameters, 2);
    returnIndex = a.toBoolean();
  }

  if (list.length() > 0) {
    AqlValue searchValue = ExtractFunctionParameterValue(trx, parameters, 1);
    auto options = trx->transactionContextPtr()->getVPackOptions();

    size_t index;
    if (ListContainsElement(trx, options, list, searchValue, index)) {
      if (!returnIndex) {
        // return true
        return AqlValue(arangodb::basics::VelocyPackHelper::TrueValue());
      }
      // return position
      transaction::BuilderLeaser builder(trx);
      builder->add(VPackValue(index));
      return AqlValue(builder.get());
    } 
  }

  // not found
  if (!returnIndex) {
    // return false
    return AqlValue(arangodb::basics::VelocyPackHelper::FalseValue());
  }

  // return -1
  transaction::BuilderLeaser builder(trx);
  builder->add(VPackValue(-1));
  return AqlValue(builder.get());
}

/// @brief function IS_SAME_COLLECTION
AqlValue Functions::IsSameCollection(
    arangodb::aql::Query* query, transaction::Methods* trx,
    VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "IS_SAME_COLLECTION", 2, 2);

  std::string const first = ExtractCollectionName(trx, parameters, 0);
  std::string const second = ExtractCollectionName(trx, parameters, 1);
  
  if (!first.empty() && !second.empty()) {
    return AqlValue(first == second);
  }

  RegisterWarning(query, "IS_SAME_COLLECTION",
                  TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
}

#include "Pregel/PregelFeature.h"
#include "Pregel/Worker.h"

AqlValue Functions::PregelResult(arangodb::aql::Query* query, transaction::Methods* trx,
                                        VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "PREGEL_RESULT", 1, 1);
  
  AqlValue arg1 = ExtractFunctionParameterValue(trx, parameters, 0);
  if (!arg1.isNumber()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
                                  TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "PREGEL_RESULT");
  }
  
  uint64_t execNr = arg1.toInt64(trx);
  pregel::PregelFeature *feature = pregel::PregelFeature::instance();
  if (feature) {
    std::shared_ptr<pregel::IWorker> worker = feature->worker(execNr);
    if (!worker) {
      RegisterWarning(query, "PREGEL_RESULT",
                      TRI_ERROR_QUERY_FUNCTION_INVALID_CODE);
      return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
    }
    transaction::BuilderLeaser builder(trx);
    worker->aqlResult(builder.get());
    return AqlValue(builder.get());
  } else {
    RegisterWarning(query, "PREGEL_RESULT",
                    TRI_ERROR_QUERY_FUNCTION_INVALID_CODE);
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }
}
