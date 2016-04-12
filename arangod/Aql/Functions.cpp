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
#include "Aql/Function.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "Basics/fpconv.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringBuffer.h"
#include "Basics/tri-strings.h"
#include "Basics/Utf8Helper.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "FulltextIndex/fulltext-index.h"
#include "FulltextIndex/fulltext-result.h"
#include "FulltextIndex/fulltext-query.h"
#include "Indexes/Index.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/FulltextIndex.h"
#include "Indexes/GeoIndex2.h"
#include "Rest/SslInterface.h"
#include "Utils/OperationCursor.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "Utils/Transaction.h"
#include "V8Server/V8Traverser.h"
#include "VocBase/KeyGenerator.h"

#include <velocypack/Collection.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

using VertexId = arangodb::traverser::VertexId;

/// @brief thread-local cache for compiled regexes
thread_local std::unordered_map<std::string, RegexMatcher*>* RegexCache =
    nullptr;

/// @brief convert a number value into an AqlValue
static AqlValue NumberValue(arangodb::AqlTransaction* trx, double value) {
  if (std::isnan(value) || !std::isfinite(value) || value == HUGE_VAL || value == -HUGE_VAL) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
  
  TransactionBuilderLeaser builder(trx);
  builder->add(VPackValue(value));
  return AqlValue(builder.get());
}

/// @brief validate the number of parameters
static void ValidateParameters(std::vector<AqlValue> const& parameters,
                               char const* function, int minParams, int maxParams) {
  if (parameters.size() < static_cast<size_t>(minParams) || 
      parameters.size() > static_cast<size_t>(maxParams)) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, function, minParams, maxParams);
  }
}

static void ValidateParameters(std::vector<AqlValue> const& parameters,
                               char const* function, int minParams) {
  return ValidateParameters(parameters, function, minParams, static_cast<int>(Function::MaxArguments));
}

/// @brief Insert a mptr into the result
static void InsertMasterPointer(TRI_doc_mptr_t const* mptr, VPackBuilder& builder) {
  //builder.add(VPackValue(static_cast<void const*>(mptr->vpack()),
  //                       VPackValueType::External));
  // This is the future, for now we have to copy:
  builder.add(VPackSlice(mptr->vpack()));
}

/// @brief clear the regex cache in a thread
static void ClearRegexCache() {
  if (RegexCache != nullptr) {
    for (auto& it : *RegexCache) {
      delete it.second;
    }
    delete RegexCache;
    RegexCache = nullptr;
  }
}

/// @brief compile a regex pattern from a string
static std::string BuildRegexPattern(char const* ptr, size_t length,
                                     bool caseInsensitive) {
  // pattern is always anchored
  std::string pattern("^");
  if (caseInsensitive) {
    pattern.append("(?i)");
  }

  bool escaped = false;

  for (size_t i = 0; i < length; ++i) {
    char const c = ptr[i];

    if (c == '\\') {
      if (escaped) {
        // literal backslash
        pattern.append("\\\\");
      }
      escaped = !escaped;
    } else {
      if (c == '%') {
        if (escaped) {
          // literal %
          pattern.push_back('%');
        } else {
          // wildcard
          pattern.append("(.|[\r\n])*");
        }
      } else if (c == '_') {
        if (escaped) {
          // literal underscore
          pattern.push_back('_');
        } else {
          // wildcard character
          pattern.append("(.|[\r\n])");
        }
      } else if (c == '?' || c == '+' || c == '[' || c == '(' || c == ')' ||
                 c == '{' || c == '}' || c == '^' || c == '$' || c == '|' ||
                 c == '\\' || c == '.') {
        // character with special meaning in a regex
        pattern.push_back('\\');
        pattern.push_back(c);
      } else {
        if (escaped) {
          // found a backslash followed by no special character
          pattern.append("\\\\");
        }

        // literal character
        pattern.push_back(c);
      }

      escaped = false;
    }
  }

  // always anchor the pattern
  pattern.push_back('$');

  return pattern;
}

/// @brief extract a function parameter from the arguments
static AqlValue ExtractFunctionParameterValue(
    arangodb::AqlTransaction*, VPackFunctionParameters const& parameters,
    size_t position) {
  if (position >= parameters.size()) {
    // parameter out of range
    return AqlValue();
  }
  return parameters.at(position);
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
static bool GetBooleanParameter(arangodb::AqlTransaction* trx,
                                VPackFunctionParameters const& parameters,
                                size_t startParameter, bool defaultValue) {
  size_t const n = parameters.size();

  if (startParameter >= n) {
    return defaultValue;
  }

  return parameters[startParameter].toBoolean();
}

/// @brief extract attribute names from the arguments
static void ExtractKeys(std::unordered_set<std::string>& names,
                        arangodb::aql::Query* query,
                        arangodb::AqlTransaction* trx,
                        VPackFunctionParameters const& parameters,
                        size_t startParameter, char const* functionName) {
  size_t const n = parameters.size();

  for (size_t i = startParameter; i < n; ++i) {
    AqlValue param = ExtractFunctionParameterValue(trx, parameters, i);

    if (param.isString()) {
      names.emplace(param.slice().copyString());
    } else if (param.isNumber()) {
      double number = param.toDouble();

      if (std::isnan(number) || number == HUGE_VAL || number == -HUGE_VAL) {
        names.emplace("null");
      } else {
        char buffer[24];
        int length = fpconv_dtoa(number, &buffer[0]);
        names.emplace(std::string(&buffer[0], static_cast<size_t>(length)));
      }
    } else if (param.isArray()) {
      AqlValueMaterializer materializer(trx);
      VPackSlice s = materializer.slice(param);

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
static void AppendAsString(arangodb::basics::VPackStringBufferAdapter& buffer,
                           VPackSlice const& slice) {
  if (slice.isNull()) {
    buffer.append("null", 4);
    return;
  }

  if (slice.isString()) {
    // dumping adds additional ''
    buffer.append(slice.copyString());
    return;
  }
   
  if (slice.isArray()) {
    bool first = true;
    for (auto const& sub : VPackArrayIterator(slice)) {
      if (!first) {
        buffer.append(",");
      } else {
        first = false;
      }
      AppendAsString(buffer, sub);
    }
    return;
  }

  if (slice.isObject()) {
    buffer.append("[object Object]");
    return;
  } 

  VPackDumper dumper(&buffer);
  dumper.dump(slice);
}

/// @brief append the VelocyPack value to a string buffer
///        Note: Backwards compatibility. Is different than Slice.toJson()
static void AppendAsString(arangodb::AqlTransaction* trx,
                           arangodb::basics::VPackStringBufferAdapter& buffer,
                           AqlValue const& value) {
  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value);

  AppendAsString(buffer, slice);
}

/// @brief Checks if the given list contains the element
static bool ListContainsElement(arangodb::AqlTransaction* trx,
                                AqlValue const& list,
                                AqlValue const& testee, size_t& index) {
  TRI_ASSERT(list.isArray());
  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(list);
  
  AqlValueMaterializer testeeMaterializer(trx);
  VPackSlice testeeSlice = testeeMaterializer.slice(testee);

  VPackArrayIterator it(slice);
  while (it.valid()) {
    if (arangodb::basics::VelocyPackHelper::compare(testeeSlice, it.value(), false) == 0) {
      index = it.index();
      return true;
    }
    it.next();
  }
  return false;
}

/// @brief Checks if the given list contains the element
/// DEPRECATED
static bool ListContainsElement(VPackSlice const& list,
                                VPackSlice const& testee, size_t& index) {
  TRI_ASSERT(list.isArray());
  for (size_t i = 0; i < static_cast<size_t>(list.length()); ++i) {
    if (arangodb::basics::VelocyPackHelper::compare(testee, list.at(i),
                                                    false) == 0) {
      index = i;
      return true;
    }
  }
  return false;
}

static bool ListContainsElement(VPackSlice const& list, VPackSlice const& testee) {
  size_t unused;
  return ListContainsElement(list, testee, unused);
}

/// @brief Computes the Variance of the given list.
///        If successful value will contain the variance and count
///        will contain the number of elements.
///        If not successful value and count contain garbage.
static bool Variance(arangodb::AqlTransaction* trx,
                     AqlValue const& values, double& value, size_t& count) {
  TRI_ASSERT(values.isArray());
  value = 0.0;
  count = 0;
  bool unused = false;
  double mean = 0.0;
  
  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(values);

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
static bool SortNumberList(arangodb::AqlTransaction* trx,
                           AqlValue const& values,
                           std::vector<double>& result) {
  TRI_ASSERT(values.isArray());
  TRI_ASSERT(result.empty());
  bool unused;
  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(values);

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

static void RequestEdges(VPackSlice const& vertexSlice,
                         arangodb::AqlTransaction* trx,
                         std::string const& collectionName,
                         Transaction::IndexHandle const& indexId,
                         TRI_edge_direction_e direction,
                         arangodb::ExampleMatcher const* matcher,
                         bool includeVertices, VPackBuilder& result) {

  std::string vertexId;
  if (vertexSlice.isString()) {
    vertexId = vertexSlice.copyString();
  } else if (vertexSlice.isObject()) {
    vertexId = arangodb::basics::VelocyPackHelper::getStringValue(vertexSlice,
                                                                  TRI_VOC_ATTRIBUTE_ID, "");
  } else {
    // Nothing to do.
    // Return (error for illegal input is thrown outside
    return;
  }

  VPackBuilder searchValueBuilder;
  EdgeIndex::buildSearchValue(direction, vertexId, searchValueBuilder);
  VPackSlice search = searchValueBuilder.slice();
  std::shared_ptr<OperationCursor> cursor = trx->indexScan(
      collectionName, arangodb::Transaction::CursorType::INDEX, indexId,
      search, 0, UINT64_MAX, 1000, false);
  if (cursor->failed()) {
    THROW_ARANGO_EXCEPTION(cursor->code);
  }

  auto opRes = std::make_shared<OperationResult>(TRI_ERROR_NO_ERROR);
  while (cursor->hasMore()) {
    cursor->getMore(opRes);
    if (opRes->failed()) {
      THROW_ARANGO_EXCEPTION(opRes->code);
    }
    VPackSlice edges = opRes->slice();
    TRI_ASSERT(edges.isArray());
    if (includeVertices) {
      for (auto const& edge : VPackArrayIterator(edges)) {
        VPackObjectBuilder guard(&result);
        if (matcher == nullptr || matcher->matches(edge)) {
          result.add("edge", edge);

          std::string target;
          TRI_ASSERT(edge.hasKey(TRI_VOC_ATTRIBUTE_FROM));
          TRI_ASSERT(edge.hasKey(TRI_VOC_ATTRIBUTE_TO));
          switch (direction) {
            case TRI_EDGE_OUT:
              target = edge.get(TRI_VOC_ATTRIBUTE_TO).copyString();
              break;
            case TRI_EDGE_IN:
              target = edge.get(TRI_VOC_ATTRIBUTE_FROM).copyString();
              break;
            case TRI_EDGE_ANY:
              target = edge.get(TRI_VOC_ATTRIBUTE_TO).copyString();
              if (target == vertexId) {
                target = edge.get(TRI_VOC_ATTRIBUTE_FROM).copyString();
              }
              break;
          }

          if (target.empty()) {
            // somehow invalid
            continue;
          }
          std::vector<std::string> split = arangodb::basics::StringUtils::split(target, "/");
          TRI_ASSERT(split.size() == 2);
          // We have to make sure the transaction know this collection!
          trx->addCollectionAtRuntime(split[0]);
          VPackBuilder vertexSearch;
          vertexSearch.openObject();
          vertexSearch.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(split[1]));
          vertexSearch.close();
          OperationOptions opts;
          OperationResult vertexResult = trx->document(split[0], vertexSearch.slice(), opts);
          if (vertexResult.failed()) {
            if (vertexResult.code == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
              // This is okay
              result.add("vertex", VPackValue(VPackValueType::Null));
            } else {
              THROW_ARANGO_EXCEPTION(vertexResult.code);
            }
          } else {
            result.add("vertex", vertexResult.slice());
          }
        }
      }
    } else {
      for (auto const& edge : VPackArrayIterator(edges)) {
        if (matcher == nullptr || matcher->matches(edge)) {
          result.add(edge);
        }
      }
    }
  }
}

/// @brief Helper function to unset or keep all given names in the value.
///        Recursively iterates over sub-object and unsets or keeps their values
///        as well
static void UnsetOrKeep(VPackSlice const& value,
                        std::unordered_set<std::string> const& names,
                        bool unset,  // true means unset, false means keep
                        bool recursive, VPackBuilder& result) {
  TRI_ASSERT(value.isObject());
  VPackObjectBuilder b(&result); // Close the object after this function
  for (auto const& entry : VPackObjectIterator(value)) {
    TRI_ASSERT(entry.key.isString());
    std::string key = entry.key.copyString();
    if (!((names.find(key) == names.end()) ^ unset)) {
      // not found and unset or found and keep 
      if (recursive && entry.value.isObject()) {
        result.add(entry.key); // Add the key
        UnsetOrKeep(entry.value, names, unset, recursive, result); // Adds the object
      } else {
        result.add(key, entry.value);
      }
    }
  }
}

static void RegisterCollectionInTransaction(
    arangodb::AqlTransaction* trx, std::string const& collectionName,
    TRI_voc_cid_t& cid) {
  cid = trx->resolver()->getCollectionIdLocal(collectionName);
  trx->addCollectionAtRuntime(cid, collectionName);
}

/// @brief Helper function to get a document by it's identifier
///        Lazy Locks the collection if necessary.
static void GetDocumentByIdentifier(arangodb::AqlTransaction* trx,
                                    std::string const& collectionName,
                                    std::string const& identifier,
                                    bool ignoreError,
                                    VPackBuilder& result) {
  OperationOptions options;
  OperationResult opRes;
  VPackBuilder searchBuilder;
  searchBuilder.openObject();
  searchBuilder.add(VPackValue(TRI_VOC_ATTRIBUTE_KEY));

  std::vector<std::string> parts =
      arangodb::basics::StringUtils::split(identifier, "/");


  if (parts.size() == 1) {
    searchBuilder.add(VPackValue(identifier));
    searchBuilder.close();

    try {
      TRI_voc_cid_t cid;
      RegisterCollectionInTransaction(trx, collectionName, cid);
    } catch (arangodb::basics::Exception const&) {
      if (ignoreError) {
        return;
      }
      throw;
    }

    opRes = trx->document(collectionName, searchBuilder.slice(), options);
  } else if (parts.size() == 2) {
    if (collectionName.empty()) {
      searchBuilder.add(VPackValue(parts[1]));
      searchBuilder.close();

      try {
        TRI_voc_cid_t cid;
        RegisterCollectionInTransaction(trx, parts[0], cid);
      } catch (arangodb::basics::Exception const&) {
        if (ignoreError) {
          return;
        }
        throw;
      }

      opRes = trx->document(parts[0], searchBuilder.slice(), options);
    } else if (parts[0] != collectionName) {
      // Reqesting an _id that cannot be stored in this collection
      if (ignoreError) {
        return;
      }
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST);
    } else {
      searchBuilder.add(VPackValue(parts[1]));
      searchBuilder.close();

      try {
        TRI_voc_cid_t cid;
        RegisterCollectionInTransaction(trx, collectionName, cid);
      } catch (arangodb::basics::Exception const&) {
        if (ignoreError) {
          return;
        }
        throw;
      }

      opRes = trx->document(collectionName, searchBuilder.slice(), options);
    }
  } else {
    if (ignoreError) {
      return;
    }
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }
  if (opRes.failed()) {
    if (ignoreError) {
      return;
    }
    THROW_ARANGO_EXCEPTION(opRes.code);
  }

  result.add(opRes.slice());
}

/// @brief Helper function to merge given parameters
///        Works for an array of objects as first parameter or arbitrary many
///        object parameters
static AqlValue MergeParameters(arangodb::aql::Query* query,
                                 arangodb::AqlTransaction* trx,
                                 VPackFunctionParameters const& parameters,
                                 char const* funcName,
                                 bool recursive) {
  VPackBuilder builder;

  size_t const n = parameters.size();
  if (n == 0) {
    builder.openObject();
    builder.close();
    return AqlValue(builder);
  }

  // use the first argument as the preliminary result
  AqlValue initial = ExtractFunctionParameterValue(trx, parameters, 0);
  AqlValueMaterializer materializer(trx);
  VPackSlice initialSlice = materializer.slice(initial);

  if (initial.isArray() && n == 1) {
    // special case: a single array parameter
    try {
      // Create an empty document as start point
      builder.openObject();
      builder.close();
    } catch (...) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    // merge in all other arguments
    for (auto const& it : VPackArrayIterator(initialSlice)) {
      if (!it.isObject()) {
        RegisterInvalidArgumentWarning(query, funcName);
        builder.clear();
        builder.add(VPackValue(VPackValueType::Null));
        return AqlValue(builder);
      }
      try {
        builder = arangodb::basics::VelocyPackHelper::merge(builder.slice(), it, false,
                                                            recursive);
      } catch (...) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
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
    VPackSlice slice = materializer.slice(param);

    try {
      builder = arangodb::basics::VelocyPackHelper::merge(initialSlice, slice, false,
                                                          recursive);
      initialSlice = builder.slice();
    } catch (...) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }
  return AqlValue(builder);
}

/// @brief Transforms an unordered_map<VertexId> to AQL VelocyPack values
static AqlValue VertexIdsToAqlValueVPack(arangodb::aql::Query* query,
                                         arangodb::AqlTransaction* trx,
                                         std::unordered_set<std::string>& ids,
                                         bool includeData = false) {
  TransactionBuilderLeaser builder(trx);
  builder->openArray();
  if (includeData) {
    for (auto& it : ids) {
      // THROWS ERRORS if the Document was not found
      GetDocumentByIdentifier(trx, "", it, false, *builder.get());
    }
  } else {
    for (auto& it : ids) {
      builder->add(VPackValue(it));
    }
  }
  builder->close();

  return AqlValue(builder.get());
}

/// @brief Load geoindex for collection name
static arangodb::Index* getGeoIndex(arangodb::AqlTransaction* trx,
                                    TRI_voc_cid_t const& cid,
                                    std::string const& collectionName) {
  trx->addCollectionAtRuntime(cid, collectionName);

  auto document = trx->documentCollection(cid);

  if (document == nullptr) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                  "'%s'", collectionName.c_str());
  }

  arangodb::Index* index = nullptr;

  for (auto const& idx : document->allIndexes()) {
    if (idx->type() == arangodb::Index::TRI_IDX_TYPE_GEO1_INDEX ||
        idx->type() == arangodb::Index::TRI_IDX_TYPE_GEO2_INDEX) {
      index = idx;
      break;
    }
  }

  if (index == nullptr) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_GEO_INDEX_MISSING,
                                  collectionName.c_str());
  }

  trx->orderDitch(cid);

  return index;
}

static AqlValue buildGeoResult(arangodb::AqlTransaction* trx,
                               arangodb::aql::Query* query,
                               GeoCoordinates* cors,
                               TRI_voc_cid_t const& cid,
                               std::string const& attributeName) {
  if (cors == nullptr) {
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }

  size_t const nCoords = cors->length;
  if (nCoords == 0) {
    GeoIndex_CoordinatesFree(cors);
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }

  struct geo_coordinate_distance_t {
    geo_coordinate_distance_t(double distance, TRI_doc_mptr_t const* mptr)
        : _distance(distance), _mptr(mptr) {}

    double _distance;
    TRI_doc_mptr_t const* _mptr;
  };

  std::vector<geo_coordinate_distance_t> distances;

  try {
    distances.reserve(nCoords);

    for (size_t i = 0; i < nCoords; ++i) {
      distances.emplace_back(geo_coordinate_distance_t(
          cors->distances[i],
          static_cast<TRI_doc_mptr_t const*>(cors->coordinates[i].data)));
    }
  } catch (...) {
    GeoIndex_CoordinatesFree(cors);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  GeoIndex_CoordinatesFree(cors);

  // sort result by distance
  std::sort(distances.begin(), distances.end(),
            [](geo_coordinate_distance_t const& left,
               geo_coordinate_distance_t const& right) {
              return left._distance < right._distance;
            });

  try {
    TransactionBuilderLeaser builder(trx);
    builder->openArray();
    if (!attributeName.empty()) {
      // We have to copy the entire document
      for (auto& it : distances) {
        VPackObjectBuilder docGuard(builder.get());
        builder->add(attributeName, VPackValue(it._distance));
        VPackSlice doc(it._mptr->vpack()); // TODO
        for (auto const& entry : VPackObjectIterator(doc)) {
          std::string key = entry.key.copyString();
          if (key != attributeName) {
            builder->add(key, entry.value);
          }
        }
      }

    } else {
      for (auto& it : distances) {
        InsertMasterPointer(it._mptr, *builder.get());
      }
    }
    builder->close();
    return AqlValue(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
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

/// @brief called before a query starts
/// has the chance to set up any thread-local storage
void Functions::InitializeThreadContext() {}

/// @brief called when a query ends
/// its responsibility is to clear any thread-local storage
void Functions::DestroyThreadContext() { ClearRegexCache(); }

/// @brief function IS_NULL
AqlValue Functions::IsNull(arangodb::aql::Query* query,
                           arangodb::AqlTransaction* trx,
                           VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(trx, parameters, 0);
  return AqlValue(arangodb::basics::VelocyPackHelper::BooleanValue(a.isNull(true)));
}

/// @brief function IS_BOOL
AqlValue Functions::IsBool(arangodb::aql::Query* query,
                           arangodb::AqlTransaction* trx,
                           VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(trx, parameters, 0);
  return AqlValue(arangodb::basics::VelocyPackHelper::BooleanValue(a.isBoolean()));
}

/// @brief function IS_NUMBER
AqlValue Functions::IsNumber(arangodb::aql::Query* query,
                             arangodb::AqlTransaction* trx,
                             VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(trx, parameters, 0);
  return AqlValue(arangodb::basics::VelocyPackHelper::BooleanValue(a.isNumber()));
}

/// @brief function IS_STRING
AqlValue Functions::IsString(arangodb::aql::Query* query,
                             arangodb::AqlTransaction* trx,
                             VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(trx, parameters, 0);
  return AqlValue(arangodb::basics::VelocyPackHelper::BooleanValue(a.isString()));
}

/// @brief function IS_ARRAY
AqlValue Functions::IsArray(arangodb::aql::Query* query,
                            arangodb::AqlTransaction* trx,
                            VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(trx, parameters, 0);
  return AqlValue(arangodb::basics::VelocyPackHelper::BooleanValue(a.isArray()));
}

/// @brief function IS_OBJECT
AqlValue Functions::IsObject(arangodb::aql::Query* query,
                             arangodb::AqlTransaction* trx,
                             VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(trx, parameters, 0);
  return AqlValue(arangodb::basics::VelocyPackHelper::BooleanValue(a.isObject()));
}

/// @brief function TO_NUMBER
AqlValue Functions::ToNumber(arangodb::aql::Query* query,
                             arangodb::AqlTransaction* trx,
                             VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(trx, parameters, 0);
  bool failed;
  double value = a.toDouble(failed);

  if (failed) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
  
  TransactionBuilderLeaser builder(trx);
  builder->add(VPackValue(value));
  return AqlValue(builder.get());
}

/// @brief function TO_STRING
AqlValue Functions::ToString(arangodb::aql::Query* query,
                             arangodb::AqlTransaction* trx,
                             VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  arangodb::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE, 24);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer.stringBuffer());

  AppendAsString(trx, adapter, value);
  TransactionBuilderLeaser builder(trx);
  try {
    builder->add(VPackValuePair(buffer.begin(), buffer.length(), VPackValueType::String));
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  return AqlValue(builder.get());
}

/// @brief function TO_BOOL
AqlValue Functions::ToBool(arangodb::aql::Query* query,
                           arangodb::AqlTransaction* trx,
                           VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(trx, parameters, 0);
  return AqlValue(arangodb::basics::VelocyPackHelper::BooleanValue(a.toBoolean()));
}

/// @brief function TO_ARRAY
AqlValue Functions::ToArray(arangodb::aql::Query* query,
                            arangodb::AqlTransaction* trx,
                            VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (value.isArray()) {
    // return copy of the original array
    return AqlValue(value);
  }

  if (value.isNull(true)) {
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }
  
  TransactionBuilderLeaser builder(trx);
  builder->openArray();
  if (value.isBoolean() || value.isNumber() || value.isString()) {
    // return array with single member
    builder->add(value.slice());
  } else if (value.isObject()) {
    AqlValueMaterializer materializer(trx);
    VPackSlice slice = materializer.slice(value);
    // return an array with the attribute values
    for (auto const& it : VPackObjectIterator(slice)) {
      builder->add(it.value);
    }
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function LENGTH
AqlValue Functions::Length(arangodb::aql::Query* q,
                           arangodb::AqlTransaction* trx,
                           VPackFunctionParameters const& parameters) {
  TransactionBuilderLeaser builder(trx);

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
    double tmp = value.toDouble();
    if (std::isnan(tmp) || !std::isfinite(tmp)) {
      length = 0;
    } else {
      char buffer[24];
      length = static_cast<size_t>(fpconv_dtoa(tmp, buffer));
    }
  } else if (value.isString()) {
    length = TRI_CharLengthUtf8String(value.slice().copyString().c_str());
  } else if (value.isObject()) {
    length = static_cast<size_t>(value.length());
  }
  builder->add(VPackValue(static_cast<double>(length)));
  return AqlValue(builder.get());
}

/// @brief function FIRST
AqlValue Functions::First(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
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
  return value.at(0, mustDestroy, true);
}

/// @brief function LAST
AqlValue Functions::Last(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
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
  return value.at(n - 1, mustDestroy, true);
}

/// @brief function NTH
AqlValue Functions::Nth(arangodb::aql::Query* query,
                        arangodb::AqlTransaction* trx,
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
  int64_t index = position.toInt64();

  if (index < 0 || index >= static_cast<int64_t>(n)) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  bool mustDestroy;
  return value.at(index, mustDestroy, true);
}

/// @brief function CONCAT
AqlValue Functions::Concat(arangodb::aql::Query* query,
                           arangodb::AqlTransaction* trx,
                           VPackFunctionParameters const& parameters) {
  arangodb::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE, 24);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer.stringBuffer());

  size_t const n = parameters.size();

  for (size_t i = 0; i < n; ++i) {
    AqlValue member = ExtractFunctionParameterValue(trx, parameters, i);

    if (member.isNull(true)) {
      continue;
    }

    if (member.isArray()) {
      // append each member individually
      AqlValueMaterializer materializer(trx);
      VPackSlice slice = materializer.slice(member);
      for (auto const& sub : VPackArrayIterator(slice)) {
        if (sub.isNone() || sub.isNull()) {
          continue;
        }

        AppendAsString(adapter, sub);
      }
    } else {
      // convert member to a string and append
      AppendAsString(trx, adapter, member);
    }
  }

  // steal the StringBuffer's char* pointer so we can avoid copying data around
  // multiple times
  size_t length = buffer.length();
  try {
    TransactionBuilderLeaser builder(trx);
    builder->add(VPackValuePair(buffer.c_str(), length, VPackValueType::String));
    return AqlValue(*builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

/// @brief function LIKE
AqlValue Functions::Like(arangodb::aql::Query* query,
                         arangodb::AqlTransaction* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "LIKE", 2, 3);
  bool const caseInsensitive = GetBooleanParameter(trx, parameters, 2, false);
  arangodb::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE, 24);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer.stringBuffer());

  // build pattern from parameter #1
  AqlValue regex = ExtractFunctionParameterValue(trx, parameters, 1);
  AppendAsString(trx, adapter, regex);

  size_t const length = buffer.length();

  std::string const pattern =
      BuildRegexPattern(buffer.c_str(), length, caseInsensitive);
  RegexMatcher* matcher = nullptr;

  if (RegexCache != nullptr) {
    auto it = RegexCache->find(pattern);

    // check regex cache
    if (it != RegexCache->end()) {
      matcher = (*it).second;
    }
  }

  if (matcher == nullptr) {
    matcher =
        arangodb::basics::Utf8Helper::DefaultUtf8Helper.buildMatcher(pattern);

    try {
      if (RegexCache == nullptr) {
        RegexCache = new std::unordered_map<std::string, RegexMatcher*>();
      }
      // insert into cache, no matter if pattern is valid or not
      RegexCache->emplace(pattern, matcher);
    } catch (...) {
      delete matcher;
      ClearRegexCache();
      throw;
    }
  }

  if (matcher == nullptr) {
    // compiling regular expression failed
    RegisterWarning(query, "LIKE", TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  // extract value
  buffer.clear();
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  AppendAsString(trx, adapter, value);

  bool error = false;
  bool const result = arangodb::basics::Utf8Helper::DefaultUtf8Helper.matches(
      matcher, buffer.c_str(), buffer.length(), error);

  if (error) {
    // compiling regular expression failed
    RegisterWarning(query, "LIKE", TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  } 
  
  TransactionBuilderLeaser builder(trx);
  builder->add(VPackValue(result));
  return AqlValue(builder.get());
}

/// @brief function PASSTHRU
AqlValue Functions::Passthru(arangodb::aql::Query* query,
                             arangodb::AqlTransaction* trx,
                             VPackFunctionParameters const& parameters) {
  if (parameters.empty()) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  return ExtractFunctionParameterValue(trx, parameters, 0).clone();
}

/// @brief function UNSET
AqlValue Functions::Unset(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "UNSET", 2);
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isObject()) {
    RegisterInvalidArgumentWarning(query, "UNSET");
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  std::unordered_set<std::string> names;
  ExtractKeys(names, query, trx, parameters, 1, "UNSET");

  try {
    AqlValueMaterializer materializer(trx);
    VPackSlice slice = materializer.slice(value);
    TransactionBuilderLeaser builder(trx);
    UnsetOrKeep(slice, names, true, false, *builder.get());
    return AqlValue(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

/// @brief function UNSET_RECURSIVE
AqlValue Functions::UnsetRecursive(arangodb::aql::Query* query,
                                   arangodb::AqlTransaction* trx,
                                   VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "UNSET_RECURSIVE", 2);
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isObject()) {
    RegisterInvalidArgumentWarning(query, "UNSET_RECURSIVE");
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  std::unordered_set<std::string> names;
  ExtractKeys(names, query, trx, parameters, 1, "UNSET_RECURSIVE");

  try {
    AqlValueMaterializer materializer(trx);
    VPackSlice slice = materializer.slice(value);
    TransactionBuilderLeaser builder(trx);
    UnsetOrKeep(slice, names, true, true, *builder.get());
    return AqlValue(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

/// @brief function KEEP
AqlValue Functions::Keep(arangodb::aql::Query* query,
                         arangodb::AqlTransaction* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "KEEP", 2);
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isObject()) {
    RegisterInvalidArgumentWarning(query, "KEEP");
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  std::unordered_set<std::string> names;
  ExtractKeys(names, query, trx, parameters, 1, "KEEP");

  try {
    AqlValueMaterializer materializer(trx);
    VPackSlice slice = materializer.slice(value);
    TransactionBuilderLeaser builder(trx);
    UnsetOrKeep(slice, names, false, false, *builder.get());
    return AqlValue(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

/// @brief function MERGE
AqlValue Functions::Merge(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
                          VPackFunctionParameters const& parameters) {
  return MergeParameters(query, trx, parameters, "MERGE", false);
}

/// @brief function MERGE_RECURSIVE
AqlValue Functions::MergeRecursive(arangodb::aql::Query* query,
                                   arangodb::AqlTransaction* trx,
                                   VPackFunctionParameters const& parameters) {
  return MergeParameters(query, trx, parameters, "MERGE_RECURSIVE", true);
}

/// @brief function HAS
AqlValue Functions::Has(arangodb::aql::Query* query,
                        arangodb::AqlTransaction* trx,
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
    arangodb::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
    arangodb::basics::VPackStringBufferAdapter adapter(buffer.stringBuffer());
    AppendAsString(trx, adapter, name);
    p = std::string(buffer.c_str(), buffer.length());
  } else {
    p = name.slice().copyString();
  }

  return AqlValue(value.hasKey(trx, p));
}

/// @brief function ATTRIBUTES
AqlValue Functions::Attributes(arangodb::aql::Query* query,
                               arangodb::AqlTransaction* trx,
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
  VPackSlice slice = materializer.slice(value);

  if (doSort) {
    std::set<std::string, arangodb::basics::VelocyPackHelper::AttributeSorter>
        keys;

    VPackCollection::keys(slice, keys);
    VPackBuilder result;
    result.openArray();
    for (auto const& it : keys) {
      TRI_ASSERT(!it.empty());
      if (removeInternal && it.at(0) == '_') {
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
    TRI_ASSERT(!it.empty());
    if (removeInternal && it.at(0) == '_') {
      continue;
    }
    result.add(VPackValue(it));
  }
  result.close();
  return AqlValue(result);
}

/// @brief function VALUES
AqlValue Functions::Values(arangodb::aql::Query* query,
                           arangodb::AqlTransaction* trx,
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
  VPackSlice slice = materializer.slice(value);
  TransactionBuilderLeaser builder(trx);
  builder->openArray();
  for (auto const& entry : VPackObjectIterator(slice)) {
    if (!entry.key.isString()) {
      // somehow invalid
      continue;
    }
    if (removeInternal && entry.key.copyString().at(0) == '_') {
      // skip attribute
      continue;
    }
    builder->add(entry.value);
  }
  builder->close();

  return AqlValue(builder.get());
}

/// @brief function MIN
AqlValue Functions::Min(arangodb::aql::Query* query,
                        arangodb::AqlTransaction* trx,
                        VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "MIN", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value);

  VPackSlice minValue;
  for (auto const& it : VPackArrayIterator(slice)) {
    if (it.isNull()) {
      continue;
    }
    if (minValue.isNone() || arangodb::basics::VelocyPackHelper::compare(it, minValue, true) < 0) {
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
                        arangodb::AqlTransaction* trx,
                        VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "MAX", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value);
  VPackSlice maxValue;
  for (auto const& it : VPackArrayIterator(slice)) {
    if (maxValue.isNone() || arangodb::basics::VelocyPackHelper::compare(it, maxValue, true) > 0) {
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
                        arangodb::AqlTransaction* trx,
                        VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "SUM", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value);
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

  return NumberValue(trx, sum);
}

/// @brief function AVERAGE
AqlValue Functions::Average(arangodb::aql::Query* query,
                            arangodb::AqlTransaction* trx,
                            VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "AVERAGE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
  
  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value);

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
    return NumberValue(trx, sum / static_cast<size_t>(count));
  } 

  return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
}

/// @brief function MD5
AqlValue Functions::Md5(arangodb::aql::Query* query,
                        arangodb::AqlTransaction* trx,
                        VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  arangodb::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer.stringBuffer());

  AppendAsString(trx, adapter, value);

  // create md5
  char hash[17];
  char* p = &hash[0];
  size_t length;

  arangodb::rest::SslInterface::sslMD5(buffer.c_str(), buffer.length(), p,
                                       length);

  // as hex
  char hex[33];
  p = &hex[0];

  arangodb::rest::SslInterface::sslHEX(hash, 16, p, length);

  TransactionBuilderLeaser builder(trx);
  builder->add(VPackValue(std::string(hex, 32)));
  return AqlValue(builder.get());
}

/// @brief function SHA1
AqlValue Functions::Sha1(arangodb::aql::Query* query,
                         arangodb::AqlTransaction* trx,
                         VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  arangodb::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer.stringBuffer());

  AppendAsString(trx, adapter, value);

  // create sha1
  char hash[21];
  char* p = &hash[0];
  size_t length;

  arangodb::rest::SslInterface::sslSHA1(buffer.c_str(), buffer.length(), p,
                                        length);

  // as hex
  char hex[41];
  p = &hex[0];

  arangodb::rest::SslInterface::sslHEX(hash, 20, p, length);

  TransactionBuilderLeaser builder(trx);
  builder->add(VPackValue(std::string(hex, 40)));
  return AqlValue(builder.get());
}

/// @brief function UNIQUE
AqlValue Functions::Unique(arangodb::aql::Query* query,
                           arangodb::AqlTransaction* trx,
                           VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "UNIQUE", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "UNIQUE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value);

  std::unordered_set<VPackSlice,
                     arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      values(512, arangodb::basics::VelocyPackHelper::VPackHash(),
             arangodb::basics::VelocyPackHelper::VPackEqual());

  for (auto const& s : VPackArrayIterator(slice)) {
    if (!s.isNone()) {
      values.emplace(s);
    }
  }

  TransactionBuilderLeaser builder(trx);
  try {
    builder->openArray();
    for (auto const& it : values) {
      builder->add(it);
    }
    builder->close();
    return AqlValue(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

/// @brief function SORTED_UNIQUE
AqlValue Functions::SortedUnique(arangodb::aql::Query* query,
                                 arangodb::AqlTransaction* trx,
                                 VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "SORTED_UNIQUE", 1, 1);
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    // this is an internal function - do NOT issue a warning here
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
  
  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value);

  arangodb::basics::VelocyPackHelper::VPackLess<true> less(trx->transactionContext()->getVPackOptions(), &slice, &slice);
  std::set<VPackSlice, arangodb::basics::VelocyPackHelper::VPackLess<true>> values(less);
  for (auto const& it : VPackArrayIterator(slice)) {
    if (!it.isNone()) {
      values.insert(it);
    }
  }

  TransactionBuilderLeaser builder(trx);
  try {
    builder->openArray();
    for (auto const& it : values) {
      builder->add(it);
    }
    builder->close();
    return AqlValue(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

/// @brief function UNION
AqlValue Functions::Union(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "UNION", 2);

  try {
    TransactionBuilderLeaser builder(trx);
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
      VPackSlice slice = materializer.slice(value);

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
  } catch (arangodb::basics::Exception const&) {
    // Rethrow arangodb Errors
    throw;
  } catch (std::exception const&) {
    // All other exceptions are OUT_OF_MEMORY
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

/// @brief function UNION_DISTINCT
AqlValue Functions::UnionDistinct(arangodb::aql::Query* query,
                                  arangodb::AqlTransaction* trx,
                                  VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "UNION_DISTINCT", 2);
  size_t const n = parameters.size();

  std::unordered_set<VPackSlice,
                     arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      values(512, arangodb::basics::VelocyPackHelper::VPackHash(),
             arangodb::basics::VelocyPackHelper::VPackEqual());


  for (size_t i = 0; i < n; ++i) {
    AqlValue value = ExtractFunctionParameterValue(trx, parameters, i);

    if (!value.isArray()) {
      // not an array
      RegisterInvalidArgumentWarning(query, "UNION_DISTINCT");
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    }

    AqlValueMaterializer materializer(trx);
    VPackSlice slice = materializer.slice(value);

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
  try {
    TransactionBuilderLeaser builder(trx);
    builder->openArray();
    for (auto const& it : values) {
      builder->add(it);
    }
    builder->close();
 
    TRI_IF_FAILURE("AqlFunctions::OutOfMemory3") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    return AqlValue(builder.get());
  } catch (arangodb::basics::Exception const&) {
    // Rethrow arangodb Errors
    throw;
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

/// @brief function INTERSECTION
AqlValue Functions::Intersection(arangodb::aql::Query* query,
                                 arangodb::AqlTransaction* trx,
                                 VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "INTERSECTION", 2);
  
  std::unordered_map<VPackSlice, size_t,
                     arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      values(512, arangodb::basics::VelocyPackHelper::VPackHash(),
             arangodb::basics::VelocyPackHelper::VPackEqual());

  size_t const n = parameters.size();
  for (size_t i = 0; i < n; ++i) {
    AqlValue value = ExtractFunctionParameterValue(trx, parameters, i);

    if (!value.isArray()) {
      // not an array
      RegisterWarning(query, "INTERSECTION", TRI_ERROR_QUERY_ARRAY_EXPECTED);
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    }
    
    AqlValueMaterializer materializer(trx);
    VPackSlice slice = materializer.slice(value);

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
          TRI_ASSERT((*found).second > 0);
          ++(found->second);
        }
      }
    }
  }

  TRI_IF_FAILURE("AqlFunctions::OutOfMemory2") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  TransactionBuilderLeaser builder(trx);
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

/// @brief function NEIGHBORS
AqlValue Functions::Neighbors(arangodb::aql::Query* query,
                              arangodb::AqlTransaction* trx,
                              VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "NEIGHBORS", 4, 6);

  arangodb::traverser::NeighborsOptions opts(trx);

  AqlValue vertexCol = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!vertexCol.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
  }
  std::string vColName(vertexCol.slice().copyString());

  AqlValue edgeCol = ExtractFunctionParameterValue(trx, parameters, 1);
  if (!edgeCol.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
  }
  std::string eColName(edgeCol.slice().copyString());

  AqlValue vertexInfo = ExtractFunctionParameterValue(trx, parameters, 2);
  std::string vertexId;
  bool splitCollection = false;
  if (vertexInfo.isString()) {
    vertexId = vertexInfo.slice().copyString();
    if (vertexId.find("/") != std::string::npos) {
      splitCollection = true;
    }
  } else if (vertexInfo.isObject()) {
    if (!vertexInfo.hasKey(trx, TRI_VOC_ATTRIBUTE_ID)) {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
    }
    bool localMustDestroy;
    AqlValue id = vertexInfo.get(trx, TRI_VOC_ATTRIBUTE_ID, localMustDestroy, false);
    AqlValueGuard guard(id, localMustDestroy);

    if (!id.isString()) {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
    }
    vertexId = id.slice().copyString();
    splitCollection = true;
  } else {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
  }

  if (splitCollection) {
    size_t split;
    char const* str = vertexId.c_str();

    if (!TRI_ValidateDocumentIdKeyGenerator(str, vertexId.size(), &split)) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
    }

    std::string const collectionName = vertexId.substr(0, split);
    if (collectionName.compare(vColName) != 0) {
      THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_GRAPH_INVALID_PARAMETER,
                                    "specified vertex collection '%s' does "
                                    "not match start vertex collection '%s'",
                                    vColName.c_str(), collectionName.c_str());
    }
    auto coli = trx->resolver()->getCollectionStruct(collectionName);

    if (coli == nullptr) {
      THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                    "'%s'", collectionName.c_str());
    }

    opts.start = vertexId;
  } else {
    opts.start = vertexId;
  }

  AqlValue direction = ExtractFunctionParameterValue(trx, parameters, 3);
  if (!direction.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
  }

  {
    std::string const dir = direction.slice().copyString();
    if (dir.compare("outbound") == 0) {
      opts.direction = TRI_EDGE_OUT;
    } else if (dir.compare("inbound") == 0) {
      opts.direction = TRI_EDGE_IN;
    } else if (dir.compare("any") == 0) {
      opts.direction = TRI_EDGE_ANY;
    } else {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
    }
  }

  bool includeData = false;

  if (parameters.size() > 5) {
    AqlValue options = ExtractFunctionParameterValue(trx, parameters, 5);
    if (!options.isObject()) {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
    }

    AqlValueMaterializer materializer(trx);
    VPackSlice s = materializer.slice(options);

    includeData = arangodb::basics::VelocyPackHelper::getBooleanValue(
        s, "includeData", false);
    opts.minDepth =
        arangodb::basics::VelocyPackHelper::getNumericValue<uint64_t>(
            s, "minDepth", 1);
    if (opts.minDepth == 0) {
      opts.maxDepth =
          arangodb::basics::VelocyPackHelper::getNumericValue<uint64_t>(
              s, "maxDepth", 1);
    } else {
      opts.maxDepth =
          arangodb::basics::VelocyPackHelper::getNumericValue<uint64_t>(
              s, "maxDepth", opts.minDepth);
    }
  }

  TRI_voc_cid_t eCid = trx->resolver()->getCollectionIdLocal(eColName);

  // ensure the collection is loaded
  trx->addCollectionAtRuntime(eCid, eColName);

  // Function to return constant distance
  auto wc = [](VPackSlice) -> double { return 1; };

  auto eci = std::make_unique<EdgeCollectionInfo>(
      trx, eColName, wc);
  TRI_IF_FAILURE("EdgeCollectionInfoOOM1") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (parameters.size() > 4) {
    AqlValue examples = ExtractFunctionParameterValue(trx, parameters, 4);
    if (!(examples.isArray() && examples.length() == 0)) {
      AqlValueMaterializer materializer(trx);
      VPackSlice edges = materializer.slice(examples);
      opts.addEdgeFilter(edges, eColName);
    }
  }

  std::vector<EdgeCollectionInfo*> edgeCollectionInfos;
  arangodb::basics::ScopeGuard guard{[]() -> void {},
                                     [&edgeCollectionInfos]() -> void {
                                       for (auto& p : edgeCollectionInfos) {
                                         delete p;
                                       }
                                     }};
  edgeCollectionInfos.emplace_back(eci.get());
  eci.release();
  TRI_IF_FAILURE("EdgeCollectionInfoOOM2") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  std::unordered_set<std::string> neighbors;
  TRI_RunNeighborsSearch(edgeCollectionInfos, opts, neighbors);

  return VertexIdsToAqlValueVPack(query, trx, neighbors, includeData);
}

/// @brief function NEAR
AqlValue Functions::Near(arangodb::aql::Query* query,
                         arangodb::AqlTransaction* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "NEAR", 3, 5);

  AqlValue collectionValue = ExtractFunctionParameterValue(trx, parameters, 0);
  if (!collectionValue.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEAR");
  }

  std::string const collectionName(collectionValue.slice().copyString());

  AqlValue latitude = ExtractFunctionParameterValue(trx, parameters, 1);
  AqlValue longitude = ExtractFunctionParameterValue(trx, parameters, 2);

  if (!latitude.isNumber() || !longitude.isNumber()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEAR");
  }

  // extract limit
  int64_t limitValue = 100;

  if (parameters.size() > 3) {
    AqlValue limit = ExtractFunctionParameterValue(trx, parameters, 3);

    if (limit.isNumber()) {
      limitValue = limit.toInt64();
    } else if (!limit.isNull(true)) {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEAR");
    }
  }

  std::string attributeName;
  if (parameters.size() > 4) {
    // have a distance attribute
    AqlValue distanceValue = ExtractFunctionParameterValue(trx, parameters, 4);

    if (!distanceValue.isNull(true) && !distanceValue.isString()) {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEAR");
    }

    if (distanceValue.isString()) {
      attributeName = distanceValue.slice().copyString();
    }
  }

  TRI_voc_cid_t cid = trx->resolver()->getCollectionIdLocal(collectionName);
  arangodb::Index* index = getGeoIndex(trx, cid, collectionName);

  TRI_ASSERT(index != nullptr);

  GeoCoordinates* cors = static_cast<arangodb::GeoIndex2*>(index)->nearQuery(
      trx, latitude.toDouble(), longitude.toDouble(), limitValue);

  return buildGeoResult(trx, query, cors, cid, attributeName);
}

/// @brief function WITHIN
AqlValue Functions::Within(arangodb::aql::Query* query,
                           arangodb::AqlTransaction* trx,
                           VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "WITHIN", 4, 5);

  AqlValue collectionValue = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!collectionValue.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "WITHIN");
  }

  std::string const collectionName(collectionValue.slice().copyString());

  AqlValue latitudeValue = ExtractFunctionParameterValue(trx, parameters, 1);
  AqlValue longitudeValue = ExtractFunctionParameterValue(trx, parameters, 2);
  AqlValue radiusValue = ExtractFunctionParameterValue(trx, parameters, 3);

  if (!latitudeValue.isNumber() || !longitudeValue.isNumber() || !radiusValue.isNumber()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "WITHIN");
  }

  std::string attributeName;
  if (parameters.size() > 4) {
    // have a distance attribute
    AqlValue distanceValue = ExtractFunctionParameterValue(trx, parameters, 4);

    if (!distanceValue.isNull(true) && !distanceValue.isString()) {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "WITHIN");
    }

    if (distanceValue.isString()) {
      attributeName = distanceValue.slice().copyString();
    }
  }

  TRI_voc_cid_t cid = trx->resolver()->getCollectionIdLocal(collectionName);
  arangodb::Index* index = getGeoIndex(trx, cid, collectionName);

  TRI_ASSERT(index != nullptr);

  GeoCoordinates* cors = static_cast<arangodb::GeoIndex2*>(index)->withinQuery(
      trx, latitudeValue.toDouble(), longitudeValue.toDouble(), radiusValue.toDouble());

  return buildGeoResult(trx, query, cors, cid, attributeName);
}

/// @brief function FLATTEN
AqlValue Functions::Flatten(arangodb::aql::Query* query,
                            arangodb::AqlTransaction* trx,
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
    double tmpMaxDepth = maxDepthValue.toDouble(failed);
    if (failed || tmpMaxDepth < 1) {
      maxDepth = 1;
    } else {
      maxDepth = static_cast<size_t>(tmpMaxDepth);
    }
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice listSlice = materializer.slice(list);

  try {
    TransactionBuilderLeaser builder(trx);
    builder->openArray();
    FlattenList(listSlice, maxDepth, 0, *builder.get());
    builder->close();
    return AqlValue(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

/// @brief function ZIP
AqlValue Functions::Zip(arangodb::aql::Query* query,
                        arangodb::AqlTransaction* trx,
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

  try {
    AqlValueMaterializer keyMaterializer(trx);
    VPackSlice keysSlice = keyMaterializer.slice(keys);
    
    AqlValueMaterializer valueMaterializer(trx);
    VPackSlice valuesSlice = valueMaterializer.slice(values);

    TransactionBuilderLeaser builder(trx);
    builder->openObject();

    // Buffer will temporarily hold the keys
    arangodb::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE, 24);
    arangodb::basics::VPackStringBufferAdapter adapter(buffer.stringBuffer());
    for (VPackValueLength i = 0; i < n; ++i) {
      buffer.reset();
      AppendAsString(adapter, keysSlice.at(i));
      builder->add(std::string(buffer.c_str(), buffer.length()), valuesSlice.at(i));
    }
    builder->close();
    return AqlValue(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

/// @brief function PARSE_IDENTIFIER
AqlValue Functions::ParseIdentifier(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
    VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "PARSE_IDENTIFIER", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  std::string identifier;
  if (value.isObject() && value.hasKey(trx, TRI_VOC_ATTRIBUTE_ID)) {
    bool localMustDestroy;
    AqlValue s = value.get(trx, TRI_VOC_ATTRIBUTE_ID, localMustDestroy, false);
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

  try {
    TransactionBuilderLeaser builder(trx);
    builder->openObject();
    builder->add("collection", VPackValue(parts[0]));
    builder->add("key", VPackValue(parts[1]));
    builder->close();
    return AqlValue(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

/// @brief function Minus
AqlValue Functions::Minus(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "MINUS", 2);

  AqlValue baseArray = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!baseArray.isArray()) {
    RegisterWarning(query, "MINUS",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  std::unordered_map<VPackSlice, 
                     size_t,
                     arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      contains(512, arangodb::basics::VelocyPackHelper::VPackHash(),
             arangodb::basics::VelocyPackHelper::VPackEqual());

  // Fill the original map
  AqlValueMaterializer materializer(trx);
  VPackSlice arraySlice = materializer.slice(baseArray);
  
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
    VPackSlice arraySlice = materializer.slice(next);

    for (auto const& search : VPackArrayIterator(arraySlice)) {
      auto find = contains.find(search);

      if (find != contains.end()) {
        contains.erase(find);
      }
    }
  }

  // We omit the normalize part from js, cannot occur here
  try {
    TransactionBuilderLeaser builder(trx);
    builder->openArray();
    for (auto const& it : contains) {
      builder->add(it.first);
    }
    builder->close();
    return AqlValue(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

/// @brief function Document
AqlValue Functions::Document(arangodb::aql::Query* query,
                             arangodb::AqlTransaction* trx,
                             VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "DOCUMENT", 1, 2);

  if (parameters.size() == 1) {
    AqlValue id = ExtractFunctionParameterValue(trx, parameters, 0);
    TransactionBuilderLeaser builder(trx);
    if (id.isString()) {
      std::string identifier(id.slice().copyString());
      GetDocumentByIdentifier(trx, "", identifier, true, *builder.get());
      if (builder->isEmpty()) {
        // not found
        return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
      }
      return AqlValue(builder.get());
    } 
    if (id.isArray()) {
      AqlValueMaterializer materializer(trx);
      VPackSlice idSlice = materializer.slice(id);
      builder->openArray();
      for (auto const& next : VPackArrayIterator(idSlice)) {
        if (next.isString()) {
          std::string identifier = next.copyString();
          GetDocumentByIdentifier(trx, "", identifier, true, *builder.get());
        }
      }
      builder->close();
      return AqlValue(builder.get());
    } 
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  AqlValue collectionValue = ExtractFunctionParameterValue(trx, parameters, 0);
  if (!collectionValue.isString()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
  std::string const collectionName(collectionValue.slice().copyString());

  bool notFound = false; // TODO: what does this do?

  AqlValue id = ExtractFunctionParameterValue(trx, parameters, 1);
  if (id.isString()) {
    if (notFound) {
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    }

    TransactionBuilderLeaser builder(trx);
    std::string identifier(id.slice().copyString());
    GetDocumentByIdentifier(trx, collectionName, identifier, true, *builder.get());
    if (builder->isEmpty()) {
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    }
    return AqlValue(builder.get());
  }
   
  if (id.isArray()) {
    TransactionBuilderLeaser builder(trx);
    builder->openArray();
    if (!notFound) {
      AqlValueMaterializer materializer(trx);
      VPackSlice idSlice = materializer.slice(id);
      for (auto const& next : VPackArrayIterator(idSlice)) {
        if (next.isString()) {
          std::string identifier(next.copyString());
          GetDocumentByIdentifier(trx, collectionName, identifier, true, *builder.get());
        }
      }
    }
    builder->close();
    return AqlValue(builder.get());
  }

  // Id has invalid format
  return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
}

/// @brief function Edges
AqlValue Functions::Edges(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "EDGES", 3, 5);

  AqlValue collectionValue = ExtractFunctionParameterValue(trx, parameters, 0);
  if (!collectionValue.isString()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
  std::string const collectionName(collectionValue.slice().copyString());

  TRI_voc_cid_t cid;
  RegisterCollectionInTransaction(trx, collectionName, cid);
  
  if (!trx->isEdgeCollection(collectionName)) {
    RegisterWarning(query, "EDGES", TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
  
  AqlValue vertexValue = ExtractFunctionParameterValue(trx, parameters, 1);
  if (!vertexValue.isArray() && !vertexValue.isString() && !vertexValue.isObject()) {
    // Invalid Start vertex
    // Early Abort before parsing other parameters
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  AqlValue directionValue = ExtractFunctionParameterValue(trx, parameters, 2);
  if (!directionValue.isString()) {
    RegisterWarning(query, "EDGES",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  std::string dirString(directionValue.slice().copyString());
  // transform String to lower case
  std::transform(dirString.begin(), dirString.end(), dirString.begin(),
                 ::tolower);

  TRI_edge_direction_e direction;

  if (dirString == "inbound") {
    direction = TRI_EDGE_IN;
  } else if (dirString == "outbound") {
    direction = TRI_EDGE_OUT;
  } else if (dirString == "any") {
    direction = TRI_EDGE_ANY;
  } else {
    RegisterWarning(query, "EDGES",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  std::unique_ptr<arangodb::ExampleMatcher> matcher;

  Transaction::IndexHandle indexId = trx->edgeIndexHandle(collectionName);

  size_t const n = parameters.size();
  if (n > 3) {
    // We might have examples
    AqlValue exampleValue = ExtractFunctionParameterValue(trx, parameters, 3);
    if ((exampleValue.isArray() && exampleValue.length() != 0) || exampleValue.isObject()) {
      AqlValueMaterializer materializer(trx);
      VPackSlice exampleSlice = materializer.slice(exampleValue);

      try {
        matcher.reset(
            new arangodb::ExampleMatcher(exampleSlice, false));
      } catch (arangodb::basics::Exception const& e) {
        if (e.code() != TRI_RESULT_ELEMENT_NOT_FOUND) {
          throw;
        }
        // We can never fulfill this filter!
        // RETURN empty Array
        return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
      }
    }
  }

  bool includeVertices = false;

  if (n == 5) {
    // We have options
    AqlValue options = ExtractFunctionParameterValue(trx, parameters, 4);
    if (options.isObject()) {
      AqlValueMaterializer materializer(trx);
      VPackSlice s = materializer.slice(options);
      includeVertices = arangodb::basics::VelocyPackHelper::getBooleanValue(s, "includeVertices", false);
    }
  }

  AqlValueMaterializer vertexMaterializer(trx);
  VPackSlice vertexSlice = vertexMaterializer.slice(vertexValue);

  TransactionBuilderLeaser builder(trx);
  builder->openArray();
    
  if (vertexSlice.isArray()) {
    for (auto const& v : VPackArrayIterator(vertexSlice)) {
      RequestEdges(v, trx, collectionName, indexId, direction,
                   matcher.get(), includeVertices, *builder.get());
    }
  } else {
    RequestEdges(vertexSlice, trx, collectionName, indexId, direction,
                 matcher.get(), includeVertices, *builder.get());
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function ROUND
AqlValue Functions::Round(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "ROUND", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  bool failed = false; // we're intentionally ignoring this variable
  double input = value.toDouble(failed);

  // Rounds down for < x.4999 and up for > x.50000
  return NumberValue(trx, floor(input + 0.5));  
}

/// @brief function ABS
AqlValue Functions::Abs(arangodb::aql::Query* query,
                        arangodb::AqlTransaction* trx,
                        VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "ABS", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  bool failed = false; // we're intentionally ignoring this variable
  double input = value.toDouble(failed);

  return NumberValue(trx, std::abs(input));  
}

/// @brief function CEIL
AqlValue Functions::Ceil(arangodb::aql::Query* query,
                         arangodb::AqlTransaction* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "CEIL", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  bool failed = false; // we're intentionally ignoring this variable
  double input = value.toDouble(failed);

  return NumberValue(trx, ceil(input));  
}

/// @brief function FLOOR
AqlValue Functions::Floor(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "FLOOR", 1, 1);
  
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);

  bool failed = false; // we're intentionally ignoring this variable
  double input = value.toDouble(failed);

  return NumberValue(trx, floor(input));  
}

/// @brief function SQRT
AqlValue Functions::Sqrt(arangodb::aql::Query* query,
                         arangodb::AqlTransaction* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "SQRT", 1, 1);
  
  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 0);
  
  bool failed = false; // we're intentionally ignoring this variable here
  double input = value.toDouble(failed);

  return NumberValue(trx, sqrt(input));  
}

/// @brief function POW
AqlValue Functions::Pow(arangodb::aql::Query* query,
                        arangodb::AqlTransaction* trx,
                        VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "POW", 2, 2);

  AqlValue baseValue = ExtractFunctionParameterValue(trx, parameters, 0);
  AqlValue expValue = ExtractFunctionParameterValue(trx, parameters, 1);

  bool failed = false; // we're ignoring this variable intentionally
  double base = baseValue.toDouble(failed);
  double exp = expValue.toDouble(failed);

  return NumberValue(trx, pow(base, exp));
}

/// @brief function RAND
AqlValue Functions::Rand(arangodb::aql::Query* query,
                         arangodb::AqlTransaction* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "RAND", 0, 0);

  // This random functionality is not too good yet...
  return NumberValue(trx, static_cast<double>(std::rand()) / RAND_MAX);
}

/// @brief function FIRST_DOCUMENT
AqlValue Functions::FirstDocument(arangodb::aql::Query* query,
                                  arangodb::AqlTransaction* trx,
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
                              arangodb::AqlTransaction* trx,
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
                         arangodb::AqlTransaction* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "PUSH", 2, 3);
  
  AqlValue list = ExtractFunctionParameterValue(trx, parameters, 0);
  AqlValue toPush = ExtractFunctionParameterValue(trx, parameters, 1);

  AqlValueMaterializer toPushMaterializer(trx);
  VPackSlice p = toPushMaterializer.slice(toPush);

  if (list.isNull(true)) {
    TransactionBuilderLeaser builder(trx);
    builder->openArray();
    builder->add(p);
    builder->close();
    return AqlValue(builder.get());
  } 
  
  if (!list.isArray()) {
    RegisterWarning(query, "PUSH",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  TransactionBuilderLeaser builder(trx);
  builder->openArray();
  AqlValueMaterializer materializer(trx);
  VPackSlice l = materializer.slice(list);

  for (auto const& it : VPackArrayIterator(l)) {
    builder->add(it);
  }
  if (parameters.size() == 3) {
    AqlValue unique = ExtractFunctionParameterValue(trx, parameters, 2);
    if (!unique.toBoolean() || !ListContainsElement(l, p)) {
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
                        arangodb::AqlTransaction* trx,
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

  try {
    AqlValueMaterializer materializer(trx);
    VPackSlice slice = materializer.slice(list);

    TransactionBuilderLeaser builder(trx);
    builder->openArray();
    auto iterator = VPackArrayIterator(slice);
    while (iterator.valid() && !iterator.isLast()) {
      builder->add(iterator.value());
      iterator.next();
    }
    builder->close();
    return AqlValue(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  } 
}

/// @brief function APPEND
AqlValue Functions::Append(arangodb::aql::Query* query,
                           arangodb::AqlTransaction* trx,
                           VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "APPEND", 2, 3);
  AqlValue list = ExtractFunctionParameterValue(trx, parameters, 0);
  AqlValue toAppend = ExtractFunctionParameterValue(trx, parameters, 1);

  if (toAppend.isNull(true)) {
    return list.clone();
  }

  bool unique = false;
  if (parameters.size() == 3) {
    AqlValue a = ExtractFunctionParameterValue(trx, parameters, 2);
    unique = a.toBoolean();
  }
  
  AqlValueMaterializer toAppendMaterializer(trx);
  VPackSlice t = toAppendMaterializer.slice(toAppend);
    
  AqlValueMaterializer materializer(trx);
  VPackSlice l = materializer.slice(list);

  TransactionBuilderLeaser builder(trx);
  builder->openArray();

  if (!list.isNull(true)) {
    TRI_ASSERT(list.isArray());
    for (auto const& it : VPackArrayIterator(l)) {
      builder->add(it);
    }
  }
  if (!toAppend.isArray()) {
    if (!unique || !ListContainsElement(l, t)) {
      builder->add(t);
    }
  } else {
    AqlValueMaterializer materializer(trx);
    VPackSlice slice = materializer.slice(toAppend);
    for (auto const& it : VPackArrayIterator(slice)) {
      if (!unique || !ListContainsElement(l, it)) {
        builder->add(it);
      }
    }
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function UNSHIFT
AqlValue Functions::Unshift(arangodb::aql::Query* query,
                            arangodb::AqlTransaction* trx,
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

  size_t unused;
  if (unique && list.isArray() && ListContainsElement(trx, list, toAppend, unused)) {
    // Short circuit, nothing to do return list
    return list.clone();
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice a = materializer.slice(toAppend);

  TransactionBuilderLeaser builder(trx);
  builder->openArray();
  builder->add(a);
    
  if (list.isArray()) {
    AqlValueMaterializer materializer(trx);
    VPackSlice v = materializer.slice(list);
    for (auto const& it : VPackArrayIterator(v)) {
      builder->add(it);
    }
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function SHIFT
AqlValue Functions::Shift(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
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

  TransactionBuilderLeaser builder(trx);
  builder->openArray();
  
  if (list.length() > 0) {
    AqlValueMaterializer materializer(trx);
    VPackSlice l = materializer.slice(list);

    auto iterator = VPackArrayIterator(l);
    // This jumps over the first element
    while (iterator.next()) {
      builder->add(iterator.value());
    }
  }
  builder->close();

  return AqlValue(builder.get());
}

/// @brief function REMOVE_VALUE
AqlValue Functions::RemoveValue(arangodb::aql::Query* query,
                                arangodb::AqlTransaction* trx,
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

  try {
    TransactionBuilderLeaser builder(trx);
    builder->openArray();
    bool useLimit = false;
    int64_t limit = list.length();

    if (parameters.size() == 3) {
      AqlValue limitValue = ExtractFunctionParameterValue(trx, parameters, 2);
      if (!limitValue.isNull(true)) {
        limit = limitValue.toInt64();
        useLimit = true;
      }
    }
    
    AqlValue toRemove = ExtractFunctionParameterValue(trx, parameters, 1);
    AqlValueMaterializer toRemoveMaterializer(trx);
    VPackSlice r = toRemoveMaterializer.slice(toRemove);

    AqlValueMaterializer materializer(trx);
    VPackSlice v = materializer.slice(list);

    for (auto const& it : VPackArrayIterator(v)) {
      if (useLimit && limit == 0) {
        // Just copy
        builder->add(it);
        continue;
      }
      if (arangodb::basics::VelocyPackHelper::compare(r, it, false) == 0) {
        --limit;
        continue;
      }
      builder->add(it);
    }
    builder->close();
    return AqlValue(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

/// @brief function REMOVE_VALUES
AqlValue Functions::RemoveValues(arangodb::aql::Query* query,
                                 arangodb::AqlTransaction* trx,
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
  
  try {
    AqlValueMaterializer valuesMaterializer(trx);
    VPackSlice v = valuesMaterializer.slice(values);

    AqlValueMaterializer listMaterializer(trx);
    VPackSlice l = listMaterializer.slice(list);

    TransactionBuilderLeaser builder(trx);
    builder->openArray();
    for (auto const& it : VPackArrayIterator(l)) {
      if (!ListContainsElement(v, it)) {
        builder->add(it);
      }
    }
    builder->close();
    return AqlValue(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

/// @brief function REMOVE_NTH
AqlValue Functions::RemoveNth(arangodb::aql::Query* query,
                              arangodb::AqlTransaction* trx,
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
  double p = position.toDouble();
  if (p >= count || p < -count) {
    // out of bounds
    return list.clone();
  }

  if (p < 0) {
    p += count;
  }

  try {
    AqlValueMaterializer materializer(trx);
    VPackSlice v = materializer.slice(list);

    TransactionBuilderLeaser builder(trx);
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
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

/// @brief function NOT_NULL
AqlValue Functions::NotNull(arangodb::aql::Query* query,
                            arangodb::AqlTransaction* trx,
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
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
    VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "CURRENT_DATABASE", 0, 0);

  TransactionBuilderLeaser builder(trx);
  builder->add(VPackValue(query->vocbase()->_name));
  return AqlValue(builder.get());
}

/// @brief function COLLECTION_COUNT
AqlValue Functions::CollectionCount(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
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
  auto document = trx->documentCollection(cid);

  if (document == nullptr) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                  "'%s'", collectionName.c_str());
  }

  TransactionBuilderLeaser builder(trx);
  builder->add(VPackValue(document->size()));
  return AqlValue(builder.get());
}

/// @brief function VARIANCE_SAMPLE
AqlValue Functions::VarianceSample(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
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

  return NumberValue(trx, value / (count - 1));
}

/// @brief function VARIANCE_POPULATION
AqlValue Functions::VariancePopulation(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
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

  return NumberValue(trx, value / count);
}

/// @brief function STDDEV_SAMPLE
AqlValue Functions::StdDevSample(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
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

  return NumberValue(trx, sqrt(value / (count - 1)));
}

/// @brief function STDDEV_POPULATION
AqlValue Functions::StdDevPopulation(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
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

  return NumberValue(trx, sqrt(value / count));
}

/// @brief function MEDIAN
AqlValue Functions::Median(arangodb::aql::Query* query,
                           arangodb::AqlTransaction* trx,
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
    return NumberValue(trx, (values[midpoint - 1] + values[midpoint]) / 2);
  }
  return NumberValue(trx, values[midpoint]);
}

/// @brief function PERCENTILE
AqlValue Functions::Percentile(arangodb::aql::Query* query,
                               arangodb::AqlTransaction* trx,
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
  double p = border.toDouble(unused);
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
    return NumberValue(trx, values[0]);
  }

  TRI_ASSERT(l > 1);

  if (useInterpolation) {
    double const idx = p * (l + 1) / 100.0;
    double const pos = floor(idx);

    if (pos >= l) {
      return NumberValue(trx, values[l - 1]);
    } 
    if (pos <= 0) {
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    } 
    
    double const delta = idx - pos;
    return NumberValue(trx, delta * (values[static_cast<size_t>(pos)] -
                                     values[static_cast<size_t>(pos) - 1]) +
                                  values[static_cast<size_t>(pos) - 1]);
  }

  double const idx = p * l / 100.0;
  double const pos = ceil(idx);
  if (pos >= l) {
    return NumberValue(trx, values[l - 1]);
  } 
  if (pos <= 0) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  } 
    
  return NumberValue(trx, values[static_cast<size_t>(pos) - 1]);
}

/// @brief function RANGE
AqlValue Functions::Range(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "RANGE", 2, 3);

  AqlValue left = ExtractFunctionParameterValue(trx, parameters, 0);
  AqlValue right = ExtractFunctionParameterValue(trx, parameters, 1);

  bool unused = true;
  double from = left.toDouble(unused);
  double to = right.toDouble(unused);

  if (parameters.size() < 3) {
    return AqlValue(left.toInt64(), right.toInt64());
  }

  AqlValue stepValue = ExtractFunctionParameterValue(trx, parameters, 2);
  if (stepValue.isNull(true)) {
    // no step specified
    return AqlValue(left.toInt64(), right.toInt64());
  } 
  
  double step = stepValue.toDouble(unused);

  if (step == 0.0 || (from < to && step < 0.0) || (from > to && step > 0.0)) {
    RegisterWarning(query, "RANGE",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  try {
    TransactionBuilderLeaser builder(trx);
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
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

/// @brief function POSITION
AqlValue Functions::Position(arangodb::aql::Query* query,
                             arangodb::AqlTransaction* trx,
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

    size_t index;
    if (ListContainsElement(trx, list, searchValue, index)) {
      if (!returnIndex) {
        // return true
        return AqlValue(arangodb::basics::VelocyPackHelper::TrueValue());
      }
      // return position
      TransactionBuilderLeaser builder(trx);
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
  TransactionBuilderLeaser builder(trx);
  builder->add(VPackValue(-1));
  return AqlValue(builder.get());
}

/// @brief function FULLTEXT
AqlValue Functions::Fulltext(arangodb::aql::Query* query,
                             arangodb::AqlTransaction* trx,
                             VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "FULLTEXT", 3, 4);

  AqlValue collection = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!collection.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FULLTEXT");
  }

  std::string const collectionName(collection.slice().copyString());

  AqlValue attribute = ExtractFunctionParameterValue(trx, parameters, 1);

  if (!attribute.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FULLTEXT");
  }

  std::string attributeName(attribute.slice().copyString());

  AqlValue queryValue = ExtractFunctionParameterValue(trx, parameters, 2);

  if (!queryValue.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FULLTEXT");
  }

  std::string queryString = queryValue.slice().copyString();

  size_t maxResults = 0;  // 0 means "all results"
  if (parameters.size() >= 4) {
    AqlValue limit = ExtractFunctionParameterValue(trx, parameters, 3);
    if (!limit.isNull(true) && !limit.isNumber()) {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FULLTEXT");
    } 
    if (limit.isNumber()) {
      int64_t value = limit.toInt64();
      if (value > 0) {
        maxResults = static_cast<size_t>(value);
      }
    }
  }

  auto resolver = trx->resolver();
  TRI_voc_cid_t cid = resolver->getCollectionIdLocal(collectionName);
  trx->addCollectionAtRuntime(cid, collectionName);

  auto document = trx->documentCollection(cid);

  if (document == nullptr) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                  "", collectionName.c_str());
  }

  arangodb::Index* index = nullptr;

  std::vector<std::vector<arangodb::basics::AttributeName>> const search(
      {{arangodb::basics::AttributeName(attributeName, false)}});

  for (auto const& idx : document->allIndexes()) {
    if (idx->type() == arangodb::Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {
      // test if index is on the correct field
      if (arangodb::basics::AttributeName::isIdentical(idx->fields(), search,
                                                       false)) {
        // match!
        index = idx;
        break;
      }
    }
  }

  if (index == nullptr) {
    // fiddle collection name into error message
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FULLTEXT_INDEX_MISSING,
                                  collectionName.c_str());
  }

  trx->orderDitch(cid);

  TRI_fulltext_query_t* ft =
      TRI_CreateQueryFulltextIndex(TRI_FULLTEXT_SEARCH_MAX_WORDS, maxResults);

  if (ft == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  bool isSubstringQuery = false;
  int res =
      TRI_ParseQueryFulltextIndex(ft, queryString.c_str(), &isSubstringQuery);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeQueryFulltextIndex(ft);
    THROW_ARANGO_EXCEPTION(res);
  }

  auto fulltextIndex = static_cast<arangodb::FulltextIndex*>(index);
  // note: the following call will free "ft"!
  TRI_fulltext_result_t* queryResult =
      TRI_QueryFulltextIndex(fulltextIndex->internals(), ft);

  if (queryResult == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  TransactionBuilderLeaser builder(trx);
  try {
    builder->openArray();

    size_t const numResults = queryResult->_numDocuments;
    for (size_t i = 0; i < numResults; ++i) {
      InsertMasterPointer((TRI_doc_mptr_t const*)queryResult->_documents[i],
                          *builder.get());
    }
    builder->close();
    TRI_FreeResultFulltextIndex(queryResult);
    return AqlValue(builder.get());
  } catch (...) {
    TRI_FreeResultFulltextIndex(queryResult);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

/// @brief function IS_SAME_COLLECTION
AqlValue Functions::IsSameCollection(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
    VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "IS_SAME_COLLECTION", 2, 2);

  AqlValue first = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!first.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "IS_SAME_COLLECTION");
  }

  std::string const collectionName(first.slice().copyString());

  AqlValue value = ExtractFunctionParameterValue(trx, parameters, 1);
  std::string identifier;

  if (value.isObject() && value.hasKey(trx, TRI_VOC_ATTRIBUTE_ID)) {
    bool localMustDestroy;
    value = value.get(trx, TRI_VOC_ATTRIBUTE_ID, localMustDestroy, false);
    AqlValueGuard guard(value, localMustDestroy);

    if (value.isString()) {
      identifier = value.slice().copyString();
    }
  } else if (value.isString()) {
    identifier = value.slice().copyString();
  }

  if (!identifier.empty()) {
    size_t pos = identifier.find('/');

    if (pos != std::string::npos) {
      bool const isSame = (collectionName == identifier.substr(0, pos));
      return AqlValue(isSame);
    }

    // fallthrough intentional
  }

  RegisterWarning(query, "IS_SAME_COLLECTION",
                  TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
}

