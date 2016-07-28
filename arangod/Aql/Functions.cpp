////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, C++ implementation of AQL functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Functions.h"
#include "Aql/Function.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "Basics/fpconv.h"
#include "Basics/JsonHelper.h"
#include "Basics/json-utilities.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringBuffer.h"
#include "Basics/Utf8Helper.h"
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

using namespace triagens::aql;
using Json = triagens::basics::Json;
using CollectionNameResolver = triagens::arango::CollectionNameResolver;
using VertexId = triagens::arango::traverser::VertexId;

////////////////////////////////////////////////////////////////////////////////
/// @brief thread-local cache for compiled regexes
////////////////////////////////////////////////////////////////////////////////

thread_local std::unordered_map<std::string, RegexMatcher*>* RegexCache = nullptr;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief clear the regex cache in a thread
////////////////////////////////////////////////////////////////////////////////
  
static void ClearRegexCache () {
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

static std::string BuildRegexPattern (char const* ptr,
                                      size_t length,
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
      escaped = ! escaped;
    }
    else {
      if (c == '%') {
        if (escaped) {
          // literal %
          pattern.push_back('%');
        }
        else {
          // wildcard
          pattern.append("(.|[\r\n])*");
        }
      }
      else if (c == '_') {
        if (escaped) {
          // literal underscore
          pattern.push_back('_');
        }
        else {
          // wildcard character
          pattern.append("(.|[\r\n])");
        }
      }
      else if (c == '?' || c == '+' || c == '[' || c == '(' || c == ')' ||
               c == '{' || c == '}' || c == '^' || c == '$' || c == '|' || 
               c == '\\' || c == '.') {
        // character with special meaning in a regex
        pattern.push_back('\\');
        pattern.push_back(c);
      }
      else {
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

static Json ExtractFunctionParameter (triagens::arango::AqlTransaction* trx,
                                      FunctionParameters const& parameters,
                                      size_t position,
                                      bool copy) {
  if (position >= parameters.size()) {
    // parameter out of range
    return Json(Json::Null);
  }

  auto const& parameter = parameters[position];
  return parameter.first.toJson(trx, parameter.second, copy);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register warning
////////////////////////////////////////////////////////////////////////////////
            
static void RegisterWarning (triagens::aql::Query* query,
                             char const* functionName,
                             int code) {
  std::string msg;

  if (code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH) {
    msg = triagens::basics::Exception::FillExceptionString(code, functionName);
  }
  else {
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
            
static void RegisterInvalidArgumentWarning (triagens::aql::Query* query,
                                            char const* functionName) {
  RegisterWarning(query, functionName, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}
 
////////////////////////////////////////////////////////////////////////////////
/// @brief safely copies a TRI_json_t
/// "safely" here means the function will neither accept a nullptr as input
/// nor will ever return a nullptr
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* SafeCopyJson(TRI_json_t const* src) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(src != nullptr);
#endif

  if (src == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  auto copy = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, src);

  if (copy == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a value into a number value
////////////////////////////////////////////////////////////////////////////////

static double ValueToNumber (TRI_json_t const* json, 
                             bool& isValid) {
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
      try {
        std::string const str = triagens::basics::JsonHelper::getStringValue(json, "");
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
      }
      catch (...) {
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
        json = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, 0));
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

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a value into a boolean value
////////////////////////////////////////////////////////////////////////////////

static bool ValueToBoolean (TRI_json_t const* json) {
  bool boolValue = false;

  if (json->_type == TRI_JSON_BOOLEAN) {
    boolValue = json->_value._boolean;
  }
  else if (json->_type == TRI_JSON_NUMBER) {
    boolValue = (json->_value._number != 0.0);
  }
  else if (json->_type == TRI_JSON_STRING ||
           json->_type == TRI_JSON_STRING_REFERENCE) {
    // the null byte does not count
    boolValue = (json->_value._string.length > 1);
  }
  else if (json->_type == TRI_JSON_ARRAY ||
           json->_type == TRI_JSON_OBJECT) {
    boolValue = true;
  }
 
  return boolValue;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a boolean parameter from an array
////////////////////////////////////////////////////////////////////////////////

static bool GetBooleanParameter (triagens::arango::AqlTransaction* trx,
                                 FunctionParameters const& parameters,
                                 size_t startParameter,
                                 bool defaultValue) {
  size_t const n = parameters.size();

  if (startParameter >= n) {
    return defaultValue;  
  }
    
  auto temp = ExtractFunctionParameter(trx, parameters, startParameter, false);
  return ValueToBoolean(temp.json());
}
 
////////////////////////////////////////////////////////////////////////////////
/// @brief extract attribute names from the arguments
////////////////////////////////////////////////////////////////////////////////

static void ExtractKeys (std::unordered_set<std::string>& names,
                         triagens::aql::Query* query,
                         triagens::arango::AqlTransaction* trx,
                         FunctionParameters const& parameters,
                         size_t startParameter,
                         char const* functionName) {  
  size_t const n = parameters.size();

  for (size_t i = startParameter; i < n; ++i) {
    auto param = ExtractFunctionParameter(trx, parameters, i, false);

    if (param.isString()) {
      TRI_json_t const* json = param.json();
      names.emplace(triagens::basics::JsonHelper::getStringValue(json, ""));
    }
    else if (param.isNumber()) {
      TRI_json_t const* json = param.json();
      double number = json->_value._number;

      if (std::isnan(number) || number == HUGE_VAL || number == -HUGE_VAL) {
        names.emplace("null");
      } 
      else {
        char buffer[24];
        int length = fpconv_dtoa(number, &buffer[0]);
        names.emplace(std::string(&buffer[0], static_cast<size_t>(length)));
      }
    }
    else if (param.isArray()) {
      TRI_json_t const* p = param.json();

      size_t const n2 = param.size();
      for (size_t j = 0; j < n2; ++j) {
        auto v = static_cast<TRI_json_t const*>(TRI_AtVector(&p->_value._objects, j));
        if (TRI_IsStringJson(v)) {
          names.emplace(triagens::basics::JsonHelper::getStringValue(v, ""));
        }
        else {
          RegisterInvalidArgumentWarning(query, functionName);
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append the JSON value to a string buffer
////////////////////////////////////////////////////////////////////////////////

static void AppendAsString (triagens::basics::StringBuffer& buffer,
                            TRI_json_t const* json) {
  TRI_json_type_e const type = (json == nullptr ? TRI_JSON_UNUSED : json->_type);

  switch (type) {
    case TRI_JSON_UNUSED: 
    case TRI_JSON_NULL: {
      buffer.appendText(TRI_CHAR_LENGTH_PAIR("null"));
      break;
    }
    case TRI_JSON_BOOLEAN: {
      if (json->_value._boolean) {
        buffer.appendText(TRI_CHAR_LENGTH_PAIR("true"));
      }
      else {
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
      buffer.appendText(triagens::basics::JsonHelper::getStringValue(json, ""));
      break;
    }
    case TRI_JSON_ARRAY: {
      size_t const n = TRI_LengthArrayJson(json);
      for (size_t i = 0; i < n; ++i) {
        if (i > 0) {
          buffer.appendChar(',');
        }
        AppendAsString(buffer, static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i)));
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
/// @brief Checks if the given list contains the element
////////////////////////////////////////////////////////////////////////////////

static bool ListContainsElement (Json const& list, Json const& testee, size_t& index) {
  for (size_t i = 0; i < list.size(); ++i) {
    if (TRI_CheckSameValueJson(testee.json(), list.at(i).json())) {
      // We found the element in the list.
      index = i;
      return true;
    }
  }
  return false;
}

static bool ListContainsElement (Json const& list, Json const& testee) {
  size_t unused;
  return ListContainsElement(list, testee, unused);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Computes the Variance of the given list.
///        If successful value will contain the variance and count
///        will contain the number of elements.
///        If not successful value and count contain garbage.
////////////////////////////////////////////////////////////////////////////////

static bool Variance (Json const& values, double& value, size_t& count) {
  TRI_ASSERT(values.isArray());
  value = 0.0;
  count = 0;
  bool unused = false;
  double mean = 0;
  for (size_t i = 0; i < values.size(); ++i) {
    Json element = values.at(i);
    if (! element.isNull()) {
      if (! element.isNumber()) {
        return false;
      }
      double current = ValueToNumber(element.json(), unused);
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

static bool SortNumberList (Json const& values, std::vector<double>& result) {
  TRI_ASSERT(values.isArray());
  TRI_ASSERT(result.empty());
  bool unused;
  for (size_t i = 0; i < values.size(); ++i) {
    Json element = values.at(i);
    if (! element.isNull()) {
      if (! element.isNumber()) {
        return false;
      }
      result.emplace_back(ValueToNumber(element.json(), unused));
    }
  }
  std::sort(result.begin(), result.end());
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Expands a shaped Json
////////////////////////////////////////////////////////////////////////////////

static inline Json ExpandShapedJson (VocShaper* shaper,
                                     CollectionNameResolver const* resolver,
                                     TRI_voc_cid_t const& cid,
                                     TRI_doc_mptr_t const* mptr) {
  TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(mptr->getDataPtr());

  TRI_shaped_json_t shaped;
  TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, marker);
  Json json(shaper->memoryZone(), TRI_JsonShapedJson(shaper, &shaped));
  char const* key = TRI_EXTRACT_MARKER_KEY(marker);
  std::string id(resolver->getCollectionName(cid));
  id.push_back('/');
  id.append(key);
  json(TRI_VOC_ATTRIBUTE_ID, Json(id));
  json(TRI_VOC_ATTRIBUTE_REV, Json(std::to_string(TRI_EXTRACT_MARKER_RID(marker))));
  json(TRI_VOC_ATTRIBUTE_KEY, Json(key));

  if (TRI_IS_EDGE_MARKER(marker)) {
    std::string from(resolver->getCollectionNameCluster(TRI_EXTRACT_MARKER_FROM_CID(marker)));
    from.push_back('/');
    from.append(TRI_EXTRACT_MARKER_FROM_KEY(marker));
    json(TRI_VOC_ATTRIBUTE_FROM, Json(from));
    std::string to(resolver->getCollectionNameCluster(TRI_EXTRACT_MARKER_TO_CID(marker)));

    to.push_back('/');
    to.append(TRI_EXTRACT_MARKER_TO_KEY(marker));
    json(TRI_VOC_ATTRIBUTE_TO, Json(to));
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Reads a document by cid and key
///        Also lazy locks the collection.
///        Returns null if the document does not exist
////////////////////////////////////////////////////////////////////////////////

static Json ReadDocument (triagens::arango::AqlTransaction* trx,
                          CollectionNameResolver const* resolver,
                          TRI_voc_cid_t cid,
                          char const* key) {
  auto collection = trx->trxCollection(cid);

  if (collection == nullptr) {
    int res = TRI_AddCollectionTransaction(trx->getInternals(), 
                                           cid,
                                           TRI_TRANSACTION_READ,
                                           trx->nestingLevel(),
                                           true,
                                           true);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
    TRI_EnsureCollectionsTransaction(trx->getInternals());
    collection = trx->trxCollection(cid);

    if (collection == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "collection is a nullptr");
    }
  }
  
  TRI_doc_mptr_copy_t mptr;
  int res = trx->readSingle(collection, &mptr, key); 

  if (res != TRI_ERROR_NO_ERROR) {
    return Json(Json::Null);
  }

  return ExpandShapedJson(
    collection->_collection->_collection->getShaper(),
    resolver,
    cid,
    &mptr
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function to filter the given list of mptr
////////////////////////////////////////////////////////////////////////////////

static void FilterDocuments (triagens::arango::ExampleMatcher const* matcher,
                             TRI_voc_cid_t cid,
                             std::vector<TRI_doc_mptr_copy_t>& toFilter) {
  if (matcher == nullptr) {
    return;
  }
  size_t resultCount = toFilter.size();
  for (size_t i = 0; i < resultCount; /* nothing */) {
    if (! matcher->matches(cid, &toFilter[i])) {
      toFilter.erase(toFilter.begin() + i);
      --resultCount;
    }
    else {
      ++i;
    }
  }
}

static void RequestEdges (triagens::basics::Json const& vertexJson,
                          triagens::arango::AqlTransaction* trx,
                          CollectionNameResolver const* resolver,
                          VocShaper* shaper,
                          TRI_voc_cid_t cid,
                          TRI_document_collection_t* collection,
                          TRI_edge_direction_e direction,
                          triagens::arango::ExampleMatcher const* matcher,
                          bool includeVertices,
                          triagens::basics::Json& result) {
  std::string vertexId;
  if (vertexJson.isString()) {
    vertexId = triagens::basics::JsonHelper::getStringValue(vertexJson.json(), "");
  }
  else if (vertexJson.isObject()) {
    vertexId = triagens::basics::JsonHelper::getStringValue(vertexJson.json(), "_id", "");
  }
  else {
    // Nothing to do.
    // Return (error for illegal input is thrown outside
    return;
  }

  std::vector<std::string> parts = triagens::basics::StringUtils::split(vertexId, "/");
  if (parts.size() != 2) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD, vertexId);
  }

  TRI_voc_cid_t startCid = resolver->getCollectionId(parts[0]);
  if (startCid == 0) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                  "'%s'",
                                  parts[0].c_str());
  }

  char* key = const_cast<char*>(parts[1].c_str());
  std::vector<TRI_doc_mptr_copy_t> edges = TRI_LookupEdgesDocumentCollection(
    collection,
    direction,
    startCid,
    key
  );
  FilterDocuments(matcher, cid, edges);
  size_t resultCount = edges.size();
  result.reserve(resultCount);

  if (includeVertices) {
    for (size_t i = 0; i < resultCount; ++i) {
      Json resultPair(Json::Object, 2);
      resultPair.set("edge", ExpandShapedJson(
        shaper,
        resolver,
        cid,
        &(edges[i])
      ));
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

      resultPair.set("vertex", ReadDocument(trx, resolver, targetCid, targetKey));
      result.add(resultPair);
    }
  }
  else {
    for (size_t i = 0; i < resultCount; ++i) {
      result.add(ExpandShapedJson(
        shaper,
        resolver,
        cid,
        &(edges[i])
      ));
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                      AQL functions public helpers
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief called before a query starts
/// has the chance to set up any thread-local storage
////////////////////////////////////////////////////////////////////////////////

void Functions::InitializeThreadContext () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief called when a query ends
/// its responsibility is to clear any thread-local storage
////////////////////////////////////////////////////////////////////////////////

void Functions::DestroyThreadContext () {
  ClearRegexCache();
}

// -----------------------------------------------------------------------------
// --SECTION--                                             AQL function bindings
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_NULL
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsNull (triagens::aql::Query*, 
                            triagens::arango::AqlTransaction* trx,
                            FunctionParameters const& parameters) {
  auto const value = ExtractFunctionParameter(trx, parameters, 0, false);
  return AqlValue(new Json(value.isNull()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_BOOL
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsBool (triagens::aql::Query*,
                            triagens::arango::AqlTransaction* trx,
                            FunctionParameters const& parameters) {
  auto const value = ExtractFunctionParameter(trx, parameters, 0, false);
  return AqlValue(new Json(value.isBoolean()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_NUMBER
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsNumber (triagens::aql::Query*,
                              triagens::arango::AqlTransaction* trx,
                              FunctionParameters const& parameters) {
  auto const value = ExtractFunctionParameter(trx, parameters, 0, false);
  return AqlValue(new Json(value.isNumber()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_STRING
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsString (triagens::aql::Query*,
                              triagens::arango::AqlTransaction* trx,
                              FunctionParameters const& parameters) {
  auto const value = ExtractFunctionParameter(trx, parameters, 0, false);
  return AqlValue(new Json(value.isString()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_ARRAY
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsArray (triagens::aql::Query*,
                             triagens::arango::AqlTransaction* trx,
                             FunctionParameters const& parameters) {
  auto const value = ExtractFunctionParameter(trx, parameters, 0, false);
  return AqlValue(new Json(value.isArray()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_OBJECT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsObject (triagens::aql::Query*,
                              triagens::arango::AqlTransaction* trx,
                              FunctionParameters const& parameters) {
  auto const value = ExtractFunctionParameter(trx, parameters, 0, false);
  return AqlValue(new Json(value.isObject()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function TO_NUMBER
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::ToNumber (triagens::aql::Query*,
                             triagens::arango::AqlTransaction* trx,
                             FunctionParameters const& parameters) {
  auto const value = ExtractFunctionParameter(trx, parameters, 0, false);

  bool isValid;
  double v = ValueToNumber(value.json(), isValid);

  if (! isValid) {
    return AqlValue(new Json(Json::Null));
  }
  return AqlValue(new Json(v));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function TO_STRING
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::ToString (triagens::aql::Query*,
                              triagens::arango::AqlTransaction* trx,
                              FunctionParameters const& parameters) {
  auto const value = ExtractFunctionParameter(trx, parameters, 0, false);

  triagens::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE, 24);

  AppendAsString(buffer, value.json());
  size_t length = buffer.length();
  std::unique_ptr<TRI_json_t> j(TRI_CreateStringJson(TRI_UNKNOWN_MEM_ZONE, buffer.steal(), length));

  if (j == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, j.get());
  j.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function TO_BOOL
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::ToBool (triagens::aql::Query*,
                            triagens::arango::AqlTransaction* trx,
                            FunctionParameters const& parameters) {
  auto const value = ExtractFunctionParameter(trx, parameters, 0, false);

  return AqlValue(new Json(ValueToBoolean(value.json())));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function TO_ARRAY
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::ToArray (triagens::aql::Query*,
                             triagens::arango::AqlTransaction* trx,
                             FunctionParameters const& parameters) {
  auto const value = ExtractFunctionParameter(trx, parameters, 0, false);

  if (value.isBoolean() ||
      value.isNumber() ||
      value.isString()) {
    // return array with single member
    Json array(Json::Array, 1);
    array.add(SafeCopyJson(value.json()));

    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, array.steal()));
  }
  if (value.isArray()) {
    // return copy of the original array
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE,
                             SafeCopyJson(value.json())));
  }
  if (value.isObject()) {
    // return an array with the attribute values
    auto const source = value.json();
    size_t const n = TRI_LengthVector(&source->_value._objects);

    Json array(Json::Array, n);
    for (size_t i = 1; i < n; i += 2) {
      auto v = static_cast<TRI_json_t const*>(TRI_AtVector(&source->_value._objects, i));
      array.add(SafeCopyJson(v));
    } 

    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, array.steal()));
  }

  // return empty array
  return AqlValue(new Json(Json::Array));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function LENGTH
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Length (triagens::aql::Query*,
                            triagens::arango::AqlTransaction* trx,
                            FunctionParameters const& parameters) {
  if (! parameters.empty() &&
      parameters[0].first.isArray()) {
    // shortcut!
    return AqlValue(new Json(static_cast<double>(parameters[0].first.arraySize())));
  }

  auto const value = ExtractFunctionParameter(trx, parameters, 0, false);

  TRI_json_t const* json = value.json();
  size_t length = 0;

  if (json != nullptr) {
    switch (json->_type) {
      case TRI_JSON_UNUSED:
      case TRI_JSON_NULL: {
        length = 0;
        break;
      }

      case TRI_JSON_BOOLEAN: {
        length = (json->_value._boolean ? 1 : 0);
        break;
      }

      case TRI_JSON_NUMBER: {
        if (std::isnan(json->_value._number) ||
            ! std::isfinite(json->_value._number)) {
          // invalid value
          length = strlen("null");
        }
        else {
          // convert to a string representation of the number
          char buffer[24];
          length = static_cast<size_t>(fpconv_dtoa(json->_value._number, buffer));
        }
        break;
      }

      case TRI_JSON_STRING:
      case TRI_JSON_STRING_REFERENCE: {
        // return number of characters (not bytes) in string
        length = TRI_CharLengthUtf8String(json->_value._string.data);
        break;
      }

      case TRI_JSON_OBJECT: {
        // return number of attributes
        length = TRI_LengthVector(&json->_value._objects) / 2;
        break;
      }

      case TRI_JSON_ARRAY: {
        // return list length
        length = TRI_LengthArrayJson(json);
        break;
      }
    }
  }

  return AqlValue(new Json(static_cast<double>(length)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function FIRST
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::First (triagens::aql::Query* query,
                           triagens::arango::AqlTransaction* trx,
                           FunctionParameters const& parameters) {
  if (parameters.size() < 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "FIRST", (int) 1, (int) 1);
  }

  auto value = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! value.isArray()) {
    // not an array
    RegisterWarning(query, "FIRST", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }
 
  if (value.size() == 0) {
    return AqlValue(new Json(Json::Null));
  }

  auto j = new Json(TRI_UNKNOWN_MEM_ZONE, value.at(0).copy().steal(), Json::AUTOFREE);
  return AqlValue(j);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function LAST
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Last (triagens::aql::Query* query,
                          triagens::arango::AqlTransaction* trx,
                          FunctionParameters const& parameters) {
  if (parameters.size() < 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "LAST", (int) 1, (int) 1);
  }

  auto value = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! value.isArray()) {
    // not an array
    RegisterWarning(query, "LAST", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }

  size_t const n = value.size(); 

  if (n == 0) {
    return AqlValue(new Json(Json::Null));
  }

  auto j = new Json(TRI_UNKNOWN_MEM_ZONE, value.at(n - 1).copy().steal(), Json::AUTOFREE);
  return AqlValue(j);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function NTH
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Nth (triagens::aql::Query* query,
                         triagens::arango::AqlTransaction* trx,
                         FunctionParameters const& parameters) {
  if (parameters.size() < 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "LAST", (int) 2, (int) 2);
  }

  auto value = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! value.isArray()) {
    // not an array
    RegisterWarning(query, "NTH", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }

  size_t const n = value.size(); 

  if (n == 0) {
    return AqlValue(new Json(Json::Null));
  }

  Json indexJson = ExtractFunctionParameter(trx, parameters, 1, false);
  bool isValid = true;
  double numValue = ValueToNumber(indexJson.json(), isValid);

  if (! isValid || numValue < 0.0) {
    return AqlValue(new Json(Json::Null));
  }

  size_t index = static_cast<size_t>(numValue);

  if (index >= n) {
    return AqlValue(new Json(Json::Null));
  }

  auto j = new Json(TRI_UNKNOWN_MEM_ZONE, value.at(index).copy().steal(), Json::AUTOFREE);
  return AqlValue(j);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function CONCAT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Concat (triagens::aql::Query*,
                            triagens::arango::AqlTransaction* trx,
                            FunctionParameters const& parameters) {
  triagens::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE, 24);

  size_t const n = parameters.size();

  for (size_t i = 0; i < n; ++i) {
    auto const member = ExtractFunctionParameter(trx, parameters, i, false);

    if (member.isEmpty() || member.isNull()) {
      continue;
    }
      
    TRI_json_t const* json = member.json();
    
    if (member.isArray()) {
      // append each member individually
      size_t const subLength = TRI_LengthArrayJson(json);

      for (size_t j = 0; j < subLength; ++j) {
        auto sub = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, j));

        if (sub == nullptr || sub->_type == TRI_JSON_NULL) {
          continue;
        }

        AppendAsString(buffer, sub);
      }
    }
    else {
      // convert member to a string and append
      AppendAsString(buffer, json);
    }
  }
  
  // steal the StringBuffer's char* pointer so we can avoid copying data around
  // multiple times
  size_t length = buffer.length();
  std::unique_ptr<TRI_json_t> j(TRI_CreateStringJson(TRI_UNKNOWN_MEM_ZONE, buffer.steal(), length));

  if (j == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, j.get());
  j.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function LIKE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Like (triagens::aql::Query* query,
                          triagens::arango::AqlTransaction* trx,
                          FunctionParameters const& parameters) {
  if (parameters.size() < 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "LIKE", (int) 2, (int) 3);
  }

  bool const caseInsensitive = GetBooleanParameter(trx, parameters, 2, false);
  triagens::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE, 24);

  // build pattern from parameter #1
  auto const regex = ExtractFunctionParameter(trx, parameters, 1, false);
  AppendAsString(buffer, regex.json());
  size_t const length = buffer.length();

  std::string const pattern = BuildRegexPattern(buffer.c_str(), length, caseInsensitive);
  RegexMatcher* matcher = nullptr;

  if (RegexCache != nullptr) {
    auto it = RegexCache->find(pattern);

    // check regex cache
    if (it != RegexCache->end()) {
      matcher = (*it).second;
    }
  }

  if (matcher == nullptr) {
    matcher = triagens::basics::Utf8Helper::DefaultUtf8Helper.buildMatcher(pattern);

    try {
      if (RegexCache == nullptr) {
        RegexCache = new std::unordered_map<std::string, RegexMatcher*>();
      }
      // insert into cache, no matter if pattern is valid or not
      RegexCache->emplace(pattern, matcher);
    }
    catch (...) {
      delete matcher;
      ClearRegexCache();
      throw;
    }
  }
  
  if (matcher == nullptr) {
    // compiling regular expression failed
    RegisterWarning(query, "LIKE", TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(new Json(Json::Null));
  }

  // extract value
  buffer.clear();
  auto const value = ExtractFunctionParameter(trx, parameters, 0, false);
  AppendAsString(buffer, value.json());
 
  bool error = false;
  bool const result = triagens::basics::Utf8Helper::DefaultUtf8Helper.matches(matcher, buffer.c_str(), buffer.length(), error);

  if (error) {
    // compiling regular expression failed
    RegisterWarning(query, "LIKE", TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(new Json(Json::Null));
  }
        
  return AqlValue(new Json(result));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function PASSTHRU
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Passthru (triagens::aql::Query*,
                              triagens::arango::AqlTransaction* trx,
                              FunctionParameters const& parameters) {

  if (parameters.empty()) {
    return AqlValue(new Json(Json::Null));
  }

  auto json = ExtractFunctionParameter(trx, parameters, 0, true);
  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, json.steal()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function UNSET
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Unset (triagens::aql::Query* query,
                           triagens::arango::AqlTransaction* trx,
                           FunctionParameters const& parameters) {
  auto value = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! value.isObject()) {
    RegisterInvalidArgumentWarning(query, "UNSET");
    return AqlValue(new Json(Json::Null));
  }
 
  std::unordered_set<std::string> names;
  ExtractKeys(names, query, trx, parameters, 1, "UNSET");


  // create result object
  TRI_json_t const* valueJson = value.json();
  size_t const n = TRI_LengthVector(&valueJson->_value._objects);

  size_t size;
  if (names.size() >= n / 2) {
    size = 4; 
  }
  else {
    size = (n / 2) - names.size(); 
  }

  std::unique_ptr<TRI_json_t> j(TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE, size));

  if (j == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  for (size_t i = 0; i < n; i += 2) {
    auto key = static_cast<TRI_json_t const*>(TRI_AtVector(&valueJson->_value._objects, i));
    auto value = static_cast<TRI_json_t const*>(TRI_AtVector(&valueJson->_value._objects, i + 1));

    if (TRI_IsStringJson(key) && 
        names.find(key->_value._string.data) == names.end()) {
      auto copy = SafeCopyJson(value);

      if (copy == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      } 

      TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, j.get(), key->_value._string.data, copy);
    }
  } 

  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, j.get());
  j.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function UNSET_RECURSIVE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::UnsetRecursive (triagens::aql::Query* query,
                                    triagens::arango::AqlTransaction* trx,
                                    FunctionParameters const& parameters) {
  auto value = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! value.isObject()) {
    RegisterInvalidArgumentWarning(query, "UNSET_RECURSIVE");
    return AqlValue(new Json(Json::Null));
  }
 
  std::unordered_set<std::string> names;
  ExtractKeys(names, query, trx, parameters, 1, "UNSET_RECURSIVE");

  std::function<TRI_json_t*(TRI_json_t const*, std::unordered_set<std::string> const&)> func;

  func = [&func](TRI_json_t const* value, std::unordered_set<std::string> const& names) -> TRI_json_t* {
    TRI_ASSERT(TRI_IsObjectJson(value));

    size_t const n = TRI_LengthVector(&value->_value._objects);
    size_t size;
    if (names.size() >= n / 2) {
      size = 4; 
    }
    else {
      size = (n / 2) - names.size(); 
    }

    std::unique_ptr<TRI_json_t> j(TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE, size));

    if (j == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    for (size_t i = 0; i < n; i += 2) {
      auto k = static_cast<TRI_json_t const*>(TRI_AtVector(&value->_value._objects, i));
      auto v = static_cast<TRI_json_t const*>(TRI_AtVector(&value->_value._objects, i + 1));

      if (TRI_IsStringJson(k) && 
          names.find(k->_value._string.data) == names.end()) {
        TRI_json_t* copy = nullptr;

        if (TRI_IsObjectJson(v)) {
          copy = func(v, names);     
        }
        else {
          copy = SafeCopyJson(v);
        }

        if (copy == nullptr) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
        } 

        TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, j.get(), k->_value._string.data, copy);
      }
    }

    return j.release();
  };

  TRI_json_t* result = func(value.json(), names);
  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, result);
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function KEEP
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Keep (triagens::aql::Query* query,
                          triagens::arango::AqlTransaction* trx,
                          FunctionParameters const& parameters) {
  auto value = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! value.isObject()) {
    RegisterInvalidArgumentWarning(query, "KEEP");
    return AqlValue(new Json(Json::Null));
  }
 
  std::unordered_set<std::string> names;
  ExtractKeys(names, query, trx, parameters, 1, "KEEP");


  // create result object
  std::unique_ptr<TRI_json_t> j(TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE, names.size()));

  if (j == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  TRI_json_t const* valueJson = value.json();
  size_t const n = TRI_LengthVector(&valueJson->_value._objects);

  for (size_t i = 0; i < n; i += 2) {
    auto key = static_cast<TRI_json_t const*>(TRI_AtVector(&valueJson->_value._objects, i));
    auto value = static_cast<TRI_json_t const*>(TRI_AtVector(&valueJson->_value._objects, i + 1));

    if (TRI_IsStringJson(key) && 
        names.find(key->_value._string.data) != names.end()) {
      auto copy = SafeCopyJson(value);

      if (copy == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      } 

      TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, j.get(), key->_value._string.data, copy);
    }
  } 

  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, j.get());
  j.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function MERGE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Merge (triagens::aql::Query* query,
                           triagens::arango::AqlTransaction* trx,
                           FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n == 0) {
    // no parameters
    return AqlValue(new Json(Json::Object));
  }

  // use the first argument as the preliminary result
  auto initial = ExtractFunctionParameter(trx, parameters, 0, true);

  if (initial.isArray() && n == 1) {
    // special case: a single array parameter
    std::unique_ptr<TRI_json_t> array(initial.steal());
    std::unique_ptr<TRI_json_t> result(TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE));

    if (result == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  
    // now merge in all other arguments
    size_t const k = TRI_LengthArrayJson(array.get());

    for (size_t i = 0; i < k; ++i) {
      auto v = static_cast<TRI_json_t const*>(TRI_LookupArrayJson(array.get(), i));

      if (! TRI_IsObjectJson(v)) {
        RegisterInvalidArgumentWarning(query, "MERGE");
        return AqlValue(new Json(Json::Null));
      }

      auto merged = TRI_MergeJson(TRI_UNKNOWN_MEM_ZONE, result.get(), v, false, false);
  
      if (merged == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }

      result.reset(merged);
    }
     
    auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, result.get());
    result.release();
    return AqlValue(jr);
  }

  if (! initial.isObject()) {
    RegisterInvalidArgumentWarning(query, "MERGE");
    return AqlValue(new Json(Json::Null));
  }

  std::unique_ptr<TRI_json_t> result(initial.steal());

  // now merge in all other arguments
  for (size_t i = 1; i < n; ++i) {
    auto param = ExtractFunctionParameter(trx, parameters, i, false);

    if (! param.isObject()) {
      RegisterInvalidArgumentWarning(query, "MERGE");
      return AqlValue(new Json(Json::Null));
    }
 
    auto merged = TRI_MergeJson(TRI_UNKNOWN_MEM_ZONE, result.get(), param.json(), false, false);

    if (merged == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    result.reset(merged);
  } 

  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, result.get());
  result.release();
  return AqlValue(jr);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief function MERGE_RECURSIVE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::MergeRecursive (triagens::aql::Query* query,
                                    triagens::arango::AqlTransaction* trx,
                                    FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n == 0) {
    // no parameters
    return AqlValue(new Json(Json::Object));
  }

  // use the first argument as the preliminary result
  auto initial = ExtractFunctionParameter(trx, parameters, 0, true);

  if (initial.isArray() && n == 1) {
    // special case: a single array parameter
    std::unique_ptr<TRI_json_t> array(initial.steal());
    std::unique_ptr<TRI_json_t> result(TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE));

    if (result == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  
    // now merge in all other arguments
    size_t const k = TRI_LengthArrayJson(array.get());

    for (size_t i = 0; i < k; ++i) {
      auto v = static_cast<TRI_json_t const*>(TRI_LookupArrayJson(array.get(), i));

      if (! TRI_IsObjectJson(v)) {
        RegisterInvalidArgumentWarning(query, "MERGE_RECURSIVE");
        return AqlValue(new Json(Json::Null));
      }

      auto merged = TRI_MergeJson(TRI_UNKNOWN_MEM_ZONE, result.get(), v, false, true);
  
      if (merged == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }

      result.reset(merged);
    }
     
    auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, result.get());
    result.release();
    return AqlValue(jr);
  }

  if (! initial.isObject()) {
    RegisterInvalidArgumentWarning(query, "MERGE_RECURSIVE");
    return AqlValue(new Json(Json::Null));
  }

  std::unique_ptr<TRI_json_t> result(initial.steal());

  // now merge in all other arguments
  for (size_t i = 1; i < n; ++i) {
    auto param = ExtractFunctionParameter(trx, parameters, i, false);

    if (! param.isObject()) {
      RegisterInvalidArgumentWarning(query, "MERGE_RECURSIVE");
      return AqlValue(new Json(Json::Null));
    }
 
    auto merged = TRI_MergeJson(TRI_UNKNOWN_MEM_ZONE, result.get(), param.json(), false, true);

    if (merged == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    result.reset(merged);
  } 

  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, result.get());
  result.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function HAS
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Has (triagens::aql::Query* query,
                         triagens::arango::AqlTransaction* trx,
                         FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 2) {
    // no parameters
    return AqlValue(new Json(false));
  }
    
  auto value = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! value.isObject()) {
    // not an object
    return AqlValue(new Json(false));
  }
 
  // process name parameter 
  auto name = ExtractFunctionParameter(trx, parameters, 1, false);

  char const* p;

  if (! name.isString()) {
    triagens::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
    AppendAsString(buffer, name.json());
    p = buffer.c_str();
  }
  else {
    p = name.json()->_value._string.data;
  }
 
  bool const hasAttribute = (TRI_LookupObjectJson(value.json(), p) != nullptr);
  return AqlValue(new Json(hasAttribute));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function ATTRIBUTES
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Attributes (triagens::aql::Query* query,
                                triagens::arango::AqlTransaction* trx,
                                FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 1) {
    // no parameters
    return AqlValue(new Json(Json::Null));
  }
    
  auto value = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! value.isObject()) {
    // not an object
    RegisterWarning(query, "ATTRIBUTES", TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(new Json(Json::Null));
  }
 
  bool const removeInternal = GetBooleanParameter(trx, parameters, 1, false);
  bool const doSort = GetBooleanParameter(trx, parameters, 2, false);

  auto const valueJson = value.json();
  TRI_ASSERT(TRI_IsObjectJson(valueJson));

  size_t const numValues = TRI_LengthVectorJson(valueJson);

  if (numValues == 0) {
    // empty object
    return AqlValue(new Json(Json::Object));
  }

  std::vector<std::pair<char const*, size_t>> sortPositions;
  sortPositions.reserve(numValues / 2);

  // create a vector with positions into the object
  for (size_t i = 0; i < numValues; i += 2) {
    auto key = static_cast<TRI_json_t const*>(TRI_AddressVector(&valueJson->_value._objects, i));

    if (! TRI_IsStringJson(key)) {
      // somehow invalid
      continue;
    }

    if (removeInternal && *key->_value._string.data == '_') {
      // skip attribute
      continue;
    }

    sortPositions.emplace_back(std::make_pair(key->_value._string.data, i));
  }

  if (doSort) {
    // sort according to attribute name
    std::sort(sortPositions.begin(), sortPositions.end(), [] (std::pair<char const*, size_t> const& lhs,
                                                              std::pair<char const*, size_t> const& rhs) -> bool {
      return TRI_compare_utf8(lhs.first, rhs.first) < 0;
    });
  }

  // create the output
  Json result(Json::Array, sortPositions.size());

  // iterate over either sorted or unsorted object 
  for (auto const& it : sortPositions) {
    auto key = static_cast<TRI_json_t const*>(TRI_AddressVector(&valueJson->_value._objects, it.second));

    result.add(Json(triagens::basics::JsonHelper::getStringValue(key, "")));
  } 

  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, result.steal()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function VALUES
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Values (triagens::aql::Query* query,
                            triagens::arango::AqlTransaction* trx,
                            FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 1) {
    // no parameters
    return AqlValue(new Json(Json::Null));
  }
    
  auto value = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! value.isObject()) {
    // not an object
    RegisterWarning(query, "ATTRIBUTES", TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(new Json(Json::Null));
  }
 
  bool const removeInternal = GetBooleanParameter(trx, parameters, 1, false);

  auto const valueJson = value.json();
  TRI_ASSERT(TRI_IsObjectJson(valueJson));

  size_t const numValues = TRI_LengthVectorJson(valueJson);

  if (numValues == 0) {
    // empty object
    return AqlValue(new Json(Json::Object));
  }

  // create the output
  Json result(Json::Array, numValues);

  // create a vector with positions into the object
  for (size_t i = 0; i < numValues; i += 2) {
    auto key = static_cast<TRI_json_t const*>(TRI_AddressVector(&valueJson->_value._objects, i));

    if (! TRI_IsStringJson(key)) {
      // somehow invalid
      continue;
    }

    if (removeInternal && *key->_value._string.data == '_') {
      // skip attribute
      continue;
    }

    auto value = static_cast<TRI_json_t const*>(TRI_AddressVector(&valueJson->_value._objects, i + 1));
    result.add(Json(TRI_UNKNOWN_MEM_ZONE, SafeCopyJson(value)));
  }

  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, result.steal()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function MIN
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Min (triagens::aql::Query* query,
                         triagens::arango::AqlTransaction* trx,
                         FunctionParameters const& parameters) {
  auto value = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! value.isArray()) {
    // not an array
    RegisterWarning(query, "MIN", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }

  TRI_json_t const* valueJson = value.json();
  size_t const n = TRI_LengthArrayJson(valueJson);
  TRI_json_t const* minValue = nullptr;;

  for (size_t i = 0; i < n; ++i) {
    auto value = static_cast<TRI_json_t const*>(TRI_AtVector(&valueJson->_value._objects, i));

    if (TRI_IsNullJson(value)) {
      continue;
    }

    if (minValue == nullptr ||
        TRI_CompareValuesJson(value, minValue) < 0) {
      minValue = value;
    }
  } 

  if (minValue != nullptr) {
    std::unique_ptr<TRI_json_t> result(SafeCopyJson(minValue));
    
    if (result != nullptr) {
      auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, result.get());
      result.release();
      return AqlValue(jr);
    }
  }

  return AqlValue(new Json(Json::Null));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function MAX
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Max (triagens::aql::Query* query,
                         triagens::arango::AqlTransaction* trx,
                         FunctionParameters const& parameters) {
  auto value = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! value.isArray()) {
    // not an array
    RegisterWarning(query, "MAX", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }

  TRI_json_t const* valueJson = value.json();
  size_t const n = TRI_LengthArrayJson(valueJson);
  TRI_json_t const* maxValue = nullptr;;

  for (size_t i = 0; i < n; ++i) {
    auto value = static_cast<TRI_json_t const*>(TRI_AtVector(&valueJson->_value._objects, i));

    if (TRI_IsNullJson(value)) {
      continue;
    }

    if (maxValue == nullptr ||
        TRI_CompareValuesJson(value, maxValue) > 0) {
      maxValue = value;
    }
  } 

  if (maxValue != nullptr) {
    std::unique_ptr<TRI_json_t> result(SafeCopyJson(maxValue));
    
    if (result != nullptr) {
      auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, result.get());
      result.release();
      return AqlValue(jr);
    }
  }

  return AqlValue(new Json(Json::Null));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function SUM
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Sum (triagens::aql::Query* query,
                         triagens::arango::AqlTransaction* trx,
                         FunctionParameters const& parameters) {
  auto value = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! value.isArray()) {
    // not an array
    RegisterWarning(query, "SUM", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }

  TRI_json_t const* valueJson = value.json();
  size_t const n = TRI_LengthArrayJson(valueJson);

  double sum = 0.0;

  for (size_t i = 0; i < n; ++i) {
    auto value = static_cast<TRI_json_t const*>(TRI_AtVector(&valueJson->_value._objects, i));

    if (TRI_IsNullJson(value)) {
      continue;
    }

    if (! TRI_IsNumberJson(value)) {
      RegisterInvalidArgumentWarning(query, "SUM");
      return AqlValue(new Json(Json::Null));
    }

    // got a numeric value
    double const number = value->_value._number;

    if (! std::isnan(number) && number != HUGE_VAL && number != -HUGE_VAL) {
      sum += number;
    } 
  } 

  if (! std::isnan(sum) && sum != HUGE_VAL && sum != -HUGE_VAL) {
    return AqlValue(new Json(sum));
  } 

  return AqlValue(new Json(Json::Null));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function AVERAGE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Average (triagens::aql::Query* query,
                             triagens::arango::AqlTransaction* trx,
                             FunctionParameters const& parameters) {
  auto value = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! value.isArray()) {
    // not an array
    RegisterWarning(query, "AVERAGE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }

  TRI_json_t const* valueJson = value.json();
  size_t const n = TRI_LengthArrayJson(valueJson);
  double sum = 0.0;
  size_t count = 0;

  for (size_t i = 0; i < n; ++i) {
    auto value = static_cast<TRI_json_t const*>(TRI_AtVector(&valueJson->_value._objects, i));

    if (TRI_IsNullJson(value)) {
      continue;
    }

    if (! TRI_IsNumberJson(value)) {
      RegisterInvalidArgumentWarning(query, "AVERAGE");
      return AqlValue(new Json(Json::Null));
    }

    // got a numeric value
    double const number = value->_value._number;

    if (! std::isnan(number) && number != HUGE_VAL && number != -HUGE_VAL) {
      sum += number;
      ++count;
    } 
  } 

  if (count > 0 && 
      ! std::isnan(sum) && sum != HUGE_VAL && sum != -HUGE_VAL) {
    return AqlValue(new Json(sum / static_cast<size_t>(count)));
  } 

  return AqlValue(new Json(Json::Null));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function MD5
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Md5 (triagens::aql::Query* query,
                         triagens::arango::AqlTransaction* trx,
                         FunctionParameters const& parameters) {
  auto value = ExtractFunctionParameter(trx, parameters, 0, false);
    
  triagens::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
  AppendAsString(buffer, value.json());
  
  // create md5
  char hash[17]; 
  char* p = &hash[0];
  size_t length;

  triagens::rest::SslInterface::sslMD5(buffer.c_str(), buffer.length(), p, length);

  // as hex
  char hex[33];
  p = &hex[0];

  triagens::rest::SslInterface::sslHEX(hash, 16, p, length);

  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, hex, 32));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function SHA1
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Sha1 (triagens::aql::Query* query,
                          triagens::arango::AqlTransaction* trx,
                          FunctionParameters const& parameters) {
  auto value = ExtractFunctionParameter(trx, parameters, 0, false);
    
  triagens::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
  AppendAsString(buffer, value.json());
  
  // create sha1
  char hash[21];
  char* p = &hash[0];
  size_t length;

  triagens::rest::SslInterface::sslSHA1(buffer.c_str(), buffer.length(), p, length);

  // as hex
  char hex[41];
  p = &hex[0];

  triagens::rest::SslInterface::sslHEX(hash, 20, p, length);

  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, hex, 40));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function UNIQUE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Unique (triagens::aql::Query* query,
                            triagens::arango::AqlTransaction* trx,
                            FunctionParameters const& parameters) {
  if (parameters.size() != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "UNIQUE", (int) 1, (int) 1);
  }

  auto const value = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! value.isArray()) {
    // not an array
    RegisterWarning(query, "UNIQUE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }
  
  std::unordered_set<TRI_json_t const*, triagens::basics::JsonHash, triagens::basics::JsonEqual> values(
    512, 
    triagens::basics::JsonHash(), 
    triagens::basics::JsonEqual()
  );

  TRI_json_t const* valueJson = value.json();
  size_t const n = TRI_LengthArrayJson(valueJson);

  for (size_t i = 0; i < n; ++i) {
    auto value = static_cast<TRI_json_t const*>(TRI_AddressVector(&valueJson->_value._objects, i));

    if (value == nullptr) {
      continue;
    }

    values.emplace(value); 
  } 

  std::unique_ptr<TRI_json_t> result(TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE, values.size()));

  if (result == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
 
  for (auto const& it : values) {
    auto copy = SafeCopyJson(it);
    TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, result.get(), copy); 
  }
      
  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, result.get());
  result.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function SORTED_UNIQUE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::SortedUnique (triagens::aql::Query* query,
                                  triagens::arango::AqlTransaction* trx,
                                  FunctionParameters const& parameters) {
  if (parameters.size() != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "SORTED_UNIQUE", (int) 1, (int) 1);
  }

  auto const value = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! value.isArray()) {
    // not an array
    // this is an internal function - do NOT issue a warning here
    return AqlValue(new Json(Json::Null));
  }
  
  std::set<TRI_json_t const*, triagens::basics::JsonLess<true>> values;

  TRI_json_t const* valueJson = value.json();
  size_t const n = TRI_LengthArrayJson(valueJson);

  for (size_t i = 0; i < n; ++i) {
    auto value = static_cast<TRI_json_t const*>(TRI_AddressVector(&valueJson->_value._objects, i));

    if (value == nullptr) {
      continue;
    }

    values.insert(value); 
  } 

  std::unique_ptr<TRI_json_t> result(TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE, values.size()));

  if (result == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
 
  for (auto const& it : values) {
    auto copy = SafeCopyJson(it);

    if (copy == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
 
    TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, result.get(), copy); 
  }
      
  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, result.get());
  result.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function UNION
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Union (triagens::aql::Query* query,
                           triagens::arango::AqlTransaction* trx,
                           FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "UNION", (int) 2, (int) Function::MaxArguments);
  }

  std::unique_ptr<TRI_json_t> result(TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE, 16));

  if (result == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  for (size_t i = 0; i < n; ++i) {
    auto value = ExtractFunctionParameter(trx, parameters, i, false);

    if (! value.isArray()) {
      // not an array
      RegisterInvalidArgumentWarning(query, "UNION");
      return AqlValue(new Json(Json::Null));
    }

    TRI_json_t const* valueJson = value.json();
    size_t const nrValues = TRI_LengthArrayJson(valueJson);

    if (TRI_ReserveVector(&(result.get()->_value._objects), nrValues) != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    TRI_IF_FAILURE("AqlFunctions::OutOfMemory1") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    
    // this passes ownership for the JSON contens into result
    for (size_t j = 0; j < nrValues; ++j) {
      TRI_json_t* copy = SafeCopyJson(TRI_LookupArrayJson(valueJson, j));

      if (copy == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
    
      TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, result.get(), copy);

      TRI_IF_FAILURE("AqlFunctions::OutOfMemory2") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    } 
  } 
      
  TRI_IF_FAILURE("AqlFunctions::OutOfMemory3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, result.get());
  result.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function UNION_DISTINCT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::UnionDistinct (triagens::aql::Query* query,
                                   triagens::arango::AqlTransaction* trx,
                                   FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "UNION_DISTINCT", (int) 2, (int) Function::MaxArguments);
  }

  std::unordered_set<TRI_json_t*, triagens::basics::JsonHash, triagens::basics::JsonEqual> values(
    512, 
    triagens::basics::JsonHash(), 
    triagens::basics::JsonEqual()
  );

  auto freeValues = [&values] () -> void {
    for (auto& it : values) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, it);
    }
  };

  std::unique_ptr<TRI_json_t> result;

  try {
    for (size_t i = 0; i < n; ++i) {
      auto value = ExtractFunctionParameter(trx, parameters, i, false);

      if (! value.isArray()) {
        // not an array
        freeValues();
        RegisterInvalidArgumentWarning(query, "UNION_DISTINCT");
        return AqlValue(new Json(Json::Null));
      }

      TRI_json_t const* valueJson = value.json();
      size_t const nrValues = TRI_LengthArrayJson(valueJson);

      for (size_t j = 0; j < nrValues; ++j) {
        auto value = static_cast<TRI_json_t*>(TRI_AddressVector(&valueJson->_value._objects, j));

        if (values.find(value) == values.end()) { 
          std::unique_ptr<TRI_json_t> copy(SafeCopyJson(value));

          if (copy == nullptr) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }
      
          TRI_IF_FAILURE("AqlFunctions::OutOfMemory1") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }

          values.emplace(copy.get());
          copy.release();
        }
      }
    }

    result.reset(TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE, values.size()));

    if (result == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
          
    TRI_IF_FAILURE("AqlFunctions::OutOfMemory2") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
   
    for (auto const& it : values) {
      TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, result.get(), it); 
    }

  }
  catch (...) {  
    freeValues();
    throw;
  }
    
  TRI_IF_FAILURE("AqlFunctions::OutOfMemory3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
      
  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, result.get());
  result.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function INTERSECTION
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Intersection (triagens::aql::Query* query,
                                  triagens::arango::AqlTransaction* trx,
                                  FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "INTERSECTION", (int) 2, (int) Function::MaxArguments);
  }

  std::unordered_map<TRI_json_t*, size_t, triagens::basics::JsonHash, triagens::basics::JsonEqual> values(
    512, 
    triagens::basics::JsonHash(), 
    triagens::basics::JsonEqual()
  );

  auto freeValues = [&values] () -> void {
    for (auto& it : values) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, it.first);
    }
    values.clear();
  };

  std::unique_ptr<TRI_json_t> result;

  try {
    for (size_t i = 0; i < n; ++i) {
      auto value = ExtractFunctionParameter(trx, parameters, i, false);

      if (! value.isArray()) {
        // not an array
        freeValues();
        RegisterWarning(query, "INTERSECTION", TRI_ERROR_QUERY_ARRAY_EXPECTED);
        return AqlValue(new Json(Json::Null));
      }

      TRI_json_t const* valueJson = value.json();
      size_t const nrValues = TRI_LengthArrayJson(valueJson);

      for (size_t j = 0; j < nrValues; ++j) {
        auto value = static_cast<TRI_json_t const*>(TRI_AddressVector(&valueJson->_value._objects, j));

        if (i == 0) {
          // round one
          std::unique_ptr<TRI_json_t> copy(SafeCopyJson(value));

          if (copy == nullptr) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }
    
          TRI_IF_FAILURE("AqlFunctions::OutOfMemory1") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }

          auto r = values.emplace(copy.get(), 1);
 
          if (r.second) {
            // successfully inserted
            copy.release();
          }
        }
        else {
          // check if we have seen the same element before
          auto it = values.find(const_cast<TRI_json_t*>(value));

          if (it != values.end()) {
            // already seen
            TRI_ASSERT((*it).second > 0);
            if ((*it).second < i) {
              (*it).second = 0;
            } else {
              (*it).second = i + 1;
            }
          }
        }
      }
    }
 
    // count how many valid we have 
    size_t total = 0;

    for (auto const& it : values) {
      if (it.second == n) {
        ++total;
      }
    }

    result.reset(TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE, total));

    if (result == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
          
    TRI_IF_FAILURE("AqlFunctions::OutOfMemory2") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
   
    for (auto& it : values) {
      if (it.second == n) {
        TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, result.get(), it.first); 
      }
      else {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, it.first);
      }
    }
    values.clear();
   
  } 
  catch (...) {
    freeValues();
    throw;
  }
    
  TRI_IF_FAILURE("AqlFunctions::OutOfMemory3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
      
  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, result.get());
  result.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Transforms VertexId to Json
////////////////////////////////////////////////////////////////////////////////

static Json VertexIdToJson (triagens::arango::AqlTransaction* trx,
                            CollectionNameResolver const* resolver,
                            VertexId const& id) {
  auto collection = trx->trxCollection(id.cid);

  if (collection == nullptr) {
    int res = TRI_AddCollectionTransaction(trx->getInternals(), 
                                           id.cid,
                                           TRI_TRANSACTION_READ,
                                           trx->nestingLevel(),
                                           true,
                                           true);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    TRI_EnsureCollectionsTransaction(trx->getInternals());
    collection = trx->trxCollection(id.cid);

    if (collection == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "collection is a nullptr");
    }
  }
  
  TRI_doc_mptr_copy_t mptr;
  int res = trx->readSingle(collection, &mptr, id.key); 

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
      return Json(Json::Null);
    }
    THROW_ARANGO_EXCEPTION(res);
  }

  return ExpandShapedJson(
    collection->_collection->_collection->getShaper(),
    resolver,
    id.cid,
    &mptr
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Transforms VertexId to std::string
////////////////////////////////////////////////////////////////////////////////

static std::string VertexIdToString (CollectionNameResolver const* resolver,
                                     VertexId const& id) {
  return resolver->getCollectionName(id.cid) + "/" + std::string(id.key);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Transforms an unordered_map<VertexId> to AQL json values
////////////////////////////////////////////////////////////////////////////////

static AqlValue VertexIdsToAqlValue (triagens::arango::AqlTransaction* trx,
                                     CollectionNameResolver const* resolver,
                                     std::unordered_set<VertexId>& ids,
                                     bool includeData = false) {
  std::unique_ptr<Json> result(new Json(Json::Array, ids.size()));

  if (includeData) {
    for (auto& it : ids) {
      result->add(Json(VertexIdToJson(trx, resolver, it)));
    }
  } 
  else {
    for (auto& it : ids) {
      result->add(Json(VertexIdToString(resolver, it)));
    }
  }

  AqlValue v(result.get());
  result.release();

  return v;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function NEIGHBORS
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Neighbors (triagens::aql::Query* query,
                               triagens::arango::AqlTransaction* trx,
                               FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  triagens::arango::traverser::NeighborsOptions opts;

  if (n < 4 || n > 6) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "NEIGHBORS", (int) 4, (int) 6);
  }

  auto resolver = trx->resolver();

  Json vertexCol = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! vertexCol.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
  }
  std::string vColName = basics::JsonHelper::getStringValue(vertexCol.json(), "");

  Json edgeCol = ExtractFunctionParameter(trx, parameters, 1, false);

  if (! edgeCol.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
  }
  std::string eColName = basics::JsonHelper::getStringValue(edgeCol.json(), "");

  Json vertexInfo = ExtractFunctionParameter(trx, parameters, 2, false);
  std::string vertexId;
  if (vertexInfo.isString()) {
    vertexId = basics::JsonHelper::getStringValue(vertexInfo.json(), "");
    if (vertexId.find("/") != std::string::npos) {
      size_t split;
      char const* str = vertexId.c_str();

      if (! TRI_ValidateDocumentIdKeyGenerator(str, &split)) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
      }

      std::string const collectionName = vertexId.substr(0, split);
      if (collectionName.compare(vColName) != 0) {
        THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_GRAPH_INVALID_PARAMETER,
                                      "specified vertex collection '%s' does not match start vertex collection '%s'",
                                      vColName.c_str(),
                                      collectionName.c_str());
    }
      auto coli = resolver->getCollectionStruct(collectionName);

      if (coli == nullptr) {
        THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                      "'%s'",
                                      collectionName.c_str());
      }

      VertexId v(coli->_cid, const_cast<char*>(str + split + 1));
      opts.start = v;
    }
    else {
      VertexId v(resolver->getCollectionId(vColName), vertexId.c_str());
      opts.start = v;
    }
  }
  else if (vertexInfo.isObject()) {
    if (! vertexInfo.has("_id")) {
      THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
    }
    vertexId = basics::JsonHelper::getStringValue(vertexInfo.get("_id").json(), "");
    size_t split;
    char const* str = vertexId.c_str();

    if (! TRI_ValidateDocumentIdKeyGenerator(str, &split)) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
    }

    std::string const collectionName = vertexId.substr(0, split);
    if (collectionName.compare(vColName) != 0) {
      THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_GRAPH_INVALID_PARAMETER,
                                    "specified vertex collection '%s' does not match start vertex collection '%s'",
                                    vColName.c_str(),
                                    collectionName.c_str());
    }
    auto coli = resolver->getCollectionStruct(collectionName);

    if (coli == nullptr) {
      THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                    "'%s'",
                                    collectionName.c_str());
    }

    VertexId v(coli->_cid, const_cast<char*>(str + split + 1));
    opts.start = v;
  }
  else {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
  }

  Json direction = ExtractFunctionParameter(trx, parameters, 3, false);
  if (direction.isString()) {
    std::string const dir = basics::JsonHelper::getStringValue(direction.json(), "");
    if (dir.compare("outbound") == 0) {
      opts.direction = TRI_EDGE_OUT;
    }
    else if (dir.compare("inbound") == 0) {
      opts.direction = TRI_EDGE_IN;
    }
    else if (dir.compare("any") == 0) {
      opts.direction = TRI_EDGE_ANY;
    }
    else {
      THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
    }
  }
  else {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
  }

  bool includeData = false;

  if (n > 5) {
    auto options = ExtractFunctionParameter(trx, parameters, 5, false);
    if (! options.isObject()) {
      THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEIGHBORS");
    }
    includeData = basics::JsonHelper::getBooleanValue(options.json(), "includeData", false);
    opts.minDepth = basics::JsonHelper::getNumericValue<uint64_t>(options.json(), "minDepth", 1);
    if (opts.minDepth == 0) {
      opts.maxDepth = basics::JsonHelper::getNumericValue<uint64_t>(options.json(), "maxDepth", 1);
    } 
    else {
      opts.maxDepth = basics::JsonHelper::getNumericValue<uint64_t>(options.json(), "maxDepth", opts.minDepth);
    }
  }

  TRI_voc_cid_t eCid = resolver->getCollectionId(eColName);

  {
    // ensure the collection is loaded
    auto collection = trx->trxCollection(eCid);

    if (collection == nullptr) {
      int res = TRI_AddCollectionTransaction(trx->getInternals(), 
                                             eCid,
                                             TRI_TRANSACTION_READ,
                                             trx->nestingLevel(),
                                             true,
                                             true);
      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }

      TRI_EnsureCollectionsTransaction(trx->getInternals());
      collection = trx->trxCollection(eCid);

      if (collection == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "collection is a nullptr");
      }
    }
  }

  // Function to return constant distance
  auto wc = [](TRI_doc_mptr_copy_t&) -> double { return 1; };

  std::unique_ptr<EdgeCollectionInfo> eci(new EdgeCollectionInfo(
    eCid,
    trx->documentCollection(eCid),
    wc
  ));
  TRI_IF_FAILURE("EdgeCollectionInfoOOM1") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  
  if (n > 4) {
    auto edgeExamples = ExtractFunctionParameter(trx, parameters, 4, false);
    if (! (edgeExamples.isArray() && edgeExamples.size() == 0) ) {
      opts.addEdgeFilter(edgeExamples, eci->getShaper(), eCid, resolver); 
    }
  }
  
  std::vector<EdgeCollectionInfo*> edgeCollectionInfos;
  triagens::basics::ScopeGuard guard{
    []() -> void { },
    [&edgeCollectionInfos]() -> void {
      for (auto& p : edgeCollectionInfos) {
        delete p;
      }
    }
  };
  edgeCollectionInfos.emplace_back(eci.get());
  eci.release();
  TRI_IF_FAILURE("EdgeCollectionInfoOOM2") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  std::unordered_set<VertexId> neighbors;
  TRI_RunNeighborsSearch(
    edgeCollectionInfos,
    opts,
    neighbors
  );

  return VertexIdsToAqlValue(trx, resolver, neighbors, includeData);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function NEAR
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Near (triagens::aql::Query* query,
                          triagens::arango::AqlTransaction* trx,
                          FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 3 || n > 5) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "NEAR", (int) 3, (int) 5);
  }

  auto resolver = trx->resolver();

  Json collectionJson = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! collectionJson.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEAR");
  }

  std::string colName = basics::JsonHelper::getStringValue(collectionJson.json(), "");

  Json latitude  = ExtractFunctionParameter(trx, parameters, 1, false);
  Json longitude = ExtractFunctionParameter(trx, parameters, 2, false);

  if (! latitude.isNumber() ||
      ! longitude.isNumber()) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEAR");
  }

  // extract limit 
  int64_t limitValue = 100;

  if (n > 3) { 
    Json limit = ExtractFunctionParameter(trx, parameters, 3, false);

    if (limit.isNumber()) {
      limitValue = static_cast<int64_t>(limit.json()->_value._number);
    }
    else if (! limit.isNull()) {
      THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEAR");
    }
  }

  
  std::string attributeName;
  if (n > 4) {
    // have a distance attribute
    Json distanceAttribute = ExtractFunctionParameter(trx, parameters, 4, false);

    if (! distanceAttribute.isNull() && ! distanceAttribute.isString()) {
      THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEAR");
    }

    if (distanceAttribute.isString()) {
      attributeName = std::string(distanceAttribute.json()->_value._string.data);
    } 
  }

  TRI_voc_cid_t cid = resolver->getCollectionId(colName);
  auto collection = trx->trxCollection(cid);

  // ensure the collection is loaded
  if (collection == nullptr) {
    int res = TRI_AddCollectionTransaction(trx->getInternals(), 
                                           cid,
                                           TRI_TRANSACTION_READ,
                                           trx->nestingLevel(),
                                           true,
                                           true);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_FORMAT(res, "'%s'", colName.c_str());
    }

    TRI_EnsureCollectionsTransaction(trx->getInternals());
    collection = trx->trxCollection(cid);

    if (collection == nullptr) {
      THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                    "'%s'",
                                    colName.c_str());
    }
  }

  auto document = trx->documentCollection(cid);

  if (document == nullptr) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                  "'%s'",
                                  colName.c_str());
  }

  triagens::arango::Index* index = nullptr;

  for (auto const& idx : document->allIndexes()) {
    if (idx->type() == triagens::arango::Index::TRI_IDX_TYPE_GEO1_INDEX ||
        idx->type() == triagens::arango::Index::TRI_IDX_TYPE_GEO2_INDEX) {
      index = idx;
      break;
    } 
  }

  if (index == nullptr) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_GEO_INDEX_MISSING, colName.c_str());
  }
  
  if (trx->orderDitch(collection) == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  
  GeoCoordinates* cors = static_cast<triagens::arango::GeoIndex2*>(index)->nearQuery(
    latitude.json()->_value._number, 
    longitude.json()->_value._number, 
    limitValue
  );

  if (cors == nullptr) {
    Json array(Json::Array, 0);
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, array.steal()));
  }

  size_t const nCoords = cors->length;

  if (nCoords == 0) {
    GeoIndex_CoordinatesFree(cors);

    Json array(Json::Array, 0);
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, array.steal()));
  }

  struct geo_coordinate_distance_t {
    geo_coordinate_distance_t (double distance, TRI_doc_mptr_t const* mptr)
      : _distance(distance),
        _mptr(mptr) {
    }

    double                _distance;
    TRI_doc_mptr_t const* _mptr;
  };

  std::vector<geo_coordinate_distance_t> distances;
  try {
    distances.reserve(nCoords);
 
    for (size_t i = 0; i < nCoords; ++i) {
      distances.emplace_back(geo_coordinate_distance_t(cors->distances[i], static_cast<TRI_doc_mptr_t const*>(cors->coordinates[i].data)));
    }
  }
  catch (...) {
    GeoIndex_CoordinatesFree(cors);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
    
  GeoIndex_CoordinatesFree(cors);

  // sort result by distance
  std::sort(distances.begin(), distances.end(), [] (geo_coordinate_distance_t const& left, geo_coordinate_distance_t const& right) {
    return left._distance < right._distance;
  });
  
  auto shaper = collection->_collection->_collection->getShaper();
  Json array(Json::Array, nCoords);

  for (auto& it : distances) {
    auto j = ExpandShapedJson(
      shaper,
      resolver,
      cid,
      it._mptr
    );

    if (! attributeName.empty()) {
      j.unset(attributeName);
      j.set(attributeName, Json(it._distance));  
    }

    array.add(j);
  }

  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, array.steal()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function WITHIN
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Within (triagens::aql::Query* query,
                            triagens::arango::AqlTransaction* trx,
                            FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 4 || n > 5) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "WITHIN", (int) 4, (int) 5);
  }

  auto resolver = trx->resolver();

  Json collectionJson = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! collectionJson.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "WITHIN");
  }

  std::string colName = basics::JsonHelper::getStringValue(collectionJson.json(), "");

  Json latitude  = ExtractFunctionParameter(trx, parameters, 1, false);
  Json longitude = ExtractFunctionParameter(trx, parameters, 2, false);
  Json radius    = ExtractFunctionParameter(trx, parameters, 3, false);

  if (! latitude.isNumber() ||
      ! longitude.isNumber() ||
      ! radius.isNumber()) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "WITHIN");
  }
  
  std::string attributeName;
  if (n > 4) {
    // have a distance attribute
    Json distanceAttribute = ExtractFunctionParameter(trx, parameters, 4, false);

    if (! distanceAttribute.isNull() && ! distanceAttribute.isString()) {
      THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "WITHIN");
    }

    if (distanceAttribute.isString()) {
      attributeName = std::string(distanceAttribute.json()->_value._string.data);
    } 
  } 

  TRI_voc_cid_t cid = resolver->getCollectionId(colName);
  auto collection = trx->trxCollection(cid);

  // ensure the collection is loaded
  if (collection == nullptr) {
    int res = TRI_AddCollectionTransaction(trx->getInternals(), 
                                           cid,
                                           TRI_TRANSACTION_READ,
                                           trx->nestingLevel(),
                                           true,
                                           true);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_FORMAT(res, "'%s'", colName.c_str());
    }

    TRI_EnsureCollectionsTransaction(trx->getInternals());
    collection = trx->trxCollection(cid);

    if (collection == nullptr) {
      THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                    "'%s'",
                                    colName.c_str());
    }
  }

  auto document = trx->documentCollection(cid);
    
  if (document == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND); /// TODO
  }

  triagens::arango::Index* index = nullptr;

  for (auto const& idx : document->allIndexes()) {
    if (idx->type() == triagens::arango::Index::TRI_IDX_TYPE_GEO1_INDEX ||
        idx->type() == triagens::arango::Index::TRI_IDX_TYPE_GEO2_INDEX) {
      index = idx;
      break;
    } 
  }

  if (index == nullptr) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_GEO_INDEX_MISSING, colName.c_str());
  }
  
  if (trx->orderDitch(collection) == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  GeoCoordinates* cors = static_cast<triagens::arango::GeoIndex2*>(index)->withinQuery(
    latitude.json()->_value._number, 
    longitude.json()->_value._number, 
    radius.json()->_value._number
  );

  if (cors == nullptr) {
    Json array(Json::Array, 0);
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, array.steal()));
  }

  size_t const nCoords = cors->length;

  if (nCoords == 0) {
    GeoIndex_CoordinatesFree(cors);

    Json array(Json::Array, 0);
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, array.steal()));
  }

  struct geo_coordinate_distance_t {
    geo_coordinate_distance_t (double distance, TRI_doc_mptr_t const* mptr)
      : _distance(distance),
        _mptr(mptr) {
    }

    double                _distance;
    TRI_doc_mptr_t const* _mptr;
  };

  std::vector<geo_coordinate_distance_t> distances;
  try {
    distances.reserve(nCoords);
 
    for (size_t i = 0; i < nCoords; ++i) {
      distances.emplace_back(geo_coordinate_distance_t(cors->distances[i], static_cast<TRI_doc_mptr_t const*>(cors->coordinates[i].data)));
    }
  }
  catch (...) {
    GeoIndex_CoordinatesFree(cors);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
    
  GeoIndex_CoordinatesFree(cors);

  // sort result by distance
  std::sort(distances.begin(), distances.end(), [] (geo_coordinate_distance_t const& left, geo_coordinate_distance_t const& right) {
    return left._distance < right._distance;
  });
  
  auto shaper = collection->_collection->_collection->getShaper();
  Json array(Json::Array, nCoords);

  for (auto& it : distances) {
    auto j = ExpandShapedJson(
      shaper,
      resolver,
      cid,
      it._mptr
    );

    if (! attributeName.empty()) {
      j.unset(attributeName);
      j.set(attributeName, Json(it._distance));  
    }

    array.add(j);
  }

  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, array.steal()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief internal recursive flatten helper
////////////////////////////////////////////////////////////////////////////////

static void FlattenList (Json array, size_t maxDepth, size_t curDepth, Json& result) {
  size_t elementCount = array.size();
  for (size_t i = 0; i < elementCount; ++i) {
    Json tmp = array.at(i);
    if (tmp.isArray() && curDepth < maxDepth) {
      FlattenList(tmp, maxDepth, curDepth + 1, result);
    }
    else {
      // Transfer the content of tmp into the result
      result.transfer(tmp.json());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function FLATTEN
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Flatten (triagens::aql::Query* query,
                             triagens::arango::AqlTransaction* trx,
                             FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n == 0 || n > 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "FLATTEN", (int) 1, (int) 2);
  }

  Json listJson  = ExtractFunctionParameter(trx, parameters, 0, false);
  if (! listJson.isArray()) {
    RegisterWarning(query, "FLATTEN", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }

  size_t maxDepth = 1;
  if (n == 2) {
    Json maxDepthJson = ExtractFunctionParameter(trx, parameters, 1, false);
    bool isValid = true;
    double tmpMaxDepth = ValueToNumber(maxDepthJson.json(), isValid);
    if (! isValid || tmpMaxDepth < 1) {
      maxDepth = 1;
    }
    else {
      maxDepth = static_cast<size_t>(tmpMaxDepth);
    }
  }

  Json result(Json::Array);
  FlattenList(listJson.copy(), maxDepth, 0, result);

  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, result.steal()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function ZIP
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Zip (triagens::aql::Query* query,
                         triagens::arango::AqlTransaction* trx,
                         FunctionParameters const& parameters) {
  if (parameters.size() != 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "ZIP", (int) 2, (int) 2);
  }

  Json keysJson  = ExtractFunctionParameter(trx, parameters, 0, false);
  Json valuesJson  = ExtractFunctionParameter(trx, parameters, 1, false);
  if (! keysJson.isArray() ||
      ! valuesJson.isArray() ||
      keysJson.size() != valuesJson.size()) {
    RegisterWarning(query, "ZIP", TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(new Json(Json::Null));
  }
  size_t const n = keysJson.size();

  Json result(Json::Object, n);

  // Buffer will temporarily hold the keys
  triagens::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE, 24);
  for (size_t i = 0; i < n; ++i) {
    buffer.reset();
    AppendAsString(buffer, keysJson.at(i).json());
    result.set(buffer.c_str(), valuesJson.at(i).copy().steal());
  }
  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, result.steal()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function PARSE_IDENTIFIER
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::ParseIdentifier (triagens::aql::Query* query,
                                     triagens::arango::AqlTransaction* trx,
                                     FunctionParameters const& parameters) {
  if (parameters.size() != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "PARSE_IDENTIFIER", (int) 1, (int) 1);
  }

  Json value = ExtractFunctionParameter(trx, parameters, 0, false);
  std::string identifier;

  if (value.isObject() && value.has(TRI_VOC_ATTRIBUTE_ID)) {
    identifier = triagens::basics::JsonHelper::getStringValue(value.get(TRI_VOC_ATTRIBUTE_ID).json(), "");
  }
  else if (value.isString()) {
    identifier = triagens::basics::JsonHelper::getStringValue(value.json(), "");
  }

  if (! identifier.empty()) {
    std::vector<std::string> parts = triagens::basics::StringUtils::split(identifier, "/");
    if (parts.size() == 2) {
      Json result(Json::Object, 2);
      result.set("collection", Json(parts[0]));
      result.set("key", Json(parts[1]));
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, result.steal()));
    }
   
  }

  RegisterWarning(query, "PARSE_IDENTIFIER", TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return AqlValue(new Json(Json::Null));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function Minus
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Minus (triagens::aql::Query* query,
                           triagens::arango::AqlTransaction* trx,
                           FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 2) {
    // The max number is arbitrary
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "MINUS", (int) 2, (int) 99999);
  }

  Json baseArray = ExtractFunctionParameter(trx, parameters, 0, false);
  std::unordered_map<std::string, size_t> contains;
  contains.reserve(n);

  if (! baseArray.isArray()) {
    RegisterWarning(query, "MINUS", TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(new Json(Json::Null));
  }

  // Fill the original map
  for (size_t i = 0; i < baseArray.size(); ++i) {
    contains.emplace(baseArray.at(i).toString(), i);
  }

  // Iterate through all following parameters and delete found elements from the map
  for (size_t k = 1; k < n; ++k) {
    Json nextArray = ExtractFunctionParameter(trx, parameters, k, false);
    if (! nextArray.isArray()) {
      RegisterWarning(query, "MINUS", TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return AqlValue(new Json(Json::Null));
    }

    for (size_t j = 0; j < nextArray.size(); ++j) {
      std::string search = nextArray.at(j).toString();
      auto find = contains.find(search);
      if (find != contains.end()) {
        contains.erase(find);
      }
    }
  }

  // We ommit the normalize part from js, cannot occur here
  Json result(Json::Array, contains.size());
  for (auto& it : contains) {
    result.transfer(baseArray.at(it.second).json());
  }

  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, result.steal()));
}

static void RegisterCollectionInTransaction (triagens::arango::AqlTransaction* trx,
                                             std::string const& collectionName,
                                             TRI_voc_cid_t& cid,
                                             TRI_transaction_collection_t*& collection) {
  TRI_ASSERT(collection == nullptr);
  cid = trx->resolver()->getCollectionId(collectionName);
  if (cid == 0) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                  "'%s'",
                                  collectionName.c_str());
  }
  // ensure the collection is loaded
  collection = trx->trxCollection(cid);

  if (collection == nullptr) {
    int res = TRI_AddCollectionTransaction(trx->getInternals(), 
                                           cid,
                                           TRI_TRANSACTION_READ,
                                           trx->nestingLevel(),
                                           true,
                                           true);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_FORMAT(res, "'%s'", collectionName.c_str());
    }
    TRI_EnsureCollectionsTransaction(trx->getInternals());
    collection = trx->trxCollection(cid);

    if (collection == nullptr) {
      // This case should never occur
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "could not load collection");
    }
  }
  TRI_ASSERT(collection != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Helper function to get a document by it's identifier
///        The collection has to be locked by the transaction before
////////////////////////////////////////////////////////////////////////////////

static Json getDocumentByIdentifier (triagens::arango::AqlTransaction* trx,
                                     CollectionNameResolver const* resolver,
                                     TRI_transaction_collection_t* collection,
                                     TRI_voc_cid_t const& cid,
                                     std::string const& collectionName,
                                     std::string const& identifier) {
  std::vector<std::string> parts = triagens::basics::StringUtils::split(identifier, "/");

  TRI_doc_mptr_copy_t mptr;
  if (parts.size() == 1) {
    int res = trx->readSingle(collection, &mptr, parts[0]);
    if (res != TRI_ERROR_NO_ERROR) {
      return Json(Json::Null);
    }
  }
  else if (parts.size() == 2) {
    if (parts[0] != collectionName) {
      // Reqesting an _id that cannot be stored in this collection
      return Json(Json::Null);
    }
    int res = trx->readSingle(collection, &mptr, parts[1]);
    if (res != TRI_ERROR_NO_ERROR) {
      return Json(Json::Null);
    }
  }
  else {
    return Json(Json::Null);
  }

  return ExpandShapedJson(
    collection->_collection->_collection->getShaper(),
    resolver,
    cid,
    &mptr
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Helper function to get a document by its _id
///        This function will lazy read-lock the collection.
/// this function will not throw if the document or the collection cannot be 
/// found 
////////////////////////////////////////////////////////////////////////////////

static Json getDocumentByIdentifier (triagens::arango::AqlTransaction* trx,
                                     CollectionNameResolver const* resolver,
                                     std::string const& identifier) {
  std::vector<std::string> parts = triagens::basics::StringUtils::split(identifier, "/");

  if (parts.size() != 2) {
    return Json(Json::Null);
  }
  std::string collectionName = parts[0];
  TRI_transaction_collection_t* collection = nullptr;
  TRI_voc_cid_t cid = 0;
  try {
    RegisterCollectionInTransaction(trx, collectionName, cid, collection);
  }
  catch (triagens::basics::Exception const& ex) {
    // don't throw if collection is not found
    if (ex.code() == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
      return Json(Json::Null);
    }
    throw;
  }

  TRI_doc_mptr_copy_t mptr;
  int res = trx->readSingle(collection, &mptr, parts[1]);

  if (res != TRI_ERROR_NO_ERROR) {
    return Json(Json::Null);
  }

  return ExpandShapedJson(
    collection->_collection->_collection->getShaper(),
    resolver,
    cid,
    &mptr
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function Document
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Document (triagens::aql::Query* query,
                              triagens::arango::AqlTransaction* trx,
                              FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 1 || 2 < n) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "DOCUMENT", (int) 1, (int) 2);
  }

  auto resolver = trx->resolver();

   if (n == 1) {
    Json id = ExtractFunctionParameter(trx, parameters, 0, false);

   if (id.isString()) {
      std::string identifier = triagens::basics::JsonHelper::getStringValue(id.json(), "");
      Json result = getDocumentByIdentifier(trx, resolver, identifier);
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, result.steal()));
    }
    else if (id.isArray()) {
      size_t const n = id.size();
      Json result(Json::Array, n);
      for (size_t i = 0; i < n; ++i) {
        Json next = id.at(i);
        try {
          if (next.isString()) {
            std::string identifier = triagens::basics::JsonHelper::getStringValue(next.json(), "");
            Json tmp = getDocumentByIdentifier(trx, resolver, identifier);
            if (! tmp.isNull()) {
              result.add(tmp);
            }
          }
        }
        catch (triagens::basics::Exception const&) {
          // Ignore all ArangoDB exceptions here
        }
      }
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, result.steal()));
    }
    return AqlValue(new Json(Json::Null));
  }

  Json collectionJson = ExtractFunctionParameter(trx, parameters, 0, false);
  if (! collectionJson.isString()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
  std::string collectionName = triagens::basics::JsonHelper::getStringValue(collectionJson.json(), "");

  TRI_transaction_collection_t* collection = nullptr;
  TRI_voc_cid_t cid;
  bool notFound = false;

  try {
    RegisterCollectionInTransaction(trx, collectionName, cid, collection);
  }
  catch (triagens::basics::Exception const& ex) {
    // don't throw if collection is not found
    if (ex.code() != TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
      throw;
    }
    notFound = true;
  }

  Json id = ExtractFunctionParameter(trx, parameters, 1, false);
  if (id.isString()) {
    if (notFound) {
      return AqlValue(new Json(Json::Null));
    }
    std::string identifier = triagens::basics::JsonHelper::getStringValue(id.json(), "");
    Json result = getDocumentByIdentifier(trx, resolver, collection, cid, collectionName, identifier);
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, result.steal()));
  }
  else if (id.isArray()) {
    if (notFound) {
      return AqlValue(new Json(Json::Array));
    }
    size_t const n = id.size();
    Json result(Json::Array, n);
    for (size_t i = 0; i < n; ++i) {
      Json next = id.at(i);
      try {
        if (next.isString()) {
          std::string identifier = triagens::basics::JsonHelper::getStringValue(next.json(), "");
          Json tmp = getDocumentByIdentifier(trx, resolver, collection, cid, collectionName, identifier);
          if (! tmp.isNull()) {
            result.add(tmp);
          }
        }
      }
      catch (triagens::basics::Exception const&) {
        // Ignore all ArangoDB exceptions here
      }
    }
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, result.steal()));
  }
  // Id has invalid format
  return AqlValue(new Json(Json::Null));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function Edges
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Edges (triagens::aql::Query* query,
                           triagens::arango::AqlTransaction* trx,
                           FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 3 || 5 < n) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "EDGES", (int) 3, (int) 5);
  }

  Json collectionJson = ExtractFunctionParameter(trx, parameters, 0, false);
  if (! collectionJson.isString()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
  std::string collectionName = triagens::basics::JsonHelper::getStringValue(collectionJson.json(), "");

  TRI_transaction_collection_t* collection = nullptr;
  TRI_voc_cid_t cid;
  RegisterCollectionInTransaction(trx, collectionName, cid, collection);
  if (collection->_collection->_type != TRI_COL_TYPE_EDGE) {
    RegisterWarning(query, "EDGES", TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
    return AqlValue(new Json(Json::Null));
  }

  Json vertexJson = ExtractFunctionParameter(trx, parameters, 1, false);
  if ( ! vertexJson.isArray() && ! vertexJson.isString() && ! vertexJson.isObject()) {
    // Invalid Start vertex
    // Early Abort before parsing other parameters
    return AqlValue(new Json(Json::Null));
  }

  Json directionJson = ExtractFunctionParameter(trx, parameters, 2, false);
  if (! directionJson.isString()) {
    RegisterWarning(query, "EDGES", TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(new Json(Json::Null));
  }
  std::string dirString = basics::JsonHelper::getStringValue(directionJson.json(), "");
  // transform String to lower case
  std::transform(dirString.begin(), dirString.end(), dirString.begin(), ::tolower);

  TRI_edge_direction_e direction;

  if (dirString == "inbound") {
    direction = TRI_EDGE_IN;
  }
  else if (dirString == "outbound") {
    direction = TRI_EDGE_OUT;
  }
  else if (dirString == "any") {
    direction = TRI_EDGE_ANY;
  }
  else {
    RegisterWarning(query, "EDGES", TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(new Json(Json::Null));
  }
  auto resolver = trx->resolver();

  auto shaper = collection->_collection->_collection->getShaper();
  std::unique_ptr<triagens::arango::ExampleMatcher> matcher;
  if (n > 3) {
    // We might have examples
    Json exampleJson = ExtractFunctionParameter(trx, parameters, 3, false);
    if (! exampleJson.isNull()) {
      bool buildMatcher = false;
      if (exampleJson.isArray()) {
        size_t exampleCount = exampleJson.size();
        // We only support objects here so validate
        for (size_t k = 0; k < exampleCount; /* nothing */) {
          if (exampleJson.at(k).isObject()) {
            ++k;
            continue;
          }

          RegisterWarning(query, "EDGES", TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
          if (TRI_DeleteArrayJson(TRI_UNKNOWN_MEM_ZONE, exampleJson.json(), k)) {
            // don't modify count
            --exampleCount;
          }
          else {
            // Should never occur.
            // If it occurs in production it will simply fall through
            // it can only return more results than expected and not do any harm.
            TRI_ASSERT(false);
            ++k;
          }
        }
        if (exampleCount > 0) {
          buildMatcher = true;
        }
        // else we do not filter at all
      }
      else if (exampleJson.isObject()) {
        buildMatcher = true;
      }
      else {
        RegisterWarning(query, "EDGES", TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      }
      if (buildMatcher) {
        try {
          matcher.reset(new triagens::arango::ExampleMatcher(exampleJson.json(), shaper, resolver));
        }
        catch (triagens::basics::Exception const& e) {
          if (e.code() != TRI_RESULT_ELEMENT_NOT_FOUND) {
            throw;
          }
          // We can never fulfill this filter!
          // RETURN empty Array
          Json result(Json::Array, 0);
          return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, result.steal()));
        }
      }
    }
  }

  bool includeVertices = false;
  if (n == 5) {
    // We have options
    Json options = ExtractFunctionParameter(trx, parameters, 4, false);
    if (options.isObject() && options.has("includeVertices")) {
      includeVertices = triagens::basics::JsonHelper::getBooleanValue(options.json(), "includeVertices", false);
    }
  }

  Json result(Json::Array, 0);
  if (vertexJson.isArray()) {
    size_t length = vertexJson.size();
    for (size_t i = 0; i < length; ++i) {
      try {
        RequestEdges(vertexJson.at(i),
                     trx,
                     resolver,
                     shaper,
                     cid,
                     collection->_collection->_collection,
                     direction,
                     matcher.get(),
                     includeVertices,
                     result);
      }
      catch (...) {
        // Errors in Array are simply ignored
      }
    }
  }
  else {
    RequestEdges(vertexJson,
                 trx,
                 resolver,
                 shaper,
                 cid,
                 collection->_collection->_collection,
                 direction,
                 matcher.get(),
                 includeVertices,
                 result);
  }
  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, result.steal()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function ROUND
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Round (triagens::aql::Query* query,
                           triagens::arango::AqlTransaction* trx,
                           FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "ROUND", (int) 1, (int) 1);
  }

  Json inputJson = ExtractFunctionParameter(trx, parameters, 0, false);

  bool unused = false;
  double input = TRI_ToDoubleJson(inputJson.json(), unused);
  input = floor(input + 0.5); // Rounds down for < x.4999 and up for > x.50000
  return AqlValue(new Json(input));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function ABS
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Abs (triagens::aql::Query* query,
                         triagens::arango::AqlTransaction* trx,
                         FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "ABS", (int) 1, (int) 1);
  }

  Json inputJson = ExtractFunctionParameter(trx, parameters, 0, false);

  bool unused = false;
  double input = TRI_ToDoubleJson(inputJson.json(), unused);
  input = std::abs(input);
  return AqlValue(new Json(input));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function CEIL
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Ceil (triagens::aql::Query* query,
                          triagens::arango::AqlTransaction* trx,
                          FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "CEIL", (int) 1, (int) 1);
  }

  Json inputJson = ExtractFunctionParameter(trx, parameters, 0, false);

  bool unused = false;
  double input = TRI_ToDoubleJson(inputJson.json(), unused);
  input = ceil(input);
  return AqlValue(new Json(input));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function FLOOR
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Floor (triagens::aql::Query* query,
                           triagens::arango::AqlTransaction* trx,
                           FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "FLOOR", (int) 1, (int) 1);
  }

  Json inputJson = ExtractFunctionParameter(trx, parameters, 0, false);

  bool unused = false;
  double input = TRI_ToDoubleJson(inputJson.json(), unused);
  input = floor(input);
  return AqlValue(new Json(input));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function SQRT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Sqrt (triagens::aql::Query* query,
                          triagens::arango::AqlTransaction* trx,
                          FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "SQRT", (int) 1, (int) 1);
  }

  Json inputJson = ExtractFunctionParameter(trx, parameters, 0, false);

  bool unused = false;
  double input = TRI_ToDoubleJson(inputJson.json(), unused);
  input = sqrt(input);
  if (std::isnan(input)) {
    return AqlValue(new Json(Json::Null));
  }
  return AqlValue(new Json(input));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function POW
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Pow (triagens::aql::Query* query,
                         triagens::arango::AqlTransaction* trx,
                         FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n != 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "POW", (int) 2, (int) 2);
  }

  Json baseJson = ExtractFunctionParameter(trx, parameters, 0, false);
  Json expJson = ExtractFunctionParameter(trx, parameters, 1, false);
  
  bool unused = false;
  double base = TRI_ToDoubleJson(baseJson.json(), unused);
  double exp = TRI_ToDoubleJson(expJson.json(), unused);
  base = pow(base, exp);
  if (std::isnan(base) || ! std::isfinite(base)) {
    return AqlValue(new Json(Json::Null));
  }
  return AqlValue(new Json(base));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function RAND
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Rand (triagens::aql::Query* query,
                          triagens::arango::AqlTransaction* trx,
                          FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n != 0) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "RAND", (int) 0, (int) 0);
  }

  // This Random functionality is not too good yet...
  double output = static_cast<double>(std::rand()) / RAND_MAX;
  return AqlValue(new Json(output));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function FIRST_DOCUMENT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::FirstDocument (triagens::aql::Query* query,
                                   triagens::arango::AqlTransaction* trx,
                                   FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  for (size_t i = 0; i < n; ++i) {
    Json element = ExtractFunctionParameter(trx, parameters, i, false);
    if (element.isObject()) {
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, element.copy().steal(), Json::AUTOFREE));
    }
  }
  return AqlValue(new Json(Json::Null));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function FIRST_LIST
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::FirstList (triagens::aql::Query* query,
                               triagens::arango::AqlTransaction* trx,
                               FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  for (size_t i = 0; i < n; ++i) {
    Json element = ExtractFunctionParameter(trx, parameters, i, false);
    if (element.isArray()) {
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, element.copy().steal(), Json::AUTOFREE));
    }
  }
  return AqlValue(new Json(Json::Null));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function PUSH
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Push (triagens::aql::Query* query,
                          triagens::arango::AqlTransaction* trx,
                          FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 2 && n != 3) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "PUSH", (int) 2, (int) 3);
  }
  Json list = ExtractFunctionParameter(trx, parameters, 0, false);
  Json toPush = ExtractFunctionParameter(trx, parameters, 1, false);

  if (list.isNull()) {
    Json array(Json::Array, 1);
    array.add(toPush.copy());
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, array.copy().steal()));
  }
  if (list.isArray()) {
    if (n == 3) {
      Json unique = ExtractFunctionParameter(trx, parameters, 2, false);
      if (ValueToBoolean(unique.json()) &&
          ListContainsElement(list, toPush)) {
        return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, list.copy().steal()));
      }
    }
    list.add(toPush.copy());
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, list.copy().steal()));
  }

  RegisterWarning(query, "PUSH", TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return AqlValue(new Json(Json::Null));
}


////////////////////////////////////////////////////////////////////////////////
/// @brief function POP
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Pop (triagens::aql::Query* query,
                         triagens::arango::AqlTransaction* trx,
                         FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "POP", (int) 1, (int) 1);
  }
  Json list = ExtractFunctionParameter(trx, parameters, 0, false);

  if (list.isNull()) {
    return AqlValue(new Json(Json::Null));
  }
  if (list.isArray()) {
    if (list.size() > 0) {
      if (! TRI_DeleteArrayJson(TRI_UNKNOWN_MEM_ZONE, list.json(), list.size() - 1)) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
    }
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, list.copy().steal()));
  }

  RegisterWarning(query, "POP", TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return AqlValue(new Json(Json::Null));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function APPEND
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Append (triagens::aql::Query* query,
                            triagens::arango::AqlTransaction* trx,
                            FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 2 && n != 3) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "APPEND", (int) 2, (int) 3);
  }
  Json list = ExtractFunctionParameter(trx, parameters, 0, false);
  Json toAppend = ExtractFunctionParameter(trx, parameters, 1, false);

  if (toAppend.isNull()) {
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, list.copy().steal()));
  }
  bool unique = false;
  if (n == 3) {
      Json uniqueJson = ExtractFunctionParameter(trx, parameters, 2, false);
      unique = ValueToBoolean(uniqueJson.json());
  }
  if (list.isNull()) {
    // Create a JSON array instead.
    list = Json(Json::Array, 0);
  }

  if (! toAppend.isArray()) {
    if (unique && ListContainsElement(list, toAppend)) {
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, list.copy().steal()));
    }
    list.reserve(1);
    list.add(toAppend.copy());
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, list.copy().steal()));
  }

  size_t const expectedInserts = toAppend.size();

  list.reserve(expectedInserts);

  for (size_t i = 0; i < expectedInserts; ++i) {
    auto toPush = toAppend.at(i);
    if (unique && ListContainsElement(list, toPush)) {
      continue;
    }
    list.add(toPush.copy());
  }
  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, list.copy().steal()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function UNSHIFT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Unshift (triagens::aql::Query* query,
                             triagens::arango::AqlTransaction* trx,
                             FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 2 && n != 3) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "UNSHIFT", (int) 2, (int) 3);
  }
  Json list = ExtractFunctionParameter(trx, parameters, 0, false);
  Json toAppend = ExtractFunctionParameter(trx, parameters, 1, false);

  if (list.isNull()) {
    Json result(Json::Array, 1);
    result.add(toAppend.copy());
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, result.steal()));
  }

  if (list.isArray()) {
    bool unique = false;
    if (n == 3) {
        Json uniqueJson = ExtractFunctionParameter(trx, parameters, 2, false);
        unique = ValueToBoolean(uniqueJson.json());
    }
    if (unique && ListContainsElement(list, toAppend)) {
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, list.copy().steal()));
    }
    Json copy = list.copy();
    TRI_json_t* toAppendCopy = toAppend.copy().steal();

    int res = TRI_InsertVector(&(copy.json()->_value._objects), toAppendCopy, 0);
    // free the pointer, but not the contents
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, toAppendCopy);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, copy.steal()));
  }

  RegisterInvalidArgumentWarning(query, "UNSHIFT");
  return AqlValue(new Json(Json::Null));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function SHIFT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Shift (triagens::aql::Query* query,
                           triagens::arango::AqlTransaction* trx,
                           FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "SHIFT", (int) 1, (int) 1);
  }

  Json list = ExtractFunctionParameter(trx, parameters, 0, false);
  if (list.isNull()) {
    return AqlValue(new Json(Json::Null));
  }
  if (list.isArray()) {
    if (list.size() == 0) {
      return AqlValue(new Json(Json::Array, 0));
    }
    if (! TRI_DeleteArrayJson(TRI_UNKNOWN_MEM_ZONE, list.json(), 0)) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, list.copy().steal()));
  }

  RegisterInvalidArgumentWarning(query, "SHIFT");
  return AqlValue(new Json(Json::Null));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function REMOVE_VALUE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::RemoveValue (triagens::aql::Query* query,
                                 triagens::arango::AqlTransaction* trx,
                                 FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 2 && n != 3) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "REMOVE_VALUE", (int) 2, (int) 3);
  }

  Json list = ExtractFunctionParameter(trx, parameters, 0, false);

  if (list.isNull()) {
    return AqlValue(new Json(Json::Array, 0));
  }
  if (list.isArray()) {
    bool useLimit = false;
    Json result = list.copy();
    size_t limit = result.size();
    Json toRemove = ExtractFunctionParameter(trx, parameters, 1, false);
    if (n == 3) {
      Json limitJson = ExtractFunctionParameter(trx, parameters, 2, false);
      if (! limitJson.isNull()) {
        bool unused = false;
        limit = static_cast<size_t>(ValueToNumber(limitJson.json(), unused));
        useLimit = true;
      }
    }
    for (size_t i = 0; i < result.size(); /* No increase */) {
      if (useLimit && limit == 0) {
        break;
      }
      if (TRI_CheckSameValueJson(toRemove.json(), result.at(i).json())) {
        if (! TRI_DeleteArrayJson(TRI_UNKNOWN_MEM_ZONE, result.json(), i)) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
        }
        --limit;
      }
      else {
        ++i;
      }
    }
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, result.steal()));
  }

  RegisterInvalidArgumentWarning(query, "REMOVE_VALUE");
  return AqlValue(new Json(Json::Null));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function REMOVE_VALUES
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::RemoveValues (triagens::aql::Query* query,
                                  triagens::arango::AqlTransaction* trx,
                                  FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "REMOVE_VALUES", (int) 2, (int) 2);
  }
  Json list = ExtractFunctionParameter(trx, parameters, 0, false);
  Json values = ExtractFunctionParameter(trx, parameters, 1, false);

  if (values.isNull()) {
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, list.copy().steal()));
  }

  if (list.isNull()) {
    return AqlValue(new Json(Json::Array, 0));
  }

  if (list.isArray() && values.isArray()) {
    Json result = list.copy();
    for (size_t i = 0; i < result.size(); /* No advance */) {
      if (ListContainsElement(values, result.at(i))) {
        if (! TRI_DeleteArrayJson(TRI_UNKNOWN_MEM_ZONE, result.json(), i)) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
        }
      }
      else {
        ++i;
      }
    }
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, result.steal()));
  }

  RegisterInvalidArgumentWarning(query, "REMOVE_VALUES");
  return AqlValue(new Json(Json::Null));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function REMOVE_NTH
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::RemoveNth (triagens::aql::Query* query,
                               triagens::arango::AqlTransaction* trx,
                               FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "REMOVE_NTH", (int) 2, (int) 2);
  }

  Json list = ExtractFunctionParameter(trx, parameters, 0, false);

  if (list.isNull()) {
    return AqlValue(new Json(Json::Array, 0));
  }

  if (list.isArray()) {
    double const count = static_cast<double>(list.size());
    Json position = ExtractFunctionParameter(trx, parameters, 1, false);
    bool unused = false;
    double p = TRI_ToDoubleJson(position.json(), unused);
    if (p >= count || p < - count) {
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, list.copy().steal()));
    }
    if (p < 0) {
      p += count;
    }
    Json result = list.copy();
    if (! TRI_DeleteArrayJson(TRI_UNKNOWN_MEM_ZONE, result.json(), static_cast<size_t>(p))) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, result.steal()));
  }

  RegisterInvalidArgumentWarning(query, "REMOVE_NTH");
  return AqlValue(new Json(Json::Null));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function NOT_NULL
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::NotNull (triagens::aql::Query* query,
                             triagens::arango::AqlTransaction* trx,
                             FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  for (size_t i = 0; i < n; ++i) {
    Json element = ExtractFunctionParameter(trx, parameters, i, false);
    if (! element.isNull()) {
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, element.copy().steal()));
    }
  }
  return AqlValue(new Json(Json::Null));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function CURRENT_DATABASE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::CurrentDatabase (triagens::aql::Query* query,
                                     triagens::arango::AqlTransaction* trx,
                                     FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 0) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "CURRENT_DATABASE", (int) 0, (int) 0);
  }
  return AqlValue(new Json(query->vocbase()->_name));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function COLLECTION_COUNT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::CollectionCount (triagens::aql::Query* query,
                                     triagens::arango::AqlTransaction* trx,
                                     FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "COLLECTION_COUNT", (int) 1, (int) 1);
  }
    
  Json element = ExtractFunctionParameter(trx, parameters, 0, false);
  if (! element.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "COLLECTION_COUNT");
  }
  
  std::string const colName = basics::JsonHelper::getStringValue(element.json(), "");

  auto resolver = trx->resolver();
  TRI_voc_cid_t cid = resolver->getCollectionId(colName);
  if (cid == 0) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND); 
  }

  auto collection = trx->trxCollection(cid);

  // ensure the collection is loaded
  if (collection == nullptr) {
    int res = TRI_AddCollectionTransaction(trx->getInternals(), 
                                           cid,
                                           TRI_TRANSACTION_READ,
                                           trx->nestingLevel(),
                                           true,
                                           true);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_FORMAT(res, "'%s'", colName.c_str());
    }

    TRI_EnsureCollectionsTransaction(trx->getInternals());
    collection = trx->trxCollection(cid);

    if (collection == nullptr) {
      THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                    "'%s'",
                                    colName.c_str());
    }
  }

  auto document = trx->documentCollection(cid);
    
  if (document == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  return AqlValue(new Json(static_cast<double>(document->size())));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function VARIANCE_SAMPLE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::VarianceSample (triagens::aql::Query* query,
                                    triagens::arango::AqlTransaction* trx,
                                    FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "VARIANCE_SAMPLE", (int) 1, (int) 1);
  }

  Json list = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! list.isArray()) {
    RegisterWarning(query, "VARIANCE_SAMPLE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }

  double value = 0.0;
  size_t count = 0;

  if (! Variance(list, value, count)) {
    RegisterWarning(query, "VARIANCE_SAMPLE", TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(new Json(Json::Null));
  }

  if (count < 2) {
    return AqlValue(new Json(Json::Null));
  }

  return AqlValue(new Json(value / (count - 1)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function VARIANCE_POPULATION
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::VariancePopulation (triagens::aql::Query* query,
                                        triagens::arango::AqlTransaction* trx,
                                        FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "VARIANCE_POPULATION", (int) 1, (int) 1);
  }

  Json list = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! list.isArray()) {
    RegisterWarning(query, "VARIANCE_POPULATION", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }

  double value = 0.0;
  size_t count = 0;

  if (! Variance(list, value, count)) {
    RegisterWarning(query, "VARIANCE_POPULATION", TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(new Json(Json::Null));
  }

  if (count < 1) {
    return AqlValue(new Json(Json::Null));
  }

  return AqlValue(new Json(value / count));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function STDDEV_SAMPLE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::StdDevSample (triagens::aql::Query* query,
                                  triagens::arango::AqlTransaction* trx,
                                  FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "STDDEV_SAMPLE", (int) 1, (int) 1);
  }

  Json list = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! list.isArray()) {
    RegisterWarning(query, "STDDEV_SAMPLE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }

  double value = 0.0;
  size_t count = 0;

  if (! Variance(list, value, count)) {
    RegisterWarning(query, "STDDEV_SAMPLE", TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(new Json(Json::Null));
  }

  if (count < 2) {
    return AqlValue(new Json(Json::Null));
  }

  return AqlValue(new Json(sqrt(value / (count - 1))));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function STDDEV_POPULATION
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::StdDevPopulation (triagens::aql::Query* query,
                                      triagens::arango::AqlTransaction* trx,
                                      FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "STDDEV_POPULATION", (int) 1, (int) 1);
  }

  Json list = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! list.isArray()) {
    RegisterWarning(query, "STDDEV_POPULATION", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }

  double value = 0.0;
  size_t count = 0;

  if (! Variance(list, value, count)) {
    RegisterWarning(query, "STDDEV_POPULATION", TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(new Json(Json::Null));
  }

  if (count < 1) {
    return AqlValue(new Json(Json::Null));
  }

  return AqlValue(new Json(sqrt(value / count)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function MEDIAN
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Median (triagens::aql::Query* query,
                            triagens::arango::AqlTransaction* trx,
                            FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 1) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "MEDIAN", (int) 1, (int) 1);
  }

  Json list = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! list.isArray()) {
    RegisterWarning(query, "MEDIAN", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }

  std::vector<double> values;
  if (! SortNumberList(list, values)) {
    RegisterWarning(query, "MEDIAN", TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(new Json(Json::Null));
  }

  if (values.empty()) {
    return AqlValue(new Json(Json::Null));
  }
  size_t const l = values.size();
  size_t midpoint = l / 2;

  if (l % 2 == 0) {
    return AqlValue(new Json((values[midpoint - 1] + values[midpoint]) / 2));
  }
  return AqlValue(new Json(values[midpoint]));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function PERCENTILE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Percentile (triagens::aql::Query* query,
                                triagens::arango::AqlTransaction* trx,
                                FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 2 && n != 3) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "PERCENTILE", (int) 2, (int) 3);
  }

  Json list = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! list.isArray()) {
    RegisterWarning(query, "PERCENTILE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }

  Json border = ExtractFunctionParameter(trx, parameters, 1, false);

  if (! border.isNumber()) {
    RegisterWarning(query, "PERCENTILE", TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(new Json(Json::Null));
  }

  bool unused = false;
  double p = ValueToNumber(border.json(), unused);
  if (p <= 0 || p > 100) {
    RegisterWarning(query, "PERCENTILE", TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(new Json(Json::Null));
  }

  bool useInterpolation = false;

  if (n == 3) {
    Json methodJson = ExtractFunctionParameter(trx, parameters, 2, false);
    if (! methodJson.isString()) {
      RegisterWarning(query, "PERCENTILE", TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return AqlValue(new Json(Json::Null));
    }
    std::string method = triagens::basics::JsonHelper::getStringValue(methodJson.json(), "");
    if (method == "interpolation") {
      useInterpolation = true;
    }
    else if (method == "rank") {
      useInterpolation = false;
    }
    else {
      RegisterWarning(query, "PERCENTILE", TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return AqlValue(new Json(Json::Null));
    }
  }

  std::vector<double> values;
  if (! SortNumberList(list, values)) {
    RegisterWarning(query, "PERCENTILE", TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(new Json(Json::Null));
  }

  if (values.empty()) {
    return AqlValue(new Json(Json::Null));
  }
  size_t l = values.size();
  if (l == 1) {
    return AqlValue(new Json(values[0]));
  }

  TRI_ASSERT(l > 1);

  if (useInterpolation) {
    double idx = p * (l + 1) / 100;
    double pos = floor(idx);

    if (pos >= l) {
      return AqlValue(new Json(values[l - 1]));
    }

    if (pos <= 0) {
      return AqlValue(new Json(Json::Null));
    }

    double delta = idx - pos;
    return AqlValue(new Json(delta * (values[static_cast<size_t>(pos)] - values[static_cast<size_t>(pos) - 1]) + values[static_cast<size_t>(pos) - 1]));
  }
  double idx = p * l / 100;
  double pos = ceil(idx);
  if (pos >= l) {
    return AqlValue(new Json(values[l - 1]));
  }
  if (pos <= 0) {
    return AqlValue(new Json(Json::Null));
  }
  return AqlValue(new Json(values[static_cast<size_t>(pos) - 1]));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function RANGE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Range (triagens::aql::Query* query,
                           triagens::arango::AqlTransaction* trx,
                           FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n != 2 && n != 3) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "RANGE", (int) 2, (int) 3);
  }

  auto const leftJson = ExtractFunctionParameter(trx, parameters, 0, false);
  auto const rightJson = ExtractFunctionParameter(trx, parameters, 1, false);

  bool unused = true;
  double from = ValueToNumber(leftJson.json(), unused);
  double to = ValueToNumber(rightJson.json(), unused);

  double step = 0.0;
  if (n == 3) {
    auto const stepJson = ExtractFunctionParameter(trx, parameters, 2, false);
    if (!TRI_IsNullJson(stepJson.json())) {
      step = ValueToNumber(stepJson.json(), unused);
    }
    else {
      // no step specified
      if (from <= to) {
        step = 1.0;
      } else {
        step = -1.0;
      }
    }
  }
  else { 
    // no step specified
    if (from <= to) {
      step = 1.0;
    }
    else {
      step = -1.0;
    }
  }

  if ( step == 0.0 ||
      (from < to && step < 0.0) ||
      (from > to && step > 0.0)) {
    RegisterWarning(query, "RANGE", TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(new Json(Json::Null));
  }

  Json result(Json::Array);
  if (step < 0.0 && to <= from) {
    for (; from >= to; from += step) {
      result.add(Json(from));
    }
  }
  else {
    for (; from <= to; from += step) {
      result.add(Json(from));
    }
  }
  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, result.steal()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function POSITION
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Position (triagens::aql::Query* query,
                              triagens::arango::AqlTransaction* trx,
                              FunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n != 2 && n != 3) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "POSITION", (int) 2, (int) 3);
  }

  Json list = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! list.isArray()) {
    RegisterWarning(query, "POSITION", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }

  bool returnIndex = false;
  if (n == 3) {
    Json returnIndexJson = ExtractFunctionParameter(trx, parameters, 2, false);
    returnIndex = ValueToBoolean(returnIndexJson.json());
  }

  if (list.size() > 0) {
    Json searchValue = ExtractFunctionParameter(trx, parameters, 1, false);

    size_t index;
    if (ListContainsElement(list, searchValue, index)) {
      if (returnIndex) {
        return AqlValue(new Json(static_cast<double>(index)));
      }
      return AqlValue(new Json(true));
    }
  }

  if (returnIndex) {
    return AqlValue(new Json(-1));
  }
  return AqlValue(new Json(false));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function FULLTEXT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Fulltext (triagens::aql::Query* query,
                              triagens::arango::AqlTransaction* trx,
                              FunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 3 || n > 4) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "FULLTEXT", (int) 3, (int) 4);
  }

  auto resolver = trx->resolver();

  Json collectionJson = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! collectionJson.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FULLTEXT");
  }

  std::string colName = basics::JsonHelper::getStringValue(collectionJson.json(), "");

  Json attribute = ExtractFunctionParameter(trx, parameters, 1, false);
  
  if (! attribute.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FULLTEXT");
  }

  std::string attributeName = basics::JsonHelper::getStringValue(attribute.json(), "");

  Json queryString = ExtractFunctionParameter(trx, parameters, 2, false);
  
  if (! queryString.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FULLTEXT");
  }
  
  std::string queryValue = basics::JsonHelper::getStringValue(queryString.json(), "");

  size_t maxResults = 0; // 0 means "all results"
  if (n >= 4) {
    Json limit = ExtractFunctionParameter(trx, parameters, 3, false);
    if (! limit.isNull() && ! limit.isNumber()) {
      THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FULLTEXT");
    }
    else if (limit.isNumber()) {
      int64_t value = basics::JsonHelper::getNumericValue<int64_t>(limit.json(), 0);
      if (value > 0) {
        maxResults = static_cast<size_t>(value);
      }
    }
  }
  
  TRI_voc_cid_t cid = resolver->getCollectionId(colName);
  auto collection = trx->trxCollection(cid);

  // ensure the collection is loaded
  if (collection == nullptr) {
    int res = TRI_AddCollectionTransaction(trx->getInternals(), 
                                           cid,
                                           TRI_TRANSACTION_READ,
                                           trx->nestingLevel(),
                                           true,
                                           true);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_FORMAT(res, "'%s'", colName.c_str());
    }

    TRI_EnsureCollectionsTransaction(trx->getInternals());
    collection = trx->trxCollection(cid);

    if (collection == nullptr) {
      THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                    "'%s'",
                                    colName.c_str());
    }
  }

  auto document = trx->documentCollection(cid);
    
  if (document == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  triagens::arango::Index* index = nullptr;
 
  std::vector<std::vector<triagens::basics::AttributeName>> const search({ { triagens::basics::AttributeName(attributeName, false) } });

  for (auto const& idx : document->allIndexes()) {
    if (idx->type() == triagens::arango::Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {
      // test if index is on the correct field
      if (triagens::basics::AttributeName::isIdentical(idx->fields(), search, false)) {
        // match!
        index = idx;
        break;
      }
    } 
  }

  if (index == nullptr) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FULLTEXT_INDEX_MISSING, colName.c_str());
  }
  
  if (trx->orderDitch(collection) == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  
  TRI_fulltext_query_t* ft = TRI_CreateQueryFulltextIndex(TRI_FULLTEXT_SEARCH_MAX_WORDS, maxResults);

  if (ft == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  bool isSubstringQuery = false;
  int res = TRI_ParseQueryFulltextIndex(ft, queryValue.c_str(), &isSubstringQuery);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeQueryFulltextIndex(ft);
    THROW_ARANGO_EXCEPTION(res);
  }

  auto fulltextIndex = static_cast<triagens::arango::FulltextIndex*>(index);
  // note: the following call will free "ft"!
  TRI_fulltext_result_t* queryResult = TRI_QueryFulltextIndex(fulltextIndex->internals(), ft);

  if (queryResult == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  auto shaper = collection->_collection->_collection->getShaper();
  size_t const numResults = queryResult->_numDocuments;

  try {
    Json array(Json::Array, numResults);

    for (size_t i = 0; i < numResults; ++i) {
      auto j = ExpandShapedJson(
        shaper,
        resolver,
        cid,
        (TRI_doc_mptr_t const*) queryResult->_documents[i]
      );
      array.add(j);
    }
    TRI_FreeResultFulltextIndex(queryResult);

    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, array.steal()));
  }
  catch (...) {
    TRI_FreeResultFulltextIndex(queryResult);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_SAME_COLLECTION
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsSameCollection (triagens::aql::Query* query,
                                      triagens::arango::AqlTransaction* trx,
                                      FunctionParameters const& parameters) {
  if (parameters.size() != 2) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "IS_SAME_COLLECTION", (int) 2, (int) 2);
  }
  
  Json collectionJson = ExtractFunctionParameter(trx, parameters, 0, false);

  if (! collectionJson.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "IS_SAME_COLLECTION");
  }

  std::string colName = basics::JsonHelper::getStringValue(collectionJson.json(), "");

  Json value = ExtractFunctionParameter(trx, parameters, 1, false);
  std::string identifier;

  if (value.isObject() && value.has(TRI_VOC_ATTRIBUTE_ID)) {
    identifier = triagens::basics::JsonHelper::getStringValue(value.get(TRI_VOC_ATTRIBUTE_ID).json(), "");
  }
  else if (value.isString()) {
    identifier = triagens::basics::JsonHelper::getStringValue(value.json(), "");
  }

  if (! identifier.empty()) {
    size_t pos = identifier.find('/');

    if (pos != std::string::npos) {
      bool const same = (colName == identifier.substr(0, pos));

      return AqlValue(new Json(same));
    }

    // fallthrough intentional
  }

  RegisterWarning(query, "IS_SAME_COLLECTION", TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return AqlValue(new Json(Json::Null));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
