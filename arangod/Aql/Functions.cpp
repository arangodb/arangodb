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
#include "Basics/JsonHelper.h"
#include "Basics/json-utilities.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringBuffer.h"
#include "Basics/Utf8Helper.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "FulltextIndex/fulltext-index.h"
#include "FulltextIndex/fulltext-result.h"
#include "FulltextIndex/fulltext-query.h"
#include "Indexes/Index.h"
#include "Indexes/FulltextIndex.h"
#include "Indexes/GeoIndex2.h"
#include "Rest/SslInterface.h"
#include "V8Server/V8Traverser.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/VocShaper.h"

#include <velocypack/Collection.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;
using Json = arangodb::basics::Json;
using CollectionNameResolver = arangodb::CollectionNameResolver;
using VertexId = arangodb::traverser::VertexId;

////////////////////////////////////////////////////////////////////////////////
/// @brief thread-local cache for compiled regexes
////////////////////////////////////////////////////////////////////////////////

thread_local std::unordered_map<std::string, RegexMatcher*>* RegexCache =
    nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief Transform old AQLValues to new AqlValues
///        Only temporary function
////////////////////////////////////////////////////////////////////////////////

static VPackFunctionParameters transformParameters(
    FunctionParameters const& oldParams, arangodb::AqlTransaction* trx) {
  VPackFunctionParameters newParams;
  for (auto const& it : oldParams) {
    newParams.emplace_back(it.first, trx, it.second);
  }
  return newParams;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clear the regex cache in a thread
////////////////////////////////////////////////////////////////////////////////

static void ClearRegexCache() {
  if (RegexCache != nullptr) {
    for (auto& it : *RegexCache) {
      delete it.second;
    }
    delete RegexCache;
    RegexCache = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compile a regex pattern from a string
////////////////////////////////////////////////////////////////////////////////

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
          pattern.append(".*");
        }
      } else if (c == '_') {
        if (escaped) {
          // literal underscore
          pattern.push_back('_');
        } else {
          // wildcard character
          pattern.push_back('.');
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

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a function parameter from the arguments list
////////////////////////////////////////////////////////////////////////////////

static Json ExtractFunctionParameter(arangodb::AqlTransaction* trx,
                                     FunctionParameters const& parameters,
                                     size_t position, bool copy) {
  if (position >= parameters.size()) {
    // parameter out of range
    return Json(Json::Null);
  }

  auto const& parameter = parameters[position];
  return parameter.first.toJson(trx, parameter.second, copy);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a function parameter from the arguments list
///        VelocyPack variant
////////////////////////////////////////////////////////////////////////////////

static VPackSlice ExtractFunctionParameter(
    arangodb::AqlTransaction* , VPackFunctionParameters const& parameters,
    size_t position) {
  if (position >= parameters.size()) {
    // parameter out of range
    VPackSlice tmp;
    return tmp;
  }
  auto const& parameter = parameters[position];
  return parameter.slice();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register warning
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief register usage of an invalid function argument
////////////////////////////////////////////////////////////////////////////////

static void RegisterInvalidArgumentWarning(arangodb::aql::Query* query,
                                           char const* functionName) {
  RegisterWarning(query, functionName,
                  TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a value into a number value
////////////////////////////////////////////////////////////////////////////////

static double ValueToNumber(TRI_json_t const* json, bool& isValid) {
  switch (json->_type) {
    case TRI_JSON_NULL: {
      isValid = true;
      return 0.0;
    }
    case TRI_JSON_BOOLEAN: {
      isValid = true;
      return (json->_value._boolean ? 1.0 : 0.0);
    }

    case TRI_JSON_NUMBER: {
      isValid = true;
      return json->_value._number;
    }

    case TRI_JSON_STRING:
    case TRI_JSON_STRING_REFERENCE: {
      std::string const str =
          arangodb::basics::JsonHelper::getStringValue(json, "");
      try {
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
      // fall-through to invalidity
      break;
    }

    case TRI_JSON_ARRAY: {
      size_t const n = TRI_LengthVector(&json->_value._objects);
      if (n == 0) {
        isValid = true;
        return 0.0;
      }
      if (n == 1) {
        json = static_cast<TRI_json_t const*>(
            TRI_AtVector(&json->_value._objects, 0));
        return ValueToNumber(json, isValid);
      }
      break;
    }

    case TRI_JSON_OBJECT:
    case TRI_JSON_UNUSED: {
      break;
    }
  }

  isValid = false;
  return 0.0;
}

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

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a value into a boolean value
////////////////////////////////////////////////////////////////////////////////

static bool ValueToBoolean(VPackSlice const& slice) {
  if (slice.isBoolean()) {
    return slice.getBoolean();
  }
  if (slice.isNumber()) {
    return slice.getNumericValue<double>() != 0.0;
  }
  if (slice.isString()) {
    return slice.getStringLength() != 0;
  }
  if (slice.isArray() || slice.isObject()) {
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a boolean parameter from an array
////////////////////////////////////////////////////////////////////////////////

static bool GetBooleanParameter(arangodb::AqlTransaction* trx,
                                VPackFunctionParameters const& parameters,
                                size_t startParameter, bool defaultValue) {
  size_t const n = parameters.size();

  if (startParameter >= n) {
    return defaultValue;
  }

  auto temp = ExtractFunctionParameter(trx, parameters, startParameter);
  return ValueToBoolean(temp);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract attribute names from the arguments
////////////////////////////////////////////////////////////////////////////////

static void ExtractKeys(std::unordered_set<std::string>& names,
                        arangodb::aql::Query* query,
                        arangodb::AqlTransaction* trx,
                        VPackFunctionParameters const& parameters,
                        size_t startParameter, char const* functionName) {
  size_t const n = parameters.size();

  for (size_t i = startParameter; i < n; ++i) {
    auto param = ExtractFunctionParameter(trx, parameters, i);

    if (param.isString()) {
      names.emplace(param.copyString());
    } else if (param.isNumber()) {
      double number = param.getNumericValue<double>();

      if (std::isnan(number) || number == HUGE_VAL || number == -HUGE_VAL) {
        names.emplace("null");
      } else {
        char buffer[24];
        int length = fpconv_dtoa(number, &buffer[0]);
        names.emplace(std::string(&buffer[0], static_cast<size_t>(length)));
      }
    } else if (param.isArray()) {
      for (auto const& v : VPackArrayIterator(param)) {
        if (v.isString()) {
          names.emplace(v.copyString());
        } else {
          RegisterInvalidArgumentWarning(query, functionName);
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append the JSON value to a string buffer
////////////////////////////////////////////////////////////////////////////////

static void AppendAsString(arangodb::basics::StringBuffer& buffer,
                           TRI_json_t const* json) {
  TRI_json_type_e const type =
      (json == nullptr ? TRI_JSON_UNUSED : json->_type);

  switch (type) {
    case TRI_JSON_UNUSED:
    case TRI_JSON_NULL: {
      buffer.appendText(TRI_CHAR_LENGTH_PAIR("null"));
      break;
    }
    case TRI_JSON_BOOLEAN: {
      if (json->_value._boolean) {
        buffer.appendText(TRI_CHAR_LENGTH_PAIR("true"));
      } else {
        buffer.appendText(TRI_CHAR_LENGTH_PAIR("false"));
      }
      break;
    }
    case TRI_JSON_NUMBER: {
      buffer.appendDecimal(json->_value._number);
      break;
    }
    case TRI_JSON_STRING:
    case TRI_JSON_STRING_REFERENCE: {
      buffer.appendText(arangodb::basics::JsonHelper::getStringValue(json, ""));
      break;
    }
    case TRI_JSON_ARRAY: {
      size_t const n = TRI_LengthArrayJson(json);
      for (size_t i = 0; i < n; ++i) {
        if (i > 0) {
          buffer.appendChar(',');
        }
        AppendAsString(buffer, static_cast<TRI_json_t const*>(
                                   TRI_AtVector(&json->_value._objects, i)));
      }
      break;
    }
    case TRI_JSON_OBJECT: {
      buffer.appendText(TRI_CHAR_LENGTH_PAIR("[object Object]"));
      break;
    }
  }

  // make it null-terminated for all c-string-related functions
  buffer.ensureNullTerminated();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append the VelocyPack value to a string buffer
///        Note: Backwards compatibility. Is different than Slice.toJson()
////////////////////////////////////////////////////////////////////////////////

static void AppendAsString(arangodb::basics::VPackStringBufferAdapter& buffer,
                           VPackSlice const& slice) {
  if (slice.isString()) {
    // dumping adds additional ''
    buffer.append(slice.copyString());
  } else if (slice.isArray()) {
    bool first = true;
    for (auto const& sub : VPackArrayIterator(slice)) {
      if (!first) {
        buffer.append(",");
      } else {
        first = false;
      }
      AppendAsString(buffer, sub);
    }
  } else if (slice.isObject()) {
    buffer.append("[object Object]");
  } else {
    VPackDumper dumper(&buffer);
    dumper.dump(slice);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if the given list contains the element
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief Computes the Variance of the given list.
///        If successful value will contain the variance and count
///        will contain the number of elements.
///        If not successful value and count contain garbage.
////////////////////////////////////////////////////////////////////////////////

static bool Variance(VPackSlice const& values, double& value, size_t& count) {
  TRI_ASSERT(values.isArray());
  value = 0.0;
  count = 0;
  bool unused = false;
  double mean = 0;
  for (auto const& element : VPackArrayIterator(values)) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief Sorts the given list of Numbers in ASC order.
///        Removes all null entries.
///        Returns false if the list contains non-number values.
////////////////////////////////////////////////////////////////////////////////

static bool SortNumberList(VPackSlice const& values,
                           std::vector<double>& result) {
  TRI_ASSERT(values.isArray());
  TRI_ASSERT(result.empty());
  bool unused;
  for (auto const& element : VPackArrayIterator(values)) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief Expands a shaped Json
////////////////////////////////////////////////////////////////////////////////

static inline Json ExpandShapedJson(VocShaper* shaper,
                                    CollectionNameResolver const* resolver,
                                    TRI_voc_cid_t const& cid,
                                    TRI_doc_mptr_t const* mptr) {
  TRI_df_marker_t const* marker =
      static_cast<TRI_df_marker_t const*>(mptr->getDataPtr());

  TRI_shaped_json_t shaped;
  TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, marker);
  Json json(shaper->memoryZone(), TRI_JsonShapedJson(shaper, &shaped));
  char const* key = TRI_EXTRACT_MARKER_KEY(marker);
  std::string id(resolver->getCollectionName(cid));
  id.push_back('/');
  id.append(key);
  json(TRI_VOC_ATTRIBUTE_ID, Json(id));
  json(TRI_VOC_ATTRIBUTE_REV,
       Json(std::to_string(TRI_EXTRACT_MARKER_RID(marker))));
  json(TRI_VOC_ATTRIBUTE_KEY, Json(key));

  if (TRI_IS_EDGE_MARKER(marker)) {
    std::string from(resolver->getCollectionNameCluster(
        TRI_EXTRACT_MARKER_FROM_CID(marker)));
    from.push_back('/');
    from.append(TRI_EXTRACT_MARKER_FROM_KEY(marker));
    json(TRI_VOC_ATTRIBUTE_FROM, Json(from));
    std::string to(
        resolver->getCollectionNameCluster(TRI_EXTRACT_MARKER_TO_CID(marker)));

    to.push_back('/');
    to.append(TRI_EXTRACT_MARKER_TO_KEY(marker));
    json(TRI_VOC_ATTRIBUTE_TO, Json(to));
  }

  return json;
}

static inline void ExpandShapedJson(
    VocShaper* shaper, CollectionNameResolver const* resolver,
    TRI_voc_cid_t const& cid, TRI_doc_mptr_t const* mptr, VPackBuilder& b,
    bool keepTopLevelOpen,
    std::unordered_set<std::string> const& forbidden) {
  b.add(VPackValue(VPackValueType::Object));

  TRI_df_marker_t const* marker =
      static_cast<TRI_df_marker_t const*>(mptr->getDataPtr());

  TRI_shaped_json_t shaped;
  TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, marker);
  std::shared_ptr<VPackBuilder> tmp = TRI_VelocyPackShapedJson(shaper, &shaped); 
  // Copy the shaped into our local builder
  for (auto const& it : VPackObjectIterator(tmp->slice())) {
    std::string key = it.key.copyString();
    if (forbidden.count(key) == 0) {
      b.add(it.key.copyString(), it.value);
    }
  }

  char const* key = TRI_EXTRACT_MARKER_KEY(marker);
  std::string id(resolver->getCollectionName(cid));
  id.push_back('/');
  id.append(key);
  if (forbidden.count(TRI_VOC_ATTRIBUTE_ID) == 0) {
    b.add(TRI_VOC_ATTRIBUTE_ID, VPackValue(id));
  }
  if (forbidden.count(TRI_VOC_ATTRIBUTE_REV) == 0) {
    b.add(TRI_VOC_ATTRIBUTE_REV,
          VPackValue(std::to_string(TRI_EXTRACT_MARKER_RID(marker))));
  }
  if (forbidden.count(TRI_VOC_ATTRIBUTE_KEY) == 0) {
    b.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(key));
  }

  if (TRI_IS_EDGE_MARKER(marker)) {
    if (forbidden.count(TRI_VOC_ATTRIBUTE_FROM) == 0) {
      std::string from(resolver->getCollectionNameCluster(
          TRI_EXTRACT_MARKER_FROM_CID(marker)));
      from.push_back('/');
      from.append(TRI_EXTRACT_MARKER_FROM_KEY(marker));
      b.add(TRI_VOC_ATTRIBUTE_FROM, VPackValue(from));
    }

    if (forbidden.count(TRI_VOC_ATTRIBUTE_TO) == 0) {
      std::string to(
          resolver->getCollectionNameCluster(TRI_EXTRACT_MARKER_TO_CID(marker)));

      to.push_back('/');
      to.append(TRI_EXTRACT_MARKER_TO_KEY(marker));
      b.add(TRI_VOC_ATTRIBUTE_TO, VPackValue(to));
    }
  }
  if (!keepTopLevelOpen) {
    b.close();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Reads a document by cid and key
///        Also lazy locks the collection.
///        Returns null if the document does not exist
////////////////////////////////////////////////////////////////////////////////

static void ReadDocument(arangodb::AqlTransaction* trx,
                         CollectionNameResolver const* resolver,
                         TRI_voc_cid_t cid, char const* key,
                         VPackBuilder& result) {
  auto collection = trx->trxCollection(cid);

  if (collection == nullptr) {
    int res = TRI_AddCollectionTransaction(trx->getInternals(), cid,
                                           TRI_TRANSACTION_READ,
                                           trx->nestingLevel(), true, true);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
    TRI_EnsureCollectionsTransaction(trx->getInternals());
    collection = trx->trxCollection(cid);

    if (collection == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "collection is a nullptr");
    }
  }

  TRI_doc_mptr_copy_t mptr;
  int res = trx->document(collection, &mptr, key);

  if (res != TRI_ERROR_NO_ERROR) {
    result.add(VPackValue(VPackValueType::Null));
  } else {
    std::unordered_set<std::string> unused;
    ExpandShapedJson(collection->_collection->_collection->getShaper(),
                     resolver, cid, &mptr, result, false, unused);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function to filter the given list of mptr
////////////////////////////////////////////////////////////////////////////////

static void FilterDocuments(arangodb::ExampleMatcher const* matcher,
                            TRI_voc_cid_t cid,
                            std::vector<TRI_doc_mptr_copy_t>& toFilter) {
  if (matcher == nullptr) {
    return;
  }
  size_t resultCount = toFilter.size();
  for (size_t i = 0; i < resultCount; /* nothing */) {
    if (!matcher->matches(cid, &toFilter[i])) {
      toFilter.erase(toFilter.begin() + i);
      --resultCount;
    } else {
      ++i;
    }
  }
}

static void RequestEdges(VPackSlice const& vertexSlice,
                         arangodb::AqlTransaction* trx,
                         CollectionNameResolver const* resolver,
                         VocShaper* shaper, TRI_voc_cid_t cid,
                         TRI_document_collection_t* collection,
                         TRI_edge_direction_e direction,
                         arangodb::ExampleMatcher const* matcher,
                         bool includeVertices, VPackBuilder& result) {
  std::string vertexId;
  if (vertexSlice.isString()) {
    vertexId = vertexSlice.copyString();
  } else if (vertexSlice.isObject()) {
    vertexId = arangodb::basics::VelocyPackHelper::getStringValue(vertexSlice,
                                                                  "_id", "");
  } else {
    // Nothing to do.
    // Return (error for illegal input is thrown outside
    return;
  }

  std::vector<std::string> parts =
      arangodb::basics::StringUtils::split(vertexId, "/");
  if (parts.size() != 2) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD,
                                   vertexId);
  }

  TRI_voc_cid_t startCid = resolver->getCollectionIdLocal(parts[0]);
  if (startCid == 0) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, "'%s'",
                                  parts[0].c_str());
  }

  char* key = const_cast<char*>(parts[1].c_str());
  std::vector<TRI_doc_mptr_copy_t> edges = TRI_LookupEdgesDocumentCollection(
      trx, collection, direction, startCid, key);
  FilterDocuments(matcher, cid, edges);
  size_t resultCount = edges.size();

  if (includeVertices) {
    for (size_t i = 0; i < resultCount; ++i) {
      VPackObjectBuilder guard(&result);
      result.add(VPackValue("edge"));
      std::unordered_set<std::string> unused;
      ExpandShapedJson(shaper, resolver, cid, &(edges[i]), result, false, unused);
      char const* targetKey = nullptr;
      TRI_voc_cid_t targetCid = 0;

      switch (direction) {
        case TRI_EDGE_OUT:
          targetKey = TRI_EXTRACT_MARKER_TO_KEY(&edges[i]);
          targetCid = TRI_EXTRACT_MARKER_TO_CID(&edges[i]);
          break;
        case TRI_EDGE_IN:
          targetKey = TRI_EXTRACT_MARKER_FROM_KEY(&edges[i]);
          targetCid = TRI_EXTRACT_MARKER_FROM_CID(&edges[i]);
          break;
        case TRI_EDGE_ANY:
          targetKey = TRI_EXTRACT_MARKER_TO_KEY(&edges[i]);
          targetCid = TRI_EXTRACT_MARKER_TO_CID(&edges[i]);
          if (targetCid == startCid && strcmp(targetKey, key) == 0) {
            targetKey = TRI_EXTRACT_MARKER_FROM_KEY(&edges[i]);
            targetCid = TRI_EXTRACT_MARKER_FROM_CID(&edges[i]);
          }
          break;
      }

      if (targetKey == nullptr || targetCid == 0) {
        // somehow invalid
        continue;
      }

      result.add(VPackValue("vertex"));
      ReadDocument(trx, resolver, targetCid, targetKey, result);
    }
  } else {
    for (size_t i = 0; i < resultCount; ++i) {
      std::unordered_set<std::string> unused;
      ExpandShapedJson(shaper, resolver, cid, &(edges[i]), result, false, unused);
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Helper function to unset or keep all given names in the value.
///        Recursively iterates over sub-object and unsets or keeps their values
///        as well
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief Helper function to merge given parameters
///        Works for an array of objects as first parameter or arbitrary many
///        object parameters
////////////////////////////////////////////////////////////////////////////////

static AqlValue$ MergeParameters(arangodb::aql::Query* query,
                                 arangodb::AqlTransaction* trx,
                                 VPackFunctionParameters const& parameters,
                                 std::string const& funcName,
                                 bool recursive) {
  size_t const n = parameters.size();
  if (n == 0) {
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    {
      VPackObjectBuilder guard(b.get());
    }
    return AqlValue$(b.get());
  }
  // use the first argument as the preliminary result
  VPackBuilder b;
  auto initial = ExtractFunctionParameter(trx, parameters, 0);
  if (initial.isArray() && n == 1) {
    // special case: a single array parameter
    try {
      // Create an empty document as start point
      VPackObjectBuilder guard(&b);
    } catch (...) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    // merge in all other arguments
    for (auto const& it : VPackArrayIterator(initial)) {
      if (!it.isObject()) {
        RegisterInvalidArgumentWarning(query, funcName.c_str());
        b.clear();
        b.add(VPackValue(VPackValueType::Null));
        return AqlValue$(b);
      }
      try {
        b = arangodb::basics::VelocyPackHelper::merge(b.slice(), it, false,
                                                      recursive);
      } catch (...) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
    }
    return AqlValue$(b);
  }

  if (!initial.isObject()) {
    RegisterInvalidArgumentWarning(query, funcName.c_str());
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  // merge in all other arguments
  for (size_t i = 1; i < n; ++i) {
    auto param = ExtractFunctionParameter(trx, parameters, i);

    if (!param.isObject()) {
      RegisterInvalidArgumentWarning(query, funcName.c_str());
      std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
      b->add(VPackValue(VPackValueType::Null));
      return AqlValue$(b.get());
    }

    try {
      b = arangodb::basics::VelocyPackHelper::merge(initial, param, false,
                                                    recursive);
      initial = b.slice();
    } catch (...) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }
  return AqlValue$(b);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Transforms VertexId to VelocyPack
////////////////////////////////////////////////////////////////////////////////

static void VertexIdToVPack(arangodb::AqlTransaction* trx,
                            CollectionNameResolver const* resolver,
                            VertexId const& id,
                            VPackBuilder& b) {
  auto collection = trx->trxCollection(id.cid);

  if (collection == nullptr) {
    int res = TRI_AddCollectionTransaction(trx->getInternals(), id.cid,
                                           TRI_TRANSACTION_READ,
                                           trx->nestingLevel(), true, true);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    TRI_EnsureCollectionsTransaction(trx->getInternals());
    collection = trx->trxCollection(id.cid);

    if (collection == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "collection is a nullptr");
    }
  }

  TRI_doc_mptr_copy_t mptr;
  int res = trx->document(collection, &mptr, id.key);

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
      b.add(VPackValue(VPackValueType::Null));
      return;
    }
    THROW_ARANGO_EXCEPTION(res);
  }

  std::unordered_set<std::string> unused;
  ExpandShapedJson(collection->_collection->_collection->getShaper(), resolver,
                   id.cid, &mptr, b, false, unused);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief Transforms VertexId to std::string
////////////////////////////////////////////////////////////////////////////////

static std::string VertexIdToString(CollectionNameResolver const* resolver,
                                    VertexId const& id) {
  return resolver->getCollectionName(id.cid) + "/" + std::string(id.key);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Transforms an unordered_map<VertexId> to AQL json values
////////////////////////////////////////////////////////////////////////////////

static AqlValue$ VertexIdsToAqlValueVPack(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
    CollectionNameResolver const* resolver, std::unordered_set<VertexId>& ids,
    bool includeData = false) {
  std::shared_ptr<VPackBuilder> result = query->getSharedBuilder();
  {
    VPackArrayBuilder b(result.get());
    if (includeData) {
      for (auto& it : ids) {
        VertexIdToVPack(trx, resolver, it, *result);
      }
    } else {
      for (auto& it : ids) {
        result->add(VPackValue(VertexIdToString(resolver, it)));
      }
    }
  }

  return AqlValue$(result.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Load geoindex for collection name
////////////////////////////////////////////////////////////////////////////////

static arangodb::Index* getGeoIndex(arangodb::AqlTransaction* trx,
                                    TRI_voc_cid_t const& cid,
                                    std::string const& colName,
                                    VocShaper*& shaper) {
  auto collection = trx->trxCollection(cid);

  // ensure the collection is loaded
  if (collection == nullptr) {
    int res = TRI_AddCollectionTransaction(trx->getInternals(), cid,
                                           TRI_TRANSACTION_READ,
                                           trx->nestingLevel(), true, true);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_FORMAT(res, "'%s'", colName.c_str());
    }

    TRI_EnsureCollectionsTransaction(trx->getInternals());
    collection = trx->trxCollection(cid);

    if (collection == nullptr) {
      THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                    "'%s'", colName.c_str());
    }
  }

  auto document = trx->documentCollection(cid);

  if (document == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
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
                                  colName.c_str());
  }

  if (trx->orderDitch(collection) == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  shaper = collection->_collection->_collection->getShaper();
  return index;
}

static AqlValue$ buildGeoResult(arangodb::aql::Query* query,
                                GeoCoordinates* cors, VocShaper* shaper,
                                CollectionNameResolver const* resolver,
                                TRI_voc_cid_t const& cid,
                                std::string const& attributeName) {
  if (cors == nullptr) {
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    {
      VPackArrayBuilder guard(b.get());
    }
    return AqlValue$(b.get());
  }

  size_t const nCoords = cors->length;
  if (nCoords == 0) {
    GeoIndex_CoordinatesFree(cors);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    {
      VPackArrayBuilder guard(b.get());
    }
    return AqlValue$(b.get());
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

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  try {
    VPackArrayBuilder guard(b.get());
    std::unordered_set<std::string> forbidden;
    bool saveAttr = !attributeName.empty();
    if (saveAttr) {
      forbidden.emplace(attributeName);
    }

    for (auto& it : distances) {
      ExpandShapedJson(shaper, resolver, cid, it._mptr, *b,
                       saveAttr, forbidden);
      if (saveAttr) {
        // The Object is Open and attributeName is not set
        b->add(attributeName, VPackValue(it._distance));
        b->close();
      }
    }
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief internal recursive flatten helper
////////////////////////////////////////////////////////////////////////////////

static void FlattenList(Json array, size_t maxDepth, size_t curDepth,
                        Json& result) {
  size_t elementCount = array.size();
  for (size_t i = 0; i < elementCount; ++i) {
    Json tmp = array.at(i);
    if (tmp.isArray() && curDepth < maxDepth) {
      FlattenList(tmp, maxDepth, curDepth + 1, result);
    } else {
      // Transfer the content of tmp into the result
      result.transfer(tmp.json());
    }
  }
}

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

////////////////////////////////////////////////////////////////////////////////
/// @brief called before a query starts
/// has the chance to set up any thread-local storage
////////////////////////////////////////////////////////////////////////////////

void Functions::InitializeThreadContext() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief called when a query ends
/// its responsibility is to clear any thread-local storage
////////////////////////////////////////////////////////////////////////////////

void Functions::DestroyThreadContext() { ClearRegexCache(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_NULL
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsNull(arangodb::aql::Query* q, arangodb::AqlTransaction* trx,
                           FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(IsNullVPack(q, trx, tmp));
}

AqlValue$ Functions::IsNullVPack(arangodb::aql::Query* query,
                                 arangodb::AqlTransaction* trx,
                                 VPackFunctionParameters const& parameters) {
  auto const slice = ExtractFunctionParameter(trx, parameters, 0);
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(slice.isNull()));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_BOOL
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsBool(arangodb::aql::Query* q, arangodb::AqlTransaction* trx,
                           FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(IsBoolVPack(q, trx, tmp));
}

AqlValue$ Functions::IsBoolVPack(arangodb::aql::Query* query,
                                 arangodb::AqlTransaction* trx,
                                 VPackFunctionParameters const& parameters) {
  auto const slice = ExtractFunctionParameter(trx, parameters, 0);
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(slice.isBool()));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_NUMBER
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsNumber(arangodb::aql::Query* q,
                             arangodb::AqlTransaction* trx,
                             FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(IsNumberVPack(q, trx, tmp));
}

AqlValue$ Functions::IsNumberVPack(arangodb::aql::Query* query,
                                   arangodb::AqlTransaction* trx,
                                   VPackFunctionParameters const& parameters) {
  auto const slice = ExtractFunctionParameter(trx, parameters, 0);
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(slice.isNumber()));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_STRING
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsString(arangodb::aql::Query* q,
                             arangodb::AqlTransaction* trx,
                             FunctionParameters const& parameters) {

  auto tmp = transformParameters(parameters, trx);
  return AqlValue(IsStringVPack(q, trx, tmp));
}

AqlValue$ Functions::IsStringVPack(arangodb::aql::Query* query,
                                   arangodb::AqlTransaction* trx,
                                   VPackFunctionParameters const& parameters) {
  auto const slice = ExtractFunctionParameter(trx, parameters, 0);
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(slice.isString()));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_ARRAY
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsArray(arangodb::aql::Query* q,
                            arangodb::AqlTransaction* trx,
                            FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(IsArrayVPack(q, trx, tmp));
}

AqlValue$ Functions::IsArrayVPack(arangodb::aql::Query* query,
                                  arangodb::AqlTransaction* trx,
                                  VPackFunctionParameters const& parameters) {
  auto const slice = ExtractFunctionParameter(trx, parameters, 0);
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(slice.isArray()));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_OBJECT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsObject(arangodb::aql::Query* q,
                             arangodb::AqlTransaction* trx,
                             FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(IsObjectVPack(q, trx, tmp));
}

AqlValue$ Functions::IsObjectVPack(arangodb::aql::Query* query,
                                   arangodb::AqlTransaction* trx,
                                   VPackFunctionParameters const& parameters) {
  auto const slice = ExtractFunctionParameter(trx, parameters, 0);
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(slice.isObject()));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function TO_NUMBER
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::ToNumber(arangodb::aql::Query* q,
                             arangodb::AqlTransaction* trx,
                             FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(ToNumberVPack(q, trx, tmp));
}

AqlValue$ Functions::ToNumberVPack(arangodb::aql::Query* query,
                                  arangodb::AqlTransaction* trx,
                                  VPackFunctionParameters const& parameters) {
  auto const slice = ExtractFunctionParameter(trx, parameters, 0);

  bool isValid;
  double v = ValueToNumber(slice, isValid);

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();

  if (!isValid) {
    b->add(VPackValue(VPackValueType::Null));
  } else {
    b->add(VPackValue(v));
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function TO_STRING
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::ToString(arangodb::aql::Query* q,
                             arangodb::AqlTransaction* trx,
                             FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(ToStringVPack(q, trx, tmp));
}

AqlValue$ Functions::ToStringVPack(arangodb::aql::Query* query,
                                   arangodb::AqlTransaction* trx,
                                   VPackFunctionParameters const& parameters) {
  auto const value = ExtractFunctionParameter(trx, parameters, 0);

  arangodb::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE, 24);

  arangodb::basics::VPackStringBufferAdapter adapter(buffer.stringBuffer());
  AppendAsString(adapter, value);
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  try {
    std::string res(buffer.begin(), buffer.length());
    b->add(VPackValue(res));
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function TO_BOOL
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::ToBool(arangodb::aql::Query* q, arangodb::AqlTransaction* trx,
                           FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(ToBoolVPack(q, trx, tmp));
}

AqlValue$ Functions::ToBoolVPack(arangodb::aql::Query* query,
                                arangodb::AqlTransaction* trx,
                                VPackFunctionParameters const& parameters) {
  auto const value = ExtractFunctionParameter(trx, parameters, 0);
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(ValueToBoolean(value)));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function TO_ARRAY
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::ToArray(arangodb::aql::Query* q,
                            arangodb::AqlTransaction* trx,
                            FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(ToArrayVPack(q, trx, tmp));
}

AqlValue$ Functions::ToArrayVPack(arangodb::aql::Query* query,
                                  arangodb::AqlTransaction* trx,
                                  VPackFunctionParameters const& parameters) {
  auto const value = ExtractFunctionParameter(trx, parameters, 0);

  if (value.isArray()) {
    // return copy of the original array
    return AqlValue$(value);
  }

  std::shared_ptr<VPackBuilder> result = query->getSharedBuilder();
  {
    VPackArrayBuilder b(result.get());
    if (value.isBoolean() || value.isNumber() || value.isString()) {
      // return array with single member
      result->add(value);
    } else if (value.isObject()) {
      // return an array with the attribute values
      for (auto const& it : VPackObjectIterator(value)) {
        result->add(it.value);
      }
    }
    // else return empty array
  }
  return AqlValue$(result.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function LENGTH
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Length(arangodb::aql::Query* q, arangodb::AqlTransaction* trx,
                           FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(LengthVPack(q, trx, tmp));
}

AqlValue$ Functions::LengthVPack(arangodb::aql::Query* q,
                                 arangodb::AqlTransaction* trx,
                                 VPackFunctionParameters const& parameters) {
  auto const value = ExtractFunctionParameter(trx, parameters, 0);
  std::shared_ptr<VPackBuilder> builder = q->getSharedBuilder();
  if (value.isArray()) {
    // shortcut!
    builder->add(VPackValue(static_cast<double>(value.length())));
    return AqlValue$(builder->slice());
  }
  size_t length = 0;
  if (value.isNone() || value.isNull()) {
    length = 0;
  } else if (value.isBoolean()) {
    if (value.getBoolean()) {
      length = 1;
    } else {
      length = 0;
    }
  } else if (value.isNumber()) {
    double tmp = value.getNumericValue<double>();
    if (std::isnan(tmp) || !std::isfinite(tmp)) {
      length = strlen("null");
    } else {
      char buffer[24];
      length = static_cast<size_t>(fpconv_dtoa(tmp, buffer));
    }
  } else if (value.isString()) {
    std::string tmp = value.copyString();
    length = TRI_CharLengthUtf8String(tmp.c_str());
  } else if (value.isObject()) {
    length = static_cast<size_t>(value.length());
  }
  builder->add(VPackValue(static_cast<double>(length)));
  return AqlValue$(builder.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function FIRST
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::First(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
                          FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(FirstVPack(query, trx, tmp));
}

AqlValue$ Functions::FirstVPack(arangodb::aql::Query* query,
                                arangodb::AqlTransaction* trx,
                                VPackFunctionParameters const& parameters) {
  if (parameters.size() < 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "FIRST", (int)1,
        (int)1);
  }

  auto value = ExtractFunctionParameter(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "FIRST", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    std::shared_ptr<VPackBuilder> builder = query->getSharedBuilder();
    builder->add(VPackValue(VPackValueType::Null));
    return AqlValue$(builder.get());
  }

  if (value.length() == 0) {
    std::shared_ptr<VPackBuilder> builder = query->getSharedBuilder();
    builder->add(VPackValue(VPackValueType::Null));
    return AqlValue$(builder.get());
  }

  return AqlValue$(value.at(0));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function LAST
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Last(arangodb::aql::Query* query,
                         arangodb::AqlTransaction* trx,
                         FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(LastVPack(query, trx, tmp));
}

AqlValue$ Functions::LastVPack(arangodb::aql::Query* query,
                               arangodb::AqlTransaction* trx,
                               VPackFunctionParameters const& parameters) {
  if (parameters.size() < 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "LAST", (int)1,
        (int)1);
  }

  auto value = ExtractFunctionParameter(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "LAST", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    std::shared_ptr<VPackBuilder> builder = query->getSharedBuilder();
    builder->add(VPackValue(VPackValueType::Null));
    return AqlValue$(builder.get());
  }

  VPackValueLength const n = value.length();

  if (n == 0) {
    std::shared_ptr<VPackBuilder> builder = query->getSharedBuilder();
    builder->add(VPackValue(VPackValueType::Null));
    return AqlValue$(builder.get());
  }
  return AqlValue$(value.at(n-1));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function NTH
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Nth(arangodb::aql::Query* query,
                        arangodb::AqlTransaction* trx,
                        FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(NthVPack(query, trx, tmp));
}

AqlValue$ Functions::NthVPack(arangodb::aql::Query* query,
                              arangodb::AqlTransaction* trx,
                              VPackFunctionParameters const& parameters) {
  if (parameters.size() < 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "LAST", (int)2,
        (int)2);
  }

  auto value = ExtractFunctionParameter(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "NTH", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    std::shared_ptr<VPackBuilder> builder = query->getSharedBuilder();
    builder->add(VPackValue(VPackValueType::Null));
    return AqlValue$(builder.get());
  }

  VPackValueLength const n = value.length();

  if (n == 0) {
    std::shared_ptr<VPackBuilder> builder = query->getSharedBuilder();
    builder->add(VPackValue(VPackValueType::Null));
    return AqlValue$(builder.get());
  }

  VPackSlice indexSlice = ExtractFunctionParameter(trx, parameters, 1);
  bool isValid = true;
  double numValue = ValueToNumber(indexSlice, isValid);

  if (!isValid || numValue < 0.0) {
    std::shared_ptr<VPackBuilder> builder = query->getSharedBuilder();
    builder->add(VPackValue(VPackValueType::Null));
    return AqlValue$(builder.get());
  }

  size_t index = static_cast<size_t>(numValue);

  if (index >= n) {
    std::shared_ptr<VPackBuilder> builder = query->getSharedBuilder();
    builder->add(VPackValue(VPackValueType::Null));
    return AqlValue$(builder.get());
  }
  return AqlValue$(value.at(index));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function CONCAT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Concat(arangodb::aql::Query* query,
                           arangodb::AqlTransaction* trx,
                           FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(ConcatVPack(query, trx, tmp));
}

AqlValue$ Functions::ConcatVPack(arangodb::aql::Query* query,
                                 arangodb::AqlTransaction* trx,
                                 VPackFunctionParameters const& parameters) {
  arangodb::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE, 24);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer.stringBuffer());

  size_t const n = parameters.size();

  for (size_t i = 0; i < n; ++i) {
    auto const member = ExtractFunctionParameter(trx, parameters, i);

    if (member.isNone() || member.isNull()) {
      continue;
    }

    if (member.isArray()) {
      // append each member individually
      for (auto const& sub : VPackArrayIterator(member)) {
        if (sub.isNone() || sub.isNull()) {
          continue;
        }

        AppendAsString(adapter, sub);
      }
    } else {
      // convert member to a string and append
      AppendAsString(adapter, member);
    }
  }

  // steal the StringBuffer's char* pointer so we can avoid copying data around
  // multiple times
  size_t length = buffer.length();
  try {
    std::shared_ptr<VPackBuilder> builder = query->getSharedBuilder();
    std::string res(buffer.steal(), length);
    builder->add(VPackValue(std::move(res)));
    return AqlValue$(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function LIKE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Like(arangodb::aql::Query* query,
                         arangodb::AqlTransaction* trx,
                         FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(LikeVPack(query, trx, tmp));
}

AqlValue$ Functions::LikeVPack(arangodb::aql::Query* query,
                               arangodb::AqlTransaction* trx,
                               VPackFunctionParameters const& parameters) {
  if (parameters.size() < 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "LIKE", (int)2,
        (int)3);
  }

  bool const caseInsensitive = GetBooleanParameter(trx, parameters, 2, false);
  arangodb::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE, 24);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer.stringBuffer());

  // build pattern from parameter #1
  auto const regex = ExtractFunctionParameter(trx, parameters, 1);
  AppendAsString(adapter, regex);

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
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  // extract value
  buffer.clear();
  auto const value = ExtractFunctionParameter(trx, parameters, 0);
  AppendAsString(adapter, value);

  bool error = false;
  bool const result = arangodb::basics::Utf8Helper::DefaultUtf8Helper.matches(
      matcher, buffer.c_str(), buffer.length(), error);

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  if (error) {
    // compiling regular expression failed
    RegisterWarning(query, "LIKE", TRI_ERROR_QUERY_INVALID_REGEX);
    b->add(VPackValue(VPackValueType::Null));
  } else {
    b->add(VPackValue(result));
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function PASSTHRU
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Passthru(arangodb::aql::Query* query,
                             arangodb::AqlTransaction* trx,
                             FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(PassthruVPack(query, trx, tmp));
}

AqlValue$ Functions::PassthruVPack(arangodb::aql::Query* query,
                                   arangodb::AqlTransaction* trx,
                                   VPackFunctionParameters const& parameters) {
  if (parameters.empty()) {
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  auto value = ExtractFunctionParameter(trx, parameters, 0);
  return AqlValue$(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function UNSET
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Unset(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
                          FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(UnsetVPack(query, trx, tmp));
}

AqlValue$ Functions::UnsetVPack(arangodb::aql::Query* query,
                                arangodb::AqlTransaction* trx,
                                VPackFunctionParameters const& parameters) {
  auto value = ExtractFunctionParameter(trx, parameters, 0);

  if (!value.isObject()) {
    RegisterInvalidArgumentWarning(query, "UNSET");
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  std::unordered_set<std::string> names;
  ExtractKeys(names, query, trx, parameters, 1, "UNSET");

  try {
    std::shared_ptr<VPackBuilder> builder = query->getSharedBuilder();
    UnsetOrKeep(value, names, true, false, *builder);
    return AqlValue$(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function UNSET_RECURSIVE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::UnsetRecursive(arangodb::aql::Query* query,
                                   arangodb::AqlTransaction* trx,
                                   FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(UnsetRecursiveVPack(query, trx, tmp));
}

AqlValue$ Functions::UnsetRecursiveVPack(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
    VPackFunctionParameters const& parameters) {
  auto value = ExtractFunctionParameter(trx, parameters, 0);

  if (!value.isObject()) {
    RegisterInvalidArgumentWarning(query, "UNSET_RECURSIVE");
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  std::unordered_set<std::string> names;
  ExtractKeys(names, query, trx, parameters, 1, "UNSET_RECURSIVE");

  try {
    std::shared_ptr<VPackBuilder> builder = query->getSharedBuilder();
    UnsetOrKeep(value, names, true, true, *builder);
    return AqlValue$(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function KEEP
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Keep(arangodb::aql::Query* query,
                         arangodb::AqlTransaction* trx,
                         FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(KeepVPack(query, trx, tmp));
}

AqlValue$ Functions::KeepVPack(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
    VPackFunctionParameters const& parameters) {
  auto value = ExtractFunctionParameter(trx, parameters, 0);

  if (!value.isObject()) {
    RegisterInvalidArgumentWarning(query, "KEEP");
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  std::unordered_set<std::string> names;
  ExtractKeys(names, query, trx, parameters, 1, "KEEP");

  try {
    std::shared_ptr<VPackBuilder> builder = query->getSharedBuilder();
    UnsetOrKeep(value, names, false, false, *builder);
    return AqlValue$(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function MERGE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Merge(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
                          FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(MergeVPack(query, trx, tmp));
}

AqlValue$ Functions::MergeVPack(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
                          VPackFunctionParameters const& parameters) {
  return MergeParameters(query, trx, parameters, "MERGE", false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function MERGE_RECURSIVE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::MergeRecursive(arangodb::aql::Query* query,
                                   arangodb::AqlTransaction* trx,
                                   FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(MergeRecursiveVPack(query, trx, tmp));
}

AqlValue$ Functions::MergeRecursiveVPack(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
    VPackFunctionParameters const& parameters) {
  return MergeParameters(query, trx, parameters, "MERGE_RECURSIVE", true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function HAS
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Has(arangodb::aql::Query* query,
                        arangodb::AqlTransaction* trx,
                        FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(HasVPack(query, trx, tmp));
}

AqlValue$ Functions::HasVPack(arangodb::aql::Query* query,
                              arangodb::AqlTransaction* trx,
                              VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n < 2) {
    // no parameters
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(false));
    return AqlValue$(b.get());
  }

  auto value = ExtractFunctionParameter(trx, parameters, 0);

  if (!value.isObject()) {
    // not an object
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(false));
    return AqlValue$(b.get());
  }

  auto name = ExtractFunctionParameter(trx, parameters, 1);
  std::string p;
  if (!name.isString()) {
    arangodb::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
    arangodb::basics::VPackStringBufferAdapter adapter(buffer.stringBuffer());
    AppendAsString(adapter, name);
    p = std::string(buffer.c_str(), buffer.length());
  } else {
    p = name.copyString();
  }
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(value.hasKey(p)));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function ATTRIBUTES
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Attributes(arangodb::aql::Query* query,
                               arangodb::AqlTransaction* trx,
                               FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(AttributesVPack(query, trx, tmp));
}

AqlValue$ Functions::AttributesVPack(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
    VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 1) {
    // no parameters
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  auto value = ExtractFunctionParameter(trx, parameters, 0);
  if (!value.isObject()) {
    // not an object
    RegisterWarning(query, "ATTRIBUTES",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  bool const removeInternal = GetBooleanParameter(trx, parameters, 1, false);
  bool const doSort = GetBooleanParameter(trx, parameters, 2, false);

  TRI_ASSERT(value.isObject());
  if (value.length() == 0) {
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    {
      VPackObjectBuilder guard(b.get());
    }
    return AqlValue$(b.get());
  }

  if (doSort) {
    std::set<std::string, arangodb::basics::VelocyPackHelper::AttributeSorter>
        keys;
    VPackCollection::keys(value, keys);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    {
      VPackArrayBuilder guard(b.get());
      for (auto const& it : keys) {
        TRI_ASSERT(!it.empty());
        if (removeInternal && it.at(0) == '_') {
            continue;
        }
        b->add(VPackValue(it));
      }
    }
    return AqlValue$(b.get());
  } else {
    std::set<std::string> keys;
    VPackCollection::keys(value, keys);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    {
      VPackArrayBuilder guard(b.get());
      for (auto const& it : keys) {
        TRI_ASSERT(!it.empty());
        if (removeInternal && it.at(0) == '_') {
            continue;
        }
        b->add(VPackValue(it));
      }
    }
    return AqlValue$(b.get());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function VALUES
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Values(arangodb::aql::Query* query,
                           arangodb::AqlTransaction* trx,
                           FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(ValuesVPack(query, trx, tmp));
}

AqlValue$ Functions::ValuesVPack(arangodb::aql::Query* query,
                                 arangodb::AqlTransaction* trx,
                                 VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 1) {
    // no parameters
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  auto value = ExtractFunctionParameter(trx, parameters, 0);
  if (!value.isObject()) {
    // not an object
    RegisterWarning(query, "VALUES",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  bool const removeInternal = GetBooleanParameter(trx, parameters, 1, false);

  TRI_ASSERT(value.isObject());
  if (value.length() == 0) {
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    {
      VPackObjectBuilder guard(b.get());
    }
    return AqlValue$(b.get());
  }

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  {
    VPackArrayBuilder guard(b.get());
    for (auto const& entry : VPackObjectIterator(value)) {
      if (!entry.key.isString()) {
        // somehow invalid
        continue;
      }
      if (removeInternal && entry.key.copyString().at(0) == '_') {
        // skip attribute
        continue;
      }
      b->add(entry.value);
    }
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function MIN
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Min(arangodb::aql::Query* query,
                        arangodb::AqlTransaction* trx,
                        FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(MinVPack(query, trx, tmp));
}

AqlValue$ Functions::MinVPack(arangodb::aql::Query* query,
                              arangodb::AqlTransaction* trx,
                              VPackFunctionParameters const& parameters) {
  auto value = ExtractFunctionParameter(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "MIN", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  VPackSlice minValue;
  for (auto const& it : VPackArrayIterator(value)) {
    if (it.isNull()) {
      continue;
    }
    if (minValue.isNone() || arangodb::basics::VelocyPackHelper::compare(it, minValue, true) < 0) {
      minValue = it;
    }
  }
  if (minValue.isNone()) {
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }
  return AqlValue$(minValue);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function MAX
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Max(arangodb::aql::Query* query,
                        arangodb::AqlTransaction* trx,
                        FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(MaxVPack(query, trx, tmp));
}

AqlValue$ Functions::MaxVPack(arangodb::aql::Query* query,
                              arangodb::AqlTransaction* trx,
                              VPackFunctionParameters const& parameters) {
  auto value = ExtractFunctionParameter(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "MAX", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }
  VPackSlice maxValue;
  for (auto const& it : VPackArrayIterator(value)) {
    if (maxValue.isNone() || arangodb::basics::VelocyPackHelper::compare(it, maxValue, true) > 0) {
      maxValue = it;
    }
  }
  if (maxValue.isNone()) {
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }
  return AqlValue$(maxValue);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function SUM
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Sum(arangodb::aql::Query* query,
                        arangodb::AqlTransaction* trx,
                        FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(SumVPack(query, trx, tmp));
}

AqlValue$ Functions::SumVPack(arangodb::aql::Query* query,
                              arangodb::AqlTransaction* trx,
                              VPackFunctionParameters const& parameters) {
  auto value = ExtractFunctionParameter(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "SUM", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  double sum = 0.0;
  for (auto const& it : VPackArrayIterator(value)) {
    if (it.isNull()) {
      continue;
    }
    if (!it.isNumber()) {
      std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
      b->add(VPackValue(VPackValueType::Null));
      return AqlValue$(b.get());
    }
    double const number = it.getNumericValue<double>();

    if (!std::isnan(number) && number != HUGE_VAL && number != -HUGE_VAL) {
      sum += number;
    }
  }
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  if (!std::isnan(sum) && sum != HUGE_VAL && sum != -HUGE_VAL) {
    b->add(VPackValue(sum));
  } else {
    b->add(VPackValue(VPackValueType::Null));
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function AVERAGE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Average(arangodb::aql::Query* query,
                            arangodb::AqlTransaction* trx,
                            FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(AverageVPack(query, trx, tmp));
}

AqlValue$ Functions::AverageVPack(arangodb::aql::Query* query,
                                  arangodb::AqlTransaction* trx,
                                  VPackFunctionParameters const& parameters) {
  auto value = ExtractFunctionParameter(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "AVERAGE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  double sum = 0.0;
  size_t count = 0;
  for (auto const& v : VPackArrayIterator(value)) {
    if (v.isNull()) {
      continue;
    }
    if (!v.isNumber()) {
      RegisterWarning(query, "AVERAGE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
      std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
      b->add(VPackValue(VPackValueType::Null));
      return AqlValue$(b.get());
    }

    // got a numeric value
    double const number = v.getNumericValue<double>();

    if (!std::isnan(number) && number != HUGE_VAL && number != -HUGE_VAL) {
      sum += number;
      ++count;
    }
  }

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  if (count > 0 && !std::isnan(sum) && sum != HUGE_VAL && sum != -HUGE_VAL) {
    b->add(VPackValue(sum / static_cast<size_t>(count)));
  } else {
    b->add(VPackValue(VPackValueType::Null));
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function MD5
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Md5(arangodb::aql::Query*,
                        arangodb::AqlTransaction* trx,
                        FunctionParameters const& parameters) {
  auto value = ExtractFunctionParameter(trx, parameters, 0, false);

  arangodb::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
  AppendAsString(buffer, value.json());

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

  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, hex, 32));
}

AqlValue$ Functions::Md5VPack(arangodb::aql::Query* query,
                              arangodb::AqlTransaction* trx,
                              VPackFunctionParameters const& parameters) {
  auto value = ExtractFunctionParameter(trx, parameters, 0);
  arangodb::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer.stringBuffer());

  AppendAsString(adapter, value);

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

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(std::string(hex, 32)));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function SHA1
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Sha1(arangodb::aql::Query* query,
                         arangodb::AqlTransaction* trx,
                         FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(Sha1VPack(query, trx, tmp));
}

AqlValue$ Functions::Sha1VPack(arangodb::aql::Query* query,
                               arangodb::AqlTransaction* trx,
                               VPackFunctionParameters const& parameters) {
  auto value = ExtractFunctionParameter(trx, parameters, 0);

  arangodb::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer.stringBuffer());

  AppendAsString(adapter, value);

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

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(std::string(hex, 40)));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function UNIQUE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Unique(arangodb::aql::Query* query,
                           arangodb::AqlTransaction* trx,
                           FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(UniqueVPack(query, trx, tmp));
}

AqlValue$ Functions::UniqueVPack(arangodb::aql::Query* query,
                                 arangodb::AqlTransaction* trx,
                                 VPackFunctionParameters const& parameters) {
  if (parameters.size() != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "UNIQUE", (int)1,
        (int)1);
  }

  auto const value = ExtractFunctionParameter(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    RegisterWarning(query, "UNIQUE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  std::unordered_set<VPackSlice,
                     arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      values(512, arangodb::basics::VelocyPackHelper::VPackHash(),
             arangodb::basics::VelocyPackHelper::VPackEqual());

  for (auto const& s : VPackArrayIterator(value)) {
    if (!s.isNone()) {
      values.emplace(s);
    }
  }

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  try {
    VPackArrayBuilder guard(b.get());
    for (auto const& it : values) {
      b->add(it);
    }
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function SORTED_UNIQUE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::SortedUnique(arangodb::aql::Query* query,
                                 arangodb::AqlTransaction* trx,
                                 FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(SortedUniqueVPack(query, trx, tmp));
}

AqlValue$ Functions::SortedUniqueVPack(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
    VPackFunctionParameters const& parameters) {
  if (parameters.size() != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "SORTED_UNIQUE",
        (int)1, (int)1);
  }

  auto const value = ExtractFunctionParameter(trx, parameters, 0);

  if (!value.isArray()) {
    // not an array
    // this is an internal function - do NOT issue a warning here
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  std::set<VPackSlice, arangodb::basics::VelocyPackHelper::VPackLess<true>> values;
  for (auto const& it : VPackArrayIterator(value)) {
    if (!it.isNone()) {
      values.insert(it);
    }
  }

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  try {
    VPackArrayBuilder guard(b.get());
    for (auto const& it : values) {
      b->add(it);
    }
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function UNION
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Union(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
                          FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(UnionVPack(query, trx, tmp));
}

AqlValue$ Functions::UnionVPack(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
                          VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "UNION", (int)2,
        (int)Function::MaxArguments);
  }

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  try {
    VPackArrayBuilder guard(b.get());
    for (size_t i = 0; i < n; ++i) {
      auto value = ExtractFunctionParameter(trx, parameters, i);

      if (!value.isArray()) {
        // not an array
        RegisterInvalidArgumentWarning(query, "UNION");
        b->clear();
        b->add(VPackValue(VPackValueType::Null));
        return AqlValue$(b.get());
      }

      TRI_IF_FAILURE("AqlFunctions::OutOfMemory1") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      // this passes ownership for the JSON contens into result
      for (auto const& it : VPackArrayIterator(value)) {
        b->add(it);
        TRI_IF_FAILURE("AqlFunctions::OutOfMemory2") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
      }
    }
  } catch (arangodb::basics::Exception const& e) {
    // Rethrow arangodb Errors
    throw;
  } catch (std::exception const& e) {
    // All other exceptions are OUT_OF_MEMORY
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  TRI_IF_FAILURE("AqlFunctions::OutOfMemory3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function UNION_DISTINCT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::UnionDistinct(arangodb::aql::Query* query,
                                  arangodb::AqlTransaction* trx,
                                  FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(UnionDistinctVPack(query, trx, tmp));
}

AqlValue$ Functions::UnionDistinctVPack(arangodb::aql::Query* query,
                                        arangodb::AqlTransaction* trx,
                                        VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "UNION_DISTINCT",
        (int)2, (int)Function::MaxArguments);
  }

  std::unordered_set<VPackSlice,
                     arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      values(512, arangodb::basics::VelocyPackHelper::VPackHash(),
             arangodb::basics::VelocyPackHelper::VPackEqual());


  for (size_t i = 0; i < n; ++i) {
    auto value = ExtractFunctionParameter(trx, parameters, i);

    if (!value.isArray()) {
      // not an array
      RegisterInvalidArgumentWarning(query, "UNION_DISTINCT");
      std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
      b->add(VPackValue(VPackValueType::Null));
      return AqlValue$(b.get());
    }

    for (auto const& v : VPackArrayIterator(value)) {
      if (values.find(v) == values.end()) {
        TRI_IF_FAILURE("AqlFunctions::OutOfMemory1") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        values.emplace(v);
      }
    }
  }

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();

  TRI_IF_FAILURE("AqlFunctions::OutOfMemory2") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  try {
    VPackArrayBuilder guard(b.get());
    for (auto const& it : values) {
      b->add(it);
    }
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  TRI_IF_FAILURE("AqlFunctions::OutOfMemory3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function INTERSECTION
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Intersection(arangodb::aql::Query* query,
                                 arangodb::AqlTransaction* trx,
                                 FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(IntersectionVPack(query, trx, tmp));
}

AqlValue$ Functions::IntersectionVPack(arangodb::aql::Query* query,
                                       arangodb::AqlTransaction* trx,
                                       VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "INTERSECTION",
        (int)2, (int)Function::MaxArguments);
  }

  std::unordered_map<VPackSlice, size_t,
                     arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      values(512, arangodb::basics::VelocyPackHelper::VPackHash(),
             arangodb::basics::VelocyPackHelper::VPackEqual());

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();

  for (size_t i = 0; i < n; ++i) {
    auto value = ExtractFunctionParameter(trx, parameters, i);

    if (!value.isArray()) {
      // not an array
      RegisterWarning(query, "INTERSECTION", TRI_ERROR_QUERY_ARRAY_EXPECTED);
      b->clear();
      b->add(VPackValue(VPackValueType::Null));
      return AqlValue$(b.get());
    }
    
    for (auto const& it : VPackArrayIterator(value)) {
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

  {
    VPackArrayBuilder guard(b.get());
    for (auto const& it : values) {
      if (it.second == n) {
        b->add(it.first);
      }
    }
  }
  values.clear();

  TRI_IF_FAILURE("AqlFunctions::OutOfMemory3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function NEIGHBORS
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Neighbors(arangodb::aql::Query* query,
                              arangodb::AqlTransaction* trx,
                              FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(NeighborsVPack(query, trx, tmp));
}

AqlValue$ Functions::NeighborsVPack(arangodb::aql::Query* query,
                                    arangodb::AqlTransaction* trx,
                                    VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  arangodb::traverser::NeighborsOptions opts;

  if (n < 4 || n > 6) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "NEIGHBORS", (int)4,
        (int)6);
  }

  auto resolver = trx->resolver();

  VPackSlice vertexCol = ExtractFunctionParameter(trx, parameters, 0);

  if (!vertexCol.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
  }
  std::string vColName = vertexCol.copyString();

  VPackSlice edgeCol = ExtractFunctionParameter(trx, parameters, 1);
  if (!edgeCol.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
  }
  std::string eColName = edgeCol.copyString();

  VPackSlice vertexInfo = ExtractFunctionParameter(trx, parameters, 2);
  std::string vertexId;
  bool splitCollection = false;
  if (vertexInfo.isString()) {
    vertexId = vertexInfo.copyString();
    if (vertexId.find("/") != std::string::npos) {
      splitCollection = true;
    }
  } else if (vertexInfo.isObject()) {
    if (!vertexInfo.hasKey("_id")) {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
    }
    VPackSlice idSlice = vertexInfo.get("_id");
    if (!idSlice.isString()) {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
    }
    vertexId = idSlice.copyString();
    splitCollection = true;
  } else {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
  }

  if (splitCollection) {
    size_t split;
    char const* str = vertexId.c_str();

    if (!TRI_ValidateDocumentIdKeyGenerator(str, &split)) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
    }

    std::string const collectionName = vertexId.substr(0, split);
    if (collectionName.compare(vColName) != 0) {
      THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_GRAPH_INVALID_PARAMETER,
                                    "specified vertex collection '%s' does "
                                    "not match start vertex collection '%s'",
                                    vColName.c_str(), collectionName.c_str());
    }
    auto coli = resolver->getCollectionStruct(collectionName);

    if (coli == nullptr) {
      THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                    "'%s'", collectionName.c_str());
    }

    VertexId v(coli->_cid, const_cast<char*>(str + split + 1));
    opts.start = v;
  } else {
    VertexId v(resolver->getCollectionIdLocal(vColName), vertexId.c_str());
    opts.start = v;
  }

  VPackSlice direction = ExtractFunctionParameter(trx, parameters, 3);
  if (!direction.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
  }
  {
    std::string const dir = direction.copyString();
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

  if (n > 5) {
    auto options = ExtractFunctionParameter(trx, parameters, 5);
    if (!options.isObject()) {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
    }
    includeData = arangodb::basics::VelocyPackHelper::getBooleanValue(
        options, "includeData", false);
    opts.minDepth =
        arangodb::basics::VelocyPackHelper::getNumericValue<uint64_t>(
            options, "minDepth", 1);
    if (opts.minDepth == 0) {
      opts.maxDepth =
          arangodb::basics::VelocyPackHelper::getNumericValue<uint64_t>(
              options, "maxDepth", 1);
    } else {
      opts.maxDepth =
          arangodb::basics::VelocyPackHelper::getNumericValue<uint64_t>(
              options, "maxDepth", opts.minDepth);
    }
  }

  TRI_voc_cid_t eCid = resolver->getCollectionIdLocal(eColName);

  {
    // ensure the collection is loaded
    auto collection = trx->trxCollection(eCid);

    if (collection == nullptr) {
      int res = TRI_AddCollectionTransaction(trx->getInternals(), eCid,
                                             TRI_TRANSACTION_READ,
                                             trx->nestingLevel(), true, true);
      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }

      TRI_EnsureCollectionsTransaction(trx->getInternals());
      collection = trx->trxCollection(eCid);

      if (collection == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "collection is a nullptr");
      }
    }
  }

  // Function to return constant distance
  auto wc = [](TRI_doc_mptr_copy_t&) -> double { return 1; };

  auto eci = std::make_unique<EdgeCollectionInfo>(
      trx, eCid, trx->documentCollection(eCid), wc);
  TRI_IF_FAILURE("EdgeCollectionInfoOOM1") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (n > 4) {
    auto edgeExamples = ExtractFunctionParameter(trx, parameters, 4);
    if (!(edgeExamples.isArray() && edgeExamples.length() == 0)) {
      opts.addEdgeFilter(edgeExamples, eci->getShaper(), eCid, resolver);
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

  std::unordered_set<VertexId> neighbors;
  TRI_RunNeighborsSearch(edgeCollectionInfos, opts, neighbors);

  return VertexIdsToAqlValueVPack(query, trx, resolver, neighbors, includeData);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function NEAR
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Near(arangodb::aql::Query* query,
                         arangodb::AqlTransaction* trx,
                         FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(NearVPack(query, trx, tmp));
}

AqlValue$ Functions::NearVPack(arangodb::aql::Query* query,
                               arangodb::AqlTransaction* trx,
                               VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 3 || n > 5) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "NEAR", (int)3,
        (int)5);
  }

  auto resolver = trx->resolver();

  VPackSlice collectionSlice = ExtractFunctionParameter(trx, parameters, 0);
  if (!collectionSlice.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEAR");
  }

  std::string colName = collectionSlice.copyString();

  VPackSlice latitude = ExtractFunctionParameter(trx, parameters, 1);
  VPackSlice longitude = ExtractFunctionParameter(trx, parameters, 2);

  if (!latitude.isNumber() || !longitude.isNumber()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEAR");
  }

  // extract limit
  int64_t limitValue = 100;

  if (n > 3) {
    VPackSlice limit = ExtractFunctionParameter(trx, parameters, 3);

    if (limit.isNumber()) {
      limitValue = limit.getNumericValue<int64_t>();
    } else if (!limit.isNull()) {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEAR");
    }
  }

  std::string attributeName;
  if (n > 4) {
    // have a distance attribute
    VPackSlice distanceAttribute = ExtractFunctionParameter(trx, parameters, 4);

    if (!distanceAttribute.isNull() && !distanceAttribute.isString()) {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEAR");
    }

    if (distanceAttribute.isString()) {
      attributeName = distanceAttribute.copyString();
    }
  }

  TRI_voc_cid_t cid = resolver->getCollectionIdLocal(colName);
  VocShaper* shaper = nullptr;
  arangodb::Index* index = getGeoIndex(trx, cid, colName, shaper);

  TRI_ASSERT(index != nullptr);
  TRI_ASSERT(shaper != nullptr);

  GeoCoordinates* cors = static_cast<arangodb::GeoIndex2*>(index)->nearQuery(
      trx, latitude.getNumericValue<double>(),
      longitude.getNumericValue<double>(), limitValue);

  return buildGeoResult(query, cors, shaper, resolver, cid, attributeName);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function WITHIN
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Within(arangodb::aql::Query* query,
                           arangodb::AqlTransaction* trx,
                           FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(WithinVPack(query, trx, tmp));
}

AqlValue$ Functions::WithinVPack(arangodb::aql::Query* query,
                                 arangodb::AqlTransaction* trx,
                                 VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 4 || n > 5) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "WITHIN", (int)4,
        (int)5);
  }

  auto resolver = trx->resolver();

  VPackSlice collectionSlice = ExtractFunctionParameter(trx, parameters, 0);

  if (!collectionSlice.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "WITHIN");
  }

  std::string colName = collectionSlice.copyString();

  VPackSlice latitude = ExtractFunctionParameter(trx, parameters, 1);
  VPackSlice longitude = ExtractFunctionParameter(trx, parameters, 2);
  VPackSlice radius = ExtractFunctionParameter(trx, parameters, 3);

  if (!latitude.isNumber() || !longitude.isNumber() || !radius.isNumber()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "WITHIN");
  }

  std::string attributeName;
  if (n > 4) {
    // have a distance attribute
    VPackSlice distanceAttribute = ExtractFunctionParameter(trx, parameters, 4);

    if (!distanceAttribute.isNull() && !distanceAttribute.isString()) {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "WITHIN");
    }

    if (distanceAttribute.isString()) {
      attributeName = distanceAttribute.copyString();
    }
  }

  TRI_voc_cid_t cid = resolver->getCollectionIdLocal(colName);
  VocShaper* shaper = nullptr;
  arangodb::Index* index = getGeoIndex(trx, cid, colName, shaper);

  TRI_ASSERT(index != nullptr);
  TRI_ASSERT(shaper != nullptr);

  GeoCoordinates* cors = static_cast<arangodb::GeoIndex2*>(index)->withinQuery(
      trx, latitude.getNumericValue<double>(),
      longitude.getNumericValue<double>(), radius.getNumericValue<double>());

  return buildGeoResult(query, cors, shaper, resolver, cid, attributeName);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function FLATTEN
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Flatten(arangodb::aql::Query* query,
                            arangodb::AqlTransaction* trx,
                            FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(FlattenVPack(query, trx, tmp));
}

AqlValue$ Functions::FlattenVPack(arangodb::aql::Query* query,
                                  arangodb::AqlTransaction* trx,
                                  VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n == 0 || n > 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "FLATTEN", (int)1,
        (int)2);
  }

  VPackSlice listSlice = ExtractFunctionParameter(trx, parameters, 0);
  if (!listSlice.isArray()) {
    RegisterWarning(query, "FLATTEN", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  size_t maxDepth = 1;
  if (n == 2) {
    VPackSlice maxDepthSlice = ExtractFunctionParameter(trx, parameters, 1);
    bool isValid = true;
    double tmpMaxDepth = ValueToNumber(maxDepthSlice, isValid);
    if (!isValid || tmpMaxDepth < 1) {
      maxDepth = 1;
    } else {
      maxDepth = static_cast<size_t>(tmpMaxDepth);
    }
  }

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  try {
    VPackArrayBuilder guard(b.get());
    FlattenList(listSlice, maxDepth, 0, *b);
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function ZIP
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Zip(arangodb::aql::Query* query,
                        arangodb::AqlTransaction* trx,
                        FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(ZipVPack(query, trx, tmp));
}

AqlValue$ Functions::ZipVPack(arangodb::aql::Query* query,
                              arangodb::AqlTransaction* trx,
                              VPackFunctionParameters const& parameters) {
  if (parameters.size() != 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "ZIP", (int)2,
        (int)2);
  }

  VPackSlice keysSlice = ExtractFunctionParameter(trx, parameters, 0);
  VPackSlice valuesSlice = ExtractFunctionParameter(trx, parameters, 1);

  if (!keysSlice.isArray() || !valuesSlice.isArray() ||
      keysSlice.length() != valuesSlice.length()) {
    RegisterWarning(query, "ZIP",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  VPackValueLength n = keysSlice.length();

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  try {
    VPackObjectBuilder guard(b.get());

    // Buffer will temporarily hold the keys
    arangodb::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE, 24);
    arangodb::basics::VPackStringBufferAdapter adapter(buffer.stringBuffer());
    for (VPackValueLength i = 0; i < n; ++i) {
      buffer.reset();
      AppendAsString(adapter, keysSlice.at(i));
      b->add(std::string(buffer.c_str(), buffer.length()), valuesSlice.at(i));
    }
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function PARSE_IDENTIFIER
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::ParseIdentifier(arangodb::aql::Query* query,
                                    arangodb::AqlTransaction* trx,
                                    FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(ParseIdentifierVPack(query, trx, tmp));
}

AqlValue$ Functions::ParseIdentifierVPack(arangodb::aql::Query* query,
                                          arangodb::AqlTransaction* trx,
                                          VPackFunctionParameters const& parameters) {
  if (parameters.size() != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "PARSE_IDENTIFIER",
        (int)1, (int)1);
  }

  VPackSlice value = ExtractFunctionParameter(trx, parameters, 0);
  std::string identifier;
  if (value.isObject() && value.hasKey(TRI_VOC_ATTRIBUTE_ID)) {
    identifier = arangodb::basics::VelocyPackHelper::getStringValue(
        value, TRI_VOC_ATTRIBUTE_ID, "");
  } else if (value.isString()) {
    identifier = value.copyString();
  }

  if (!identifier.empty()) {
    std::vector<std::string> parts =
        arangodb::basics::StringUtils::split(identifier, "/");
    if (parts.size() == 2) {
      std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
      try {
        VPackObjectBuilder guard(b.get());
        b->add("collection", VPackValue(parts[0]));
        b->add("key", VPackValue(parts[1]));
      } catch (...) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
      return AqlValue$(b.get());
    }
  }

  RegisterWarning(query, "PARSE_IDENTIFIER",
                  TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(VPackValueType::Null));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function Minus
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Minus(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
                          FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(MinusVPack(query, trx, tmp));
}

AqlValue$ Functions::MinusVPack(arangodb::aql::Query* query,
                                arangodb::AqlTransaction* trx,
                                VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 2) {
    // The max number is arbitrary
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "MINUS", (int)2,
        (int)99999);
  }
  VPackSlice baseArray = ExtractFunctionParameter(trx, parameters, 0);

  if (!baseArray.isArray()) {
    RegisterWarning(query, "MINUS",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  std::unordered_map<std::string, size_t> contains;
  contains.reserve(n);

  // Fill the original map
  for (size_t i = 0; i < baseArray.length(); ++i) {
    contains.emplace(baseArray.at(i).toJson(), i);
  }

  // Iterate through all following parameters and delete found elements from the
  // map
  for (size_t k = 1; k < n; ++k) {
    VPackSlice nextArray = ExtractFunctionParameter(trx, parameters, k);
    if (!nextArray.isArray()) {
      RegisterWarning(query, "MINUS",
                      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
      b->add(VPackValue(VPackValueType::Null));
      return AqlValue$(b.get());
    }

    for (auto const& searchSlice : VPackArrayIterator(nextArray)) {
      std::string search = searchSlice.toJson();
      auto find = contains.find(search);
      if (find != contains.end()) {
        contains.erase(find);
      }
    }
  }

  // We ommit the normalize part from js, cannot occur here
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  try {
    VPackArrayBuilder guard(b.get());
    for (auto const& it : contains) {
      b->add(baseArray.at(it.second));
    }
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  return AqlValue$(b.get());
}

static void RegisterCollectionInTransaction(
    arangodb::AqlTransaction* trx, std::string const& collectionName,
    TRI_voc_cid_t& cid, TRI_transaction_collection_t*& collection) {
  TRI_ASSERT(collection == nullptr);
  cid = trx->resolver()->getCollectionIdLocal(collectionName);
  if (cid == 0) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, "'%s'",
                                  collectionName.c_str());
  }
  // ensure the collection is loaded
  collection = trx->trxCollection(cid);

  if (collection == nullptr) {
    int res = TRI_AddCollectionTransaction(trx->getInternals(), cid,
                                           TRI_TRANSACTION_READ,
                                           trx->nestingLevel(), true, true);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_FORMAT(res, "'%s'", collectionName.c_str());
    }
    TRI_EnsureCollectionsTransaction(trx->getInternals());
    collection = trx->trxCollection(cid);

    if (collection == nullptr) {
      // This case should never occur
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "could not load collection");
    }
  }
  TRI_ASSERT(collection != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Helper function to get a document by it's identifier
///        The collection has to be locked by the transaction before
////////////////////////////////////////////////////////////////////////////////

static void getDocumentByIdentifier(arangodb::AqlTransaction* trx,
                                    CollectionNameResolver const* resolver,
                                    TRI_transaction_collection_t* collection,
                                    TRI_voc_cid_t const& cid,
                                    std::string const& collectionName,
                                    std::string const& identifier,
                                    VPackBuilder& result) {
  std::vector<std::string> parts =
      arangodb::basics::StringUtils::split(identifier, "/");

  TRI_doc_mptr_copy_t mptr;
  if (parts.size() == 1) {
    int res = trx->document(collection, &mptr, parts[0]);
    if (res != TRI_ERROR_NO_ERROR) {
      return;
    }
  } else if (parts.size() == 2) {
    if (parts[0] != collectionName) {
      // Reqesting an _id that cannot be stored in this collection
      return;
    }
    int res = trx->document(collection, &mptr, parts[1]);
    if (res != TRI_ERROR_NO_ERROR) {
      return;
    }
  } else {
    return;
  }

  std::unordered_set<std::string> unused;
  ExpandShapedJson(collection->_collection->_collection->getShaper(), resolver,
                   cid, &mptr, result, false, unused);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Helper function to get a document by its _id
///        This function will lazy read-lock the collection.
/// this function will not throw if the document or the collection cannot be
/// found
////////////////////////////////////////////////////////////////////////////////

static void getDocumentByIdentifier(arangodb::AqlTransaction* trx,
                                    CollectionNameResolver const* resolver,
                                    std::string const& identifier,
                                    VPackBuilder& result) {
  std::vector<std::string> parts =
      arangodb::basics::StringUtils::split(identifier, "/");

  if (parts.size() != 2) {
    return;
  }
  std::string collectionName = parts[0];
  TRI_transaction_collection_t* collection = nullptr;
  TRI_voc_cid_t cid = 0;
  try {
    RegisterCollectionInTransaction(trx, collectionName, cid, collection);
  } catch (arangodb::basics::Exception const& ex) {
    // don't throw if collection is not found
    if (ex.code() == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
      return;
    }
    throw;
  }

  TRI_doc_mptr_copy_t mptr;
  int res = trx->document(collection, &mptr, parts[1]);

  if (res != TRI_ERROR_NO_ERROR) {
    return;
  }

  std::unordered_set<std::string> unused;
  ExpandShapedJson(collection->_collection->_collection->getShaper(), resolver,
                   cid, &mptr, result, false, unused);
}




////////////////////////////////////////////////////////////////////////////////
/// @brief function Document
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Document(arangodb::aql::Query* query,
                             arangodb::AqlTransaction* trx,
                             FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(DocumentVPack(query, trx, tmp));
}

AqlValue$ Functions::DocumentVPack(arangodb::aql::Query* query,
                                   arangodb::AqlTransaction* trx,
                                   VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 1 || 2 < n) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "DOCUMENT", (int)1,
        (int)2);
  }

  auto resolver = trx->resolver();
  if (n == 1) {
    VPackSlice id = ExtractFunctionParameter(trx, parameters, 0);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    if (id.isString()) {
      std::string identifier = id.copyString();
      getDocumentByIdentifier(trx, resolver, identifier, *b);
      if (b->isEmpty()) {
        // not found
        b->add(VPackValue(VPackValueType::Null));
      }
    } else if (id.isArray()) {
      VPackArrayBuilder guard(b.get());
      for (auto const& next : VPackArrayIterator(id)) {
        try {
          if (next.isString()) {
            std::string identifier = next.copyString();
            getDocumentByIdentifier(trx, resolver, identifier, *b);
          }
        } catch (arangodb::basics::Exception const&) {
          // Ignore all ArangoDB exceptions here
        }
      }
    } else {
      b->add(VPackValue(VPackValueType::Null));
    }
    return AqlValue$(b.get());
  }

  VPackSlice collectionSlice = ExtractFunctionParameter(trx, parameters, 0);
  if (!collectionSlice.isString()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
  std::string collectionName = collectionSlice.copyString();

  TRI_transaction_collection_t* collection = nullptr;
  TRI_voc_cid_t cid;
  bool notFound = false;

  try {
    RegisterCollectionInTransaction(trx, collectionName, cid, collection);
  } catch (arangodb::basics::Exception const& ex) {
    // don't throw if collection is not found
    if (ex.code() != TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
      throw;
    }
    notFound = true;
  }

  VPackSlice id = ExtractFunctionParameter(trx, parameters, 1);
  if (id.isString()) {
    if (notFound) {
      std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
      b->add(VPackValue(VPackValueType::Null));
      return AqlValue$(b.get());
    }
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    std::string identifier = id.copyString();
    getDocumentByIdentifier(trx, resolver, collection, cid, collectionName, identifier, *b);
    if (b->isEmpty()) {
      b->add(VPackValue(VPackValueType::Null));
    }
    return AqlValue$(b.get());
  } else if (id.isArray()) {
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    {
      VPackArrayBuilder guard(b.get());
      if (!notFound) {
        for (auto const& next : VPackArrayIterator(id)) {
          try {
            if (next.isString()) {
              std::string identifier = next.copyString();
              getDocumentByIdentifier(trx, resolver, collection, cid, collectionName, identifier, *b);
            }
          } catch (arangodb::basics::Exception const&) {
            // Ignore all ArangoDB exceptions here
          }
        }
      }
    }
    return AqlValue$(b.get());
  }
  // Id has invalid format
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(VPackValueType::Null));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function Edges
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Edges(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
                          FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(EdgesVPack(query, trx, tmp));
}

AqlValue$ Functions::EdgesVPack(arangodb::aql::Query* query,
                                arangodb::AqlTransaction* trx,
                                VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 3 || 5 < n) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "EDGES", (int)3,
        (int)5);
  }

  VPackSlice collectionSlice = ExtractFunctionParameter(trx, parameters, 0);
  if (!collectionSlice.isString()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
  std::string collectionName = collectionSlice.copyString();

  TRI_transaction_collection_t* collection = nullptr;
  TRI_voc_cid_t cid;
  RegisterCollectionInTransaction(trx, collectionName, cid, collection);
  if (collection->_collection->_type != TRI_COL_TYPE_EDGE) {
    RegisterWarning(query, "EDGES", TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  VPackSlice vertexSlice = ExtractFunctionParameter(trx, parameters, 1);
  if (!vertexSlice.isArray() && !vertexSlice.isString() && !vertexSlice.isObject()) {
    // Invalid Start vertex
    // Early Abort before parsing other parameters
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  VPackSlice directionSlice = ExtractFunctionParameter(trx, parameters, 2);
  if (!directionSlice.isString()) {
    RegisterWarning(query, "EDGES",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }
  std::string dirString = directionSlice.copyString();
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
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  auto resolver = trx->resolver();

  auto shaper = collection->_collection->_collection->getShaper();
  std::unique_ptr<arangodb::ExampleMatcher> matcher;

  if (n > 3) {
    // We might have examples
    VPackSlice exampleSlice = ExtractFunctionParameter(trx, parameters, 3);
    if ((exampleSlice.isArray() && exampleSlice.length() != 0)|| exampleSlice.isObject()) {
      try {
        matcher.reset(
            new arangodb::ExampleMatcher(exampleSlice, shaper, resolver, false));
      } catch (arangodb::basics::Exception const& e) {
        if (e.code() != TRI_RESULT_ELEMENT_NOT_FOUND) {
          throw;
        }
        // We can never fulfill this filter!
        // RETURN empty Array
        std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
        {
          VPackArrayBuilder guard(b.get());
        }
        return AqlValue$(b.get());
      }
    }
  }

  bool includeVertices = false;

  if (n == 5) {
    // We have options
    VPackSlice options = ExtractFunctionParameter(trx, parameters, 4);
    if (options.isObject()) {
      includeVertices = arangodb::basics::VelocyPackHelper::getBooleanValue(
          options, "includeVertices", false);
    }
  }

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  {
    VPackArrayBuilder guard(b.get());
    if (vertexSlice.isArray()) {
      for (auto const& v : VPackArrayIterator(vertexSlice)) {
        try {
          RequestEdges(v, trx, resolver, shaper, cid,
                       collection->_collection->_collection, direction,
                       matcher.get(), includeVertices, *b);
        } catch (...) {
          // Errors in Array are simply ignored
        }
      }
    } else {
      RequestEdges(vertexSlice, trx, resolver, shaper, cid,
                   collection->_collection->_collection, direction,
                   matcher.get(), includeVertices, *b);
    }
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function ROUND
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Round(arangodb::aql::Query* query,
                        arangodb::AqlTransaction* trx,
                        FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(RoundVPack(query, trx, tmp));
}

AqlValue$ Functions::RoundVPack(arangodb::aql::Query* query,
                                arangodb::AqlTransaction* trx,
                                VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "ROUND", (int)1,
        (int)1);
  }

  VPackSlice inputSlice = ExtractFunctionParameter(trx, parameters, 0);

  bool unused = false;
  double input = arangodb::basics::VelocyPackHelper::toDouble(inputSlice, unused);

  input = floor(input + 0.5);  // Rounds down for < x.4999 and up for > x.50000
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(input));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function ABS
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Abs(arangodb::aql::Query* query,
                        arangodb::AqlTransaction* trx,
                        FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(AbsVPack(query, trx, tmp));
}

AqlValue$ Functions::AbsVPack(arangodb::aql::Query* query,
                              arangodb::AqlTransaction* trx,
                              VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "ABS", (int)1,
        (int)1);
  }

  VPackSlice inputSlice = ExtractFunctionParameter(trx, parameters, 0);

  bool unused = false;
  double input = arangodb::basics::VelocyPackHelper::toDouble(inputSlice, unused);

  input = std::abs(input);
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(input));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function CEIL
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Ceil(arangodb::aql::Query* query,
                         arangodb::AqlTransaction* trx,
                         FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(CeilVPack(query, trx, tmp));
}

AqlValue$ Functions::CeilVPack(arangodb::aql::Query* query,
                         arangodb::AqlTransaction* trx,
                         VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "CEIL", (int)1,
        (int)1);
  }

  VPackSlice inputSlice = ExtractFunctionParameter(trx, parameters, 0);

  bool unused = false;
  double input = arangodb::basics::VelocyPackHelper::toDouble(inputSlice, unused);

  input = ceil(input);
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(input));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function FLOOR
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Floor(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
                          FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(FloorVPack(query, trx, tmp));
}

AqlValue$ Functions::FloorVPack(arangodb::aql::Query* query,
                                arangodb::AqlTransaction* trx,
                                VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "FLOOR", (int)1,
        (int)1);
  }

  VPackSlice inputSlice = ExtractFunctionParameter(trx, parameters, 0);

  bool unused = false;
  double input = arangodb::basics::VelocyPackHelper::toDouble(inputSlice, unused);

  input = floor(input);
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(input));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function SQRT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Sqrt(arangodb::aql::Query* query,
                         arangodb::AqlTransaction* trx,
                         FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(SqrtVPack(query, trx, tmp));
}

AqlValue$ Functions::SqrtVPack(arangodb::aql::Query* query,
                               arangodb::AqlTransaction* trx,
                               VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "SQRT", (int)1,
        (int)1);
  }

  VPackSlice inputSlice = ExtractFunctionParameter(trx, parameters, 0);

  bool unused = false;
  double input = arangodb::basics::VelocyPackHelper::toDouble(inputSlice, unused);
  input = sqrt(input);
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  if (std::isnan(input)) {
    b->add(VPackValue(VPackValueType::Null));
  } else {
    b->add(VPackValue(input));
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function POW
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Pow(arangodb::aql::Query* query,
                        arangodb::AqlTransaction* trx,
                        FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(PowVPack(query, trx, tmp));
}

AqlValue$ Functions::PowVPack(arangodb::aql::Query* query,
                        arangodb::AqlTransaction* trx,
                        VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n != 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "POW", (int)2,
        (int)2);
  }

  VPackSlice baseSlice = ExtractFunctionParameter(trx, parameters, 0);
  VPackSlice expSlice = ExtractFunctionParameter(trx, parameters, 1);

  bool unused = false;
  double base = arangodb::basics::VelocyPackHelper::toDouble(baseSlice, unused);
  double exp = arangodb::basics::VelocyPackHelper::toDouble(expSlice, unused);
  base = pow(base, exp);
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  if (std::isnan(base) || !std::isfinite(base)) {
    b->add(VPackValue(VPackValueType::Null));
  } else {
    b->add(VPackValue(base));
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function RAND
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Rand(arangodb::aql::Query* query,
                         arangodb::AqlTransaction* trx,
                         FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(RandVPack(query, trx, tmp));
}

AqlValue$ Functions::RandVPack(arangodb::aql::Query* query,
                         arangodb::AqlTransaction*,
                         VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n != 0) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "RAND", (int)0,
        (int)0);
  }

  // This Random functionality is not too good yet...
  double output = static_cast<double>(std::rand()) / RAND_MAX;
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(output));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function FIRST_DOCUMENT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::FirstDocument(arangodb::aql::Query* query,
                                  arangodb::AqlTransaction* trx,
                                  FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(FirstDocumentVPack(query, trx, tmp));
}

AqlValue$ Functions::FirstDocumentVPack(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
    VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  for (size_t i = 0; i < n; ++i) {
    VPackSlice element = ExtractFunctionParameter(trx, parameters, i);
    if (element.isObject()) {
      return AqlValue$(element);
    }
  }
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(VPackValueType::Null));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function FIRST_LIST
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::FirstList(arangodb::aql::Query* query,
                              arangodb::AqlTransaction* trx,
                              FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(FirstListVPack(query, trx, tmp));
}

AqlValue$ Functions::FirstListVPack(arangodb::aql::Query* query,
                              arangodb::AqlTransaction* trx,
                              VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  for (size_t i = 0; i < n; ++i) {
    VPackSlice element = ExtractFunctionParameter(trx, parameters, i);
    if (element.isArray()) {
      return AqlValue$(element);
    }
  }
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(VPackValueType::Null));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function PUSH
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Push(arangodb::aql::Query* query,
                         arangodb::AqlTransaction* trx,
                         FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(PushVPack(query, trx, tmp));
}

AqlValue$ Functions::PushVPack(arangodb::aql::Query* query,
                               arangodb::AqlTransaction* trx,
                               VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 2 && n != 3) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "PUSH", (int)2,
        (int)3);
  }
  VPackSlice list = ExtractFunctionParameter(trx, parameters, 0);
  VPackSlice toPush = ExtractFunctionParameter(trx, parameters, 1);

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  if (list.isNull()) {
    VPackArrayBuilder guard(b.get());
    b->add(toPush);
  } else if (list.isArray()) {
    VPackArrayBuilder guard(b.get());
    for (auto const& it : VPackArrayIterator(list)) {
      b->add(it);
    }
    if (n == 3) {
      VPackSlice unique = ExtractFunctionParameter(trx, parameters, 2);
      if (!ValueToBoolean(unique) || !ListContainsElement(list, toPush)) {
        b->add(toPush);
      }
    } else {
      b->add(toPush);
    }
  } else {
    RegisterWarning(query, "PUSH",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    b->add(VPackValue(VPackValueType::Null));
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function POP
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Pop(arangodb::aql::Query* query,
                        arangodb::AqlTransaction* trx,
                        FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(PopVPack(query, trx, tmp));
}

AqlValue$ Functions::PopVPack(arangodb::aql::Query* query,
                              arangodb::AqlTransaction* trx,
                              VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "POP", (int)1,
        (int)1);
  }
  VPackSlice list = ExtractFunctionParameter(trx, parameters, 0);

  if (list.isNull()) {
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  if (list.isArray()) {
    try {
      VPackArrayBuilder guard(b.get());
      auto iterator = VPackArrayIterator(list);
      while (!iterator.isLast()) {
        b->add(iterator.value());
        iterator.next();
      }
    } catch (...) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  } else {
    RegisterWarning(query, "POP",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    b->add(VPackValue(VPackValueType::Null));
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function APPEND
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Append(arangodb::aql::Query* query,
                           arangodb::AqlTransaction* trx,
                           FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(AppendVPack(query, trx, tmp));
}

AqlValue$ Functions::AppendVPack(arangodb::aql::Query* query,
                                 arangodb::AqlTransaction* trx,
                                 VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 2 && n != 3) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "APPEND", (int)2,
        (int)3);
  }
  VPackSlice list = ExtractFunctionParameter(trx, parameters, 0);
  VPackSlice toAppend = ExtractFunctionParameter(trx, parameters, 1);

  if (toAppend.isNull()) {
    return AqlValue$(list);
  }

  bool unique = false;
  if (n == 3) {
    VPackSlice uniqueSlice = ExtractFunctionParameter(trx, parameters, 2);
    unique = ValueToBoolean(uniqueSlice);
  }

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  {
    VPackArrayBuilder guard(b.get());
    if (!list.isNull()) {
      TRI_ASSERT(list.isArray());
      for (auto const& it : VPackArrayIterator(list)) {
        b->add(it);
      }
    }
    if (!toAppend.isArray()) {
      if (!unique || !ListContainsElement(list, toAppend)) {
        b->add(toAppend);
      }
    } else {
      for (auto const& it : VPackArrayIterator(toAppend)) {
        if (!unique || !ListContainsElement(list, it)) {
          b->add(it);
        }
      }
    }
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function UNSHIFT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Unshift(arangodb::aql::Query* query,
                            arangodb::AqlTransaction* trx,
                            FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(UnshiftVPack(query, trx, tmp));
}

AqlValue$ Functions::UnshiftVPack(arangodb::aql::Query* query,
                            arangodb::AqlTransaction* trx,
                            VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 2 && n != 3) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "UNSHIFT", (int)2,
        (int)3);
  }
  VPackSlice list = ExtractFunctionParameter(trx, parameters, 0);

  if (!list.isNull() && !list.isArray()) {
    RegisterInvalidArgumentWarning(query, "UNSHIFT");
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  VPackSlice toAppend = ExtractFunctionParameter(trx, parameters, 1);
  bool unique = false;
  if (n == 3) {
    VPackSlice uniqueSlice = ExtractFunctionParameter(trx, parameters, 2);
    unique = ValueToBoolean(uniqueSlice);
  }

  if (unique && list.isArray() && ListContainsElement(list, toAppend)) {
    // Short circuit, nothing to do return list
    return AqlValue$(list);
  }

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  {
    VPackArrayBuilder guard(b.get());
    b->add(toAppend);
    if (list.isArray()) {
      for (auto const& it : VPackArrayIterator(list)) {
        b->add(it);
      }
    }
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function SHIFT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Shift(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
                          FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(ShiftVPack(query, trx, tmp));
}

AqlValue$ Functions::ShiftVPack(arangodb::aql::Query* query,
                                arangodb::AqlTransaction* trx,
                                VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "SHIFT", (int)1,
        (int)1);
  }

  VPackSlice list = ExtractFunctionParameter(trx, parameters, 0);
  if (list.isNull()) {
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }
  if (!list.isArray()) {
    RegisterInvalidArgumentWarning(query, "SHIFT");
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  {
    VPackArrayBuilder guard(b.get());
    if (list.length() > 0) {
      auto iterator = VPackArrayIterator(list);
      // This jumps over the first element
      while(iterator.next()) {
        b->add(iterator.value());
      }
    }
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function REMOVE_VALUE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::RemoveValue(arangodb::aql::Query* query,
                                arangodb::AqlTransaction* trx,
                                FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(RemoveValueVPack(query, trx, tmp));
}

AqlValue$ Functions::RemoveValueVPack(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
    VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 2 && n != 3) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "REMOVE_VALUE",
        (int)2, (int)3);
  }

  VPackSlice list = ExtractFunctionParameter(trx, parameters, 0);

  if (list.isNull()) {
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    {
      VPackArrayBuilder guard(b.get());
    }
    return AqlValue$(b.get());
  }

  if (!list.isArray()) {
    RegisterInvalidArgumentWarning(query, "REMOVE_VALUE");
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  try {
    VPackArrayBuilder guard(b.get());
    bool useLimit = false;
    size_t limit = static_cast<size_t>(list.length());

    VPackSlice toRemove = ExtractFunctionParameter(trx, parameters, 1);
    if (n == 3) {
      VPackSlice limitSlice = ExtractFunctionParameter(trx, parameters, 2);
      if (!limitSlice.isNull()) {
        bool unused = false;
        limit = static_cast<size_t>(ValueToNumber(limitSlice, unused));
        useLimit = true;
      }
    }
    for (auto const& it : VPackArrayIterator(list)) {
      if (useLimit && limit == 0) {
        // Just copy
        b->add(it);
        continue;
      }
      if (arangodb::basics::VelocyPackHelper::compare(toRemove, it, false) ==
          0) {
        --limit;
        continue;
      }
      b->add(it);
    }
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function REMOVE_VALUES
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::RemoveValues(arangodb::aql::Query* query,
                                 arangodb::AqlTransaction* trx,
                                 FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(RemoveValuesVPack(query, trx, tmp));
}

AqlValue$ Functions::RemoveValuesVPack(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
    VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "REMOVE_VALUES",
        (int)2, (int)2);
  }
  VPackSlice list = ExtractFunctionParameter(trx, parameters, 0);
  VPackSlice values = ExtractFunctionParameter(trx, parameters, 1);

  if (values.isNull()) {
    return AqlValue$(list);
  }

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  if (list.isNull()) {
    {
      VPackArrayBuilder guard(b.get());
    }
    return AqlValue$(b.get());
  }

  if (list.isArray() && values.isArray()) {
    try {
      VPackArrayBuilder guard(b.get());
      for (auto const& it : VPackArrayIterator(list)) {
        if (!ListContainsElement(values, it)) {
          b->add(it);
        }
      }
    } catch (...) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    return AqlValue$(b.get());
  }

  RegisterInvalidArgumentWarning(query, "REMOVE_VALUES");
  b->add(VPackValue(VPackValueType::Null));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function REMOVE_NTH
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::RemoveNth(arangodb::aql::Query* query,
                              arangodb::AqlTransaction* trx,
                              FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(RemoveNthVPack(query, trx, tmp));
}

AqlValue$ Functions::RemoveNthVPack(arangodb::aql::Query* query,
                                    arangodb::AqlTransaction* trx,
                                    VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "REMOVE_NTH", (int)2,
        (int)2);
  }

  VPackSlice list = ExtractFunctionParameter(trx, parameters, 0);

  if (list.isNull()) {
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    {
      VPackArrayBuilder guard(b.get());
    }
    return AqlValue$(b.get());
  }

  if (list.isArray()) {
    double const count = static_cast<double>(list.length());
    VPackSlice position = ExtractFunctionParameter(trx, parameters, 1);
    bool unused = false;
    double p = arangodb::basics::VelocyPackHelper::toDouble(position, unused);
    if (p >= count || p < -count) {
      return AqlValue$(list);
    }
    if (p < 0) {
      p += count;
    }
    size_t target = static_cast<size_t>(p);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    try {
      size_t cur = 0;
      VPackArrayBuilder guard(b.get());
      for (auto const& it : VPackArrayIterator(list)) {
        if (cur != target) {
          b->add(it);
        }
        cur++;
      }
    } catch (...) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    return AqlValue$(b.get());
  }

  RegisterInvalidArgumentWarning(query, "REMOVE_NTH");
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(VPackValueType::Null));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function NOT_NULL
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::NotNull(arangodb::aql::Query* query,
                            arangodb::AqlTransaction* trx,
                            FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(NotNullVPack(query, trx, tmp));
}

AqlValue$ Functions::NotNullVPack(arangodb::aql::Query* query,
                                  arangodb::AqlTransaction* trx,
                                  VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  for (size_t i = 0; i < n; ++i) {
    VPackSlice element = ExtractFunctionParameter(trx, parameters, i);
    if (!element.isNull()) {
      return AqlValue$(element);
    }
  }
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(VPackValueType::Null));
  return AqlValue$(b.get());
}


////////////////////////////////////////////////////////////////////////////////
/// @brief function CURRENT_DATABASE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::CurrentDatabase(arangodb::aql::Query* query,
                                    arangodb::AqlTransaction* trx,
                                    FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(CurrentDatabaseVPack(query, trx, tmp));
}

AqlValue$ Functions::CurrentDatabaseVPack(
    arangodb::aql::Query* query, arangodb::AqlTransaction*,
    VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 0) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "CURRENT_DATABASE",
        (int)0, (int)0);
  }
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(query->vocbase()->_name));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function COLLECTION_COUNT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::CollectionCount(arangodb::aql::Query* query,
                                    arangodb::AqlTransaction* trx,
                                    FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(CollectionCountVPack(query, trx, tmp));
}

AqlValue$ Functions::CollectionCountVPack(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
    VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "COLLECTION_COUNT",
        (int)1, (int)1);
  }

  VPackSlice element = ExtractFunctionParameter(trx, parameters, 0);
  if (!element.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "COLLECTION_COUNT");
  }

  std::string const colName =
      basics::VelocyPackHelper::getStringValue(element, "");

  auto resolver = trx->resolver();
  TRI_voc_cid_t cid = resolver->getCollectionIdLocal(colName);
  if (cid == 0) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  auto collection = trx->trxCollection(cid);

  // ensure the collection is loaded
  if (collection == nullptr) {
    int res = TRI_AddCollectionTransaction(trx->getInternals(), cid,
                                           TRI_TRANSACTION_READ,
                                           trx->nestingLevel(), true, true);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_FORMAT(res, "'%s'", colName.c_str());
    }

    TRI_EnsureCollectionsTransaction(trx->getInternals());
    collection = trx->trxCollection(cid);

    if (collection == nullptr) {
      THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                    "'%s'", colName.c_str());
    }
  }

  auto document = trx->documentCollection(cid);

  if (document == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(document->size()));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function VARIANCE_SAMPLE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::VarianceSample(arangodb::aql::Query* query,
                                   arangodb::AqlTransaction* trx,
                                   FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(VarianceSampleVPack(query, trx, tmp));
}

AqlValue$ Functions::VarianceSampleVPack(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
    VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "VARIANCE_SAMPLE",
        (int)1, (int)1);
  }

  VPackSlice list = ExtractFunctionParameter(trx, parameters, 0);

  if (!list.isArray()) {
    RegisterWarning(query, "VARIANCE_SAMPLE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  double value = 0.0;
  size_t count = 0;

  if (!Variance(list, value, count)) {
    RegisterWarning(query, "VARIANCE_SAMPLE",
                    TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  if (count < 2) {
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(value / (count - 1)));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function VARIANCE_POPULATION
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::VariancePopulation(arangodb::aql::Query* query,
                                       arangodb::AqlTransaction* trx,
                                       FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(VariancePopulationVPack(query, trx, tmp));
}

AqlValue$ Functions::VariancePopulationVPack(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
    VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH,
        "VARIANCE_POPULATION", (int)1, (int)1);
  }

  VPackSlice list = ExtractFunctionParameter(trx, parameters, 0);

  if (!list.isArray()) {
    RegisterWarning(query, "VARIANCE_POPULATION",
                    TRI_ERROR_QUERY_ARRAY_EXPECTED);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  double value = 0.0;
  size_t count = 0;

  if (!Variance(list, value, count)) {
    RegisterWarning(query, "VARIANCE_POPULATION",
                    TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  if (count < 1) {
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(value / count));
  return AqlValue$(b.get());
}


////////////////////////////////////////////////////////////////////////////////
/// @brief function STDDEV_SAMPLE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::StdDevSample(arangodb::aql::Query* query,
                                 arangodb::AqlTransaction* trx,
                                 FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(StdDevSampleVPack(query, trx, tmp));
}

AqlValue$ Functions::StdDevSampleVPack(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
    VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "STDDEV_SAMPLE",
        (int)1, (int)1);
  }

  VPackSlice list = ExtractFunctionParameter(trx, parameters, 0);

  if (!list.isArray()) {
    RegisterWarning(query, "STDDEV_SAMPLE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  double value = 0.0;
  size_t count = 0;

  if (!Variance(list, value, count)) {
    RegisterWarning(query, "STDDEV_SAMPLE",
                    TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  if (count < 2) {
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(sqrt(value / (count - 1) )));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function STDDEV_POPULATION
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::StdDevPopulation(arangodb::aql::Query* query,
                                     arangodb::AqlTransaction* trx,
                                     FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(StdDevPopulationVPack(query, trx, tmp));
}

AqlValue$ Functions::StdDevPopulationVPack(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
    VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "STDDEV_POPULATION",
        (int)1, (int)1);
  }

  VPackSlice list = ExtractFunctionParameter(trx, parameters, 0);

  if (!list.isArray()) {
    RegisterWarning(query, "STDDEV_POPULATION", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  double value = 0.0;
  size_t count = 0;

  if (!Variance(list, value, count)) {
    RegisterWarning(query, "STDDEV_POPULATION",
                    TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  if (count < 1) {
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(sqrt(value / count)));
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function MEDIAN
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Median(arangodb::aql::Query* query,
                           arangodb::AqlTransaction* trx,
                           FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(MedianVPack(query, trx, tmp));
}

AqlValue$ Functions::MedianVPack(arangodb::aql::Query* query,
                                 arangodb::AqlTransaction* trx,
                                 VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "MEDIAN", (int)1,
        (int)1);
  }

  VPackSlice list = ExtractFunctionParameter(trx, parameters, 0);

  if (!list.isArray()) {
    RegisterWarning(query, "MEDIAN", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  std::vector<double> values;
  if (!SortNumberList(list, values)) {
    RegisterWarning(query, "MEDIAN", TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  if (values.empty()) {
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }
  size_t const l = values.size();
  size_t midpoint = l / 2;

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  if (l % 2 == 0) {
    b->add(VPackValue((values[midpoint - 1] + values[midpoint]) / 2));
  } else {
    b->add(VPackValue(values[midpoint]));
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function PERCENTILE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Percentile(arangodb::aql::Query* query,
                               arangodb::AqlTransaction* trx,
                               FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(PercentileVPack(query, trx, tmp));
}

AqlValue$ Functions::PercentileVPack(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
    VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 2 && n != 3) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "PERCENTILE", (int)2,
        (int)3);
  }

  VPackSlice list = ExtractFunctionParameter(trx, parameters, 0);

  if (!list.isArray()) {
    RegisterWarning(query, "PERCENTILE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  VPackSlice border = ExtractFunctionParameter(trx, parameters, 1);

  if (!border.isNumber()) {
    RegisterWarning(query, "PERCENTILE",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  bool unused = false;
  double p = ValueToNumber(border, unused);
  if (p <= 0 || p > 100) {
    RegisterWarning(query, "PERCENTILE",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  bool useInterpolation = false;

  if (n == 3) {
    VPackSlice methodSlice = ExtractFunctionParameter(trx, parameters, 2);
    if (!methodSlice.isString()) {
      RegisterWarning(query, "PERCENTILE",
                      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
      b->add(VPackValue(VPackValueType::Null));
      return AqlValue$(b.get());
    }
    std::string method = methodSlice.copyString();
    if (method == "interpolation") {
      useInterpolation = true;
    } else if (method == "rank") {
      useInterpolation = false;
    } else {
      RegisterWarning(query, "PERCENTILE",
                      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
      b->add(VPackValue(VPackValueType::Null));
      return AqlValue$(b.get());
    }
  }

  std::vector<double> values;
  if (!SortNumberList(list, values)) {
    RegisterWarning(query, "PERCENTILE",
                    TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  if (values.empty()) {
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }
  size_t l = values.size();
  if (l == 1) {
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(values[0]));
    return AqlValue$(b.get());
  }

  TRI_ASSERT(l > 1);

  if (useInterpolation) {
    double idx = p * (l + 1) / 100;
    double pos = floor(idx);

    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    if (pos >= l) {
      b->add(VPackValue(values[l - 1]));
    } else if (pos <= 0) {
      b->add(VPackValue(VPackValueType::Null));
    } else {
      double delta = idx - pos;
      b->add(VPackValue(delta * (values[static_cast<size_t>(pos)] -
                                 values[static_cast<size_t>(pos) - 1]) +
                        values[static_cast<size_t>(pos) - 1]));
    }
    return AqlValue$(b.get());
  }
  double idx = p * l / 100;
  double pos = ceil(idx);
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  if (pos >= l) {
    b->add(VPackValue(values[l - 1]));
  } else if (pos <= 0) {
    b->add(VPackValue(VPackValueType::Null));
  } else {
    b->add(VPackValue(values[static_cast<size_t>(pos) - 1]));
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function RANGE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Range(arangodb::aql::Query* query,
                          arangodb::AqlTransaction* trx,
                          FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(RangeVPack(query, trx, tmp));
}

AqlValue$ Functions::RangeVPack(arangodb::aql::Query* query,
                                arangodb::AqlTransaction* trx,
                                VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n != 2 && n != 3) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "RANGE", (int)2,
        (int)3);
  }

  VPackSlice const leftSlice = ExtractFunctionParameter(trx, parameters, 0);
  VPackSlice const rightSlice = ExtractFunctionParameter(trx, parameters, 1);

  bool unused = true;
  double from = ValueToNumber(leftSlice, unused);
  double to = ValueToNumber(rightSlice, unused);

  double step = 0.0;
  if (n == 3) {
    VPackSlice const stepSlice = ExtractFunctionParameter(trx, parameters, 2);
    if (!stepSlice.isNull()) {
      step = ValueToNumber(stepSlice, unused);
    } else {
      // no step specified
      if (from <= to) {
        step = 1.0;
      } else {
        step = -1.0;
      }
    }
  } else {
    // no step specified
    if (from <= to) {
      step = 1.0;
    } else {
      step = -1.0;
    }
  }

  if (step == 0.0 || (from < to && step < 0.0) || (from > to && step > 0.0)) {
    RegisterWarning(query, "RANGE",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  try {
    VPackArrayBuilder guard(b.get());
    if (step < 0.0 && to <= from) {
      for (; from >= to; from += step) {
        b->add(VPackValue(from));
      }
    } else {
      for (; from <= to; from += step) {
        b->add(VPackValue(from));
      }
    }
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function POSITION
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Position(arangodb::aql::Query* query,
                             arangodb::AqlTransaction* trx,
                             FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(PositionVPack(query, trx, tmp));
}

AqlValue$ Functions::PositionVPack(arangodb::aql::Query* query,
                                   arangodb::AqlTransaction* trx,
                                   VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 2 && n != 3) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "POSITION", (int)2,
        (int)3);
  }

  VPackSlice list = ExtractFunctionParameter(trx, parameters, 0);

  if (!list.isArray()) {
    RegisterWarning(query, "POSITION", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
    b->add(VPackValue(VPackValueType::Null));
    return AqlValue$(b.get());
  }

  bool returnIndex = false;
  if (n == 3) {
    VPackSlice returnIndexSlice = ExtractFunctionParameter(trx, parameters, 2);
    returnIndex = ValueToBoolean(returnIndexSlice);
  }

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  if (list.length() > 0) {
    VPackSlice searchValue = ExtractFunctionParameter(trx, parameters, 1);

    size_t index;
    if (ListContainsElement(list, searchValue, index)) {
      if (returnIndex) {
        b->add(VPackValue(index));
      } else {
        b->add(VPackValue(true));
      }
    } else {
      if (returnIndex) {
        b->add(VPackValue(-1));
      } else {
        b->add(VPackValue(false));
      }
    }
  } else {
    if (returnIndex) {
      b->add(VPackValue(-1));
    } else {
      b->add(VPackValue(false));
    }
  }
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function FULLTEXT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Fulltext(arangodb::aql::Query* query,
                             arangodb::AqlTransaction* trx,
                             FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(FulltextVPack(query, trx, tmp));
}

AqlValue$ Functions::FulltextVPack(arangodb::aql::Query* query,
                                   arangodb::AqlTransaction* trx,
                                   VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 3 || n > 4) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "FULLTEXT", (int)3,
        (int)4);
  }

  auto resolver = trx->resolver();

  VPackSlice collectionSlice = ExtractFunctionParameter(trx, parameters, 0);

  if (!collectionSlice.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FULLTEXT");
  }

  std::string colName = collectionSlice.copyString();

  VPackSlice attribute = ExtractFunctionParameter(trx, parameters, 1);

  if (!attribute.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FULLTEXT");
  }

  std::string attributeName = attribute.copyString();

  VPackSlice queryString = ExtractFunctionParameter(trx, parameters, 2);

  if (!queryString.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FULLTEXT");
  }

  std::string queryValue = queryString.copyString();

  size_t maxResults = 0;  // 0 means "all results"
  if (n >= 4) {
    VPackSlice limit = ExtractFunctionParameter(trx, parameters, 3);
    if (!limit.isNull() && !limit.isNumber()) {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FULLTEXT");
    } else if (limit.isNumber()) {
      int64_t value = limit.getNumericValue<int64_t>();
      if (value > 0) {
        maxResults = static_cast<size_t>(value);
      }
    }
  }

  TRI_voc_cid_t cid = resolver->getCollectionIdLocal(colName);
  auto collection = trx->trxCollection(cid);

  // ensure the collection is loaded
  if (collection == nullptr) {
    int res = TRI_AddCollectionTransaction(trx->getInternals(), cid,
                                           TRI_TRANSACTION_READ,
                                           trx->nestingLevel(), true, true);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_FORMAT(res, "'%s'", colName.c_str());
    }

    TRI_EnsureCollectionsTransaction(trx->getInternals());
    collection = trx->trxCollection(cid);

    if (collection == nullptr) {
      THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                    "'%s'", colName.c_str());
    }
  }

  auto document = trx->documentCollection(cid);

  if (document == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
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
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FULLTEXT_INDEX_MISSING,
                                  colName.c_str());
  }

  if (trx->orderDitch(collection) == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  TRI_fulltext_query_t* ft =
      TRI_CreateQueryFulltextIndex(TRI_FULLTEXT_SEARCH_MAX_WORDS, maxResults);

  if (ft == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  bool isSubstringQuery = false;
  int res =
      TRI_ParseQueryFulltextIndex(ft, queryValue.c_str(), &isSubstringQuery);

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

  auto shaper = collection->_collection->_collection->getShaper();
  size_t const numResults = queryResult->_numDocuments;

  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  try {
    VPackArrayBuilder guard(b.get());

    for (size_t i = 0; i < numResults; ++i) {
      std::unordered_set<std::string> unused;
      ExpandShapedJson(shaper, resolver, cid,
                       (TRI_doc_mptr_t const*)queryResult->_documents[i], *b,
                       false, unused);
    }
  } catch (...) {
    TRI_FreeResultFulltextIndex(queryResult);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  TRI_FreeResultFulltextIndex(queryResult);
  return AqlValue$(b.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_SAME_COLLECTION
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsSameCollection(arangodb::aql::Query* query,
                                     arangodb::AqlTransaction* trx,
                                     FunctionParameters const& parameters) {
  auto tmp = transformParameters(parameters, trx);
  return AqlValue(IsSameCollectionVPack(query, trx, tmp));
}

AqlValue$ Functions::IsSameCollectionVPack(
    arangodb::aql::Query* query, arangodb::AqlTransaction* trx,
    VPackFunctionParameters const& parameters) {
  if (parameters.size() != 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "IS_SAME_COLLECTION",
        (int)2, (int)2);
  }

  VPackSlice collectionSlice = ExtractFunctionParameter(trx, parameters, 0);

  if (!collectionSlice.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "IS_SAME_COLLECTION");
  }

  std::string colName = collectionSlice.copyString();

  VPackSlice value = ExtractFunctionParameter(trx, parameters, 1);
  std::string identifier;

  if (value.isObject() && value.hasKey(TRI_VOC_ATTRIBUTE_ID)) {
    identifier = arangodb::basics::VelocyPackHelper::getStringValue(
        value, TRI_VOC_ATTRIBUTE_ID, "");
  } else if (value.isString()) {
    identifier = value.copyString();
  }

  if (!identifier.empty()) {
    size_t pos = identifier.find('/');

    if (pos != std::string::npos) {
      bool const same = (colName == identifier.substr(0, pos));

      std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
      b->add(VPackValue(same));
      return AqlValue$(b.get());
    }

    // fallthrough intentional
  }

  RegisterWarning(query, "IS_SAME_COLLECTION",
                  TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  std::shared_ptr<VPackBuilder> b = query->getSharedBuilder();
  b->add(VPackValue(VPackValueType::Null));
  return AqlValue$(b.get());
}

