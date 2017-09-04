////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "AqlFunctionFeature.h"
#include "Aql/AstNode.h"
#include "Cluster/ServerState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::aql;

/// @brief determines if code is executed in cluster or not
static ExecutionCondition const NotInCluster =
    [] { return !arangodb::ServerState::instance()->isRunningInCluster(); };

/// @brief determines if code is executed on coordinator or not
static ExecutionCondition const NotInCoordinator = [] {
  return !arangodb::ServerState::instance()->isRunningInCluster() ||
         !arangodb::ServerState::instance()->isCoordinator();
};

AqlFunctionFeature* AqlFunctionFeature::AQLFUNCTIONS = nullptr;

AqlFunctionFeature::AqlFunctionFeature(
    application_features::ApplicationServer* server)
    : application_features::ApplicationFeature(server, "AQLFunctions"),
      _internalFunctionNames{
          {static_cast<int>(NODE_TYPE_OPERATOR_UNARY_PLUS), "UNARY_PLUS"},
          {static_cast<int>(NODE_TYPE_OPERATOR_UNARY_MINUS), "UNARY_MINUS"},
          {static_cast<int>(NODE_TYPE_OPERATOR_UNARY_NOT), "LOGICAL_NOT"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_EQ), "RELATIONAL_EQUAL"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_NE),
           "RELATIONAL_UNEQUAL"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_GT),
           "RELATIONAL_GREATER"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_GE),
           "RELATIONAL_GREATEREQUAL"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_LT), "RELATIONAL_LESS"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_LE),
           "RELATIONAL_LESSEQUAL"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_IN), "RELATIONAL_IN"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_NIN),
           "RELATIONAL_NOT_IN"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_PLUS), "ARITHMETIC_PLUS"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_MINUS),
           "ARITHMETIC_MINUS"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_TIMES),
           "ARITHMETIC_TIMES"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_DIV),
           "ARITHMETIC_DIVIDE"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_MOD),
           "ARITHMETIC_MODULUS"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_AND), "LOGICAL_AND"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_OR), "LOGICAL_OR"},
          {static_cast<int>(NODE_TYPE_OPERATOR_TERNARY), "TERNARY_OPERATOR"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ),
           "RELATIONAL_ARRAY_EQUAL"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_ARRAY_NE),
           "RELATIONAL_ARRAY_UNEQUAL"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_ARRAY_GT),
           "RELATIONAL_ARRAY_GREATER"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_ARRAY_GE),
           "RELATIONAL_ARRAY_GREATEREQUAL"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_ARRAY_LT),
           "RELATIONAL_ARRAY_LESS"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_ARRAY_LE),
           "RELATIONAL_ARRAY_LESSEQUAL"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_ARRAY_IN),
           "RELATIONAL_ARRAY_IN"},
          {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN),
           "RELATIONAL_ARRAY_NOT_IN"}} {
  setOptional(false);
  startsAfter("EngineSelector");
  startsAfter("Aql");
}

// This feature does not have any options
void AqlFunctionFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions>) {}

// This feature does not have any options
void AqlFunctionFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions>) {}

void AqlFunctionFeature::prepare() {
  // set singleton
  AQLFUNCTIONS = this;

  /// @brief Add all AQL functions to the FunctionDefintions map
  addTypeCheckFunctions();
  addTypeCastFunctions();
  addStringFunctions();
  addNumericFunctions();
  addListFunctions();
  addDocumentFunctions();
  addGeoFunctions();
  addDateFunctions();
  addMiscFunctions();
  addStorageEngineFunctions();
  
  add({"PREGEL_RESULT", "AQL_PREGEL_RESULT", ".", true, false, true,
    true, true, &Functions::PregelResult, NotInCoordinator});
}

void AqlFunctionFeature::unprepare() {
  // Just unlink nothing more todo
  AQLFUNCTIONS = nullptr;
}

void AqlFunctionFeature::add(Function const& func) {
  TRI_ASSERT(_functionNames.find(func.externalName) == _functionNames.end());
  // add function to the map
  _functionNames.emplace(func.externalName, func);
}

void AqlFunctionFeature::toVelocyPack(VPackBuilder& builder) {
  builder.openArray();
  for (auto const& it : _functionNames) {
    builder.openObject();
    builder.add("name", VPackValue(it.second.externalName));
    builder.add("arguments", VPackValue(it.second.arguments));
    builder.add("implementations", VPackValue(VPackValueType::Array));
    builder.add(VPackValue("js"));

    if (it.second.implementation != nullptr) {
      builder.add(VPackValue("cxx"));
    }
    builder.close(); // implementations
    builder.add("deterministic", VPackValue(it.second.isDeterministic));
    builder.add("cacheable", VPackValue(it.second.isCacheable));
    builder.add("canThrow", VPackValue(it.second.canThrow));
    builder.close();
  }
  builder.close();
}

Function const* AqlFunctionFeature::byName(std::string const& name) {
  auto it = _functionNames.find(name);

  if (it == _functionNames.end()) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_NAME_UNKNOWN,
                                  name.c_str());
  }

  // return the address of the function
  return &((*it).second);
}

std::string const& AqlFunctionFeature::getOperatorName(
    AstNodeType const type, std::string const& errorMessage) {
  auto it = _internalFunctionNames.find(static_cast<int>(type));

  if (it == _internalFunctionNames.end()) {
    // no function found for the type of node
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMessage);
  }
  return it->second;
}

// meanings of the symbols in the function arguments list
// ------------------------------------------------------
//
// . = argument of any type (except collection)
// c = collection name, will be converted into list with documents
// h = collection name, will be converted into string
// , = next argument
// | = separates mandatory from optional arguments
// + = unlimited number of optional arguments of any type

void AqlFunctionFeature::addTypeCheckFunctions() {
  // type check functions
  add({"IS_NULL", "AQL_IS_NULL", ".", true, true, false, true, true,
       &Functions::IsNull});
  add({"IS_BOOL", "AQL_IS_BOOL", ".", true, true, false, true, true,
       &Functions::IsBool});
  add({"IS_NUMBER", "AQL_IS_NUMBER", ".", true, true, false, true, true,
       &Functions::IsNumber});
  add({"IS_STRING", "AQL_IS_STRING", ".", true, true, false, true, true,
       &Functions::IsString});
  add({"IS_ARRAY", "AQL_IS_ARRAY", ".", true, true, false, true, true,
       &Functions::IsArray});
  // IS_LIST is an alias for IS_ARRAY
  add({"IS_LIST", "AQL_IS_LIST", ".", true, true, false, true, true,
       &Functions::IsArray});
  add({"IS_OBJECT", "AQL_IS_OBJECT", ".", true, true, false, true, true,
       &Functions::IsObject});
  // IS_DOCUMENT is an alias for IS_OBJECT
  add({"IS_DOCUMENT", "AQL_IS_DOCUMENT", ".", true, true, false, true, true,
       &Functions::IsObject});

  add({"IS_DATESTRING", "AQL_IS_DATESTRING", ".", true, true, false, true,
       true});
  add({"TYPENAME", "AQL_TYPENAME", ".", true, true, false, true, true,
       &Functions::Typename});
}

void AqlFunctionFeature::addTypeCastFunctions() {
  // type cast functions
  add({"TO_NUMBER", "AQL_TO_NUMBER", ".", true, true, false, true, true,
       &Functions::ToNumber});
  add({"TO_STRING", "AQL_TO_STRING", ".", true, true, false, true, true,
       &Functions::ToString});
  add({"TO_BOOL", "AQL_TO_BOOL", ".", true, true, false, true, true,
       &Functions::ToBool});
  add({"TO_ARRAY", "AQL_TO_ARRAY", ".", true, true, false, true, true,
       &Functions::ToArray});
  // TO_LIST is an alias for TO_ARRAY
  add({"TO_LIST", "AQL_TO_LIST", ".", true, true, false, true, true,
       &Functions::ToArray});
}

void AqlFunctionFeature::addStringFunctions() {
  // string functions
  add({"CONCAT", "AQL_CONCAT", ".|+", true, true, false, true, true,
       &Functions::Concat});
  add({"CONCAT_SEPARATOR", "AQL_CONCAT_SEPARATOR", ".,.|+", true, true, false,
       true, true, &Functions::ConcatSeparator});
  add({"CHAR_LENGTH", "AQL_CHAR_LENGTH", ".", true, true, false, true, true,
       &Functions::CharLength});
  add({"LOWER", "AQL_LOWER", ".", true, true, false, true, true, &Functions::Lower});
  add({"UPPER", "AQL_UPPER", ".", true, true, false, true, true, &Functions::Upper});
  add({"SUBSTRING", "AQL_SUBSTRING", ".,.|.", true, true, false, true, true});
  add({"CONTAINS", "AQL_CONTAINS", ".,.|.", true, true, false, true, true,
       &Functions::Contains});
  add({"LIKE", "AQL_LIKE", ".,.|.", true, true, false, true, true,
       &Functions::Like});
  add({"REGEX_TEST", "AQL_REGEX_TEST", ".,.|.", true, true, false, true, true,
       &Functions::RegexTest});
  add({"REGEX_REPLACE", "AQL_REGEX_REPLACE", ".,.,.|.", true, true, false, true,
       true, &Functions::RegexReplace});
  add({"LEFT", "AQL_LEFT", ".,.", true, true, false, true, true});
  add({"RIGHT", "AQL_RIGHT", ".,.", true, true, false, true, true});
  add({"TRIM", "AQL_TRIM", ".|.", true, true, false, true, true});
  add({"LTRIM", "AQL_LTRIM", ".|.", true, true, false, true, true});
  add({"RTRIM", "AQL_RTRIM", ".|.", true, true, false, true, true});
  add({"FIND_FIRST", "AQL_FIND_FIRST", ".,.|.,.", true, true, false, true,
       true});
  add({"FIND_LAST", "AQL_FIND_LAST", ".,.|.,.", true, true, false, true,
       true});
  add({"SPLIT", "AQL_SPLIT", ".|.,.", true, true, false, true, true});
  add({"SUBSTITUTE", "AQL_SUBSTITUTE", ".,.|.,.", true, true, false, true,
       true});
  add({"MD5", "AQL_MD5", ".", true, true, false, true, true, &Functions::Md5});
  add({"SHA1", "AQL_SHA1", ".", true, true, false, true, true,
       &Functions::Sha1});
  add({"HASH", "AQL_HASH", ".", true, true, false, true, true,
       &Functions::Hash});
  add({"RANDOM_TOKEN", "AQL_RANDOM_TOKEN", ".", false, false, true, true, true,
       &Functions::RandomToken});
}

void AqlFunctionFeature::addNumericFunctions() {
  // numeric functions
  add({"FLOOR", "AQL_FLOOR", ".", true, true, false, true, true,
       &Functions::Floor});
  add({"CEIL", "AQL_CEIL", ".", true, true, false, true, true,
       &Functions::Ceil});
  add({"ROUND", "AQL_ROUND", ".", true, true, false, true, true,
       &Functions::Round});
  add({"ABS", "AQL_ABS", ".", true, true, false, true, true, &Functions::Abs});
  add({"RAND", "AQL_RAND", "", false, false, false, true, true,
       &Functions::Rand});
  add({"SQRT", "AQL_SQRT", ".", true, true, false, true, true,
       &Functions::Sqrt});
  add({"POW", "AQL_POW", ".,.", true, true, false, true, true,
       &Functions::Pow});
  add({"LOG", "AQL_LOG", ".", true, true, false, true, true, &Functions::Log});
  add({"LOG2", "AQL_LOG2", ".", true, true, false, true, true,
       &Functions::Log2});
  add({"LOG10", "AQL_LOG10", ".", true, true, false, true, true,
       &Functions::Log10});
  add({"EXP", "AQL_EXP", ".", true, true, false, true, true, &Functions::Exp});
  add({"EXP2", "AQL_EXP2", ".", true, true, false, true, true,
       &Functions::Exp2});
  add({"SIN", "AQL_SIN", ".", true, true, false, true, true, &Functions::Sin});
  add({"COS", "AQL_COS", ".", true, true, false, true, true, &Functions::Cos});
  add({"TAN", "AQL_TAN", ".", true, true, false, true, true, &Functions::Tan});
  add({"ASIN", "AQL_ASIN", ".", true, true, false, true, true,
       &Functions::Asin});
  add({"ACOS", "AQL_ACOS", ".", true, true, false, true, true,
       &Functions::Acos});
  add({"ATAN", "AQL_ATAN", ".", true, true, false, true, true,
       &Functions::Atan});
  add({"ATAN2", "AQL_ATAN2", ".,.", true, true, false, true, true,
       &Functions::Atan2});
  add({"RADIANS", "AQL_RADIANS", ".", true, true, false, true, true,
       &Functions::Radians});
  add({"DEGREES", "AQL_DEGREES", ".", true, true, false, true, true,
       &Functions::Degrees});
  add({"PI", "AQL_PI", "", true, true, false, true, true, &Functions::Pi});
}

void AqlFunctionFeature::addListFunctions() {
  // list functions
  add({"RANGE", "AQL_RANGE", ".,.|.", true, true, false, true, true,
       &Functions::Range});
  add({"UNION", "AQL_UNION", ".,.|+", true, true, false, true, true,
       &Functions::Union});
  add({"UNION_DISTINCT", "AQL_UNION_DISTINCT", ".,.|+", true, true, false, true,
       true, &Functions::UnionDistinct});
  add({"MINUS", "AQL_MINUS", ".,.|+", true, true, false, true, true,
       &Functions::Minus});
  add({"OUTERSECTION", "AQL_OUTERSECTION", ".,.|+", true, true, false, true,
       true, &Functions::Outersection});
  add({"INTERSECTION", "AQL_INTERSECTION", ".,.|+", true, true, false, true,
       true, &Functions::Intersection});
  add({"FLATTEN", "AQL_FLATTEN", ".|.", true, true, false, true, true,
       &Functions::Flatten});
  add({"LENGTH", "AQL_LENGTH", ".", true, true, false, true, true,
       &Functions::Length});
  add({"COUNT", "AQL_LENGTH", ".", true, true, false, true, true,
       &Functions::Length});  // alias for LENGTH()
  add({"MIN", "AQL_MIN", ".", true, true, false, true, true, &Functions::Min});
  add({"MAX", "AQL_MAX", ".", true, true, false, true, true, &Functions::Max});
  add({"SUM", "AQL_SUM", ".", true, true, false, true, true, &Functions::Sum});
  add({"MEDIAN", "AQL_MEDIAN", ".", true, true, false, true, true,
       &Functions::Median});
  add({"PERCENTILE", "AQL_PERCENTILE", ".,.|.", true, true, false, true, true,
       &Functions::Percentile});
  add({"AVERAGE", "AQL_AVERAGE", ".", true, true, false, true, true,
       &Functions::Average});
  add({"AVG", "AQL_AVERAGE", ".", true, true, false, true, true,
       &Functions::Average});  // alias for AVERAGE()
  add({"VARIANCE_SAMPLE", "AQL_VARIANCE_SAMPLE", ".", true, true, false, true,
       true, &Functions::VarianceSample});
  add({"VARIANCE_POPULATION", "AQL_VARIANCE_POPULATION", ".", true, true, false,
       true, true, &Functions::VariancePopulation});
  add({"VARIANCE", "AQL_VARIANCE_POPULATION", ".", true, true, false, true,
       true,
       &Functions::VariancePopulation});  // alias for VARIANCE_POPULATION()
  add({"STDDEV_SAMPLE", "AQL_STDDEV_SAMPLE", ".", true, true, false, true, true,
       &Functions::StdDevSample});
  add({"STDDEV_POPULATION", "AQL_STDDEV_POPULATION", ".", true, true, false,
       true, true, &Functions::StdDevPopulation});
  add({"STDDEV", "AQL_STDDEV_POPULATION", ".", true, true, false, true, true,
       &Functions::StdDevPopulation});  // alias for STDDEV_POPULATION()
  add({"UNIQUE", "AQL_UNIQUE", ".", true, true, false, true, true,
       &Functions::Unique});
  add({"SORTED_UNIQUE", "AQL_SORTED_UNIQUE", ".", true, true, false, true, true,
       &Functions::SortedUnique});
  add({"SLICE", "AQL_SLICE", ".,.|.", true, true, false, true, true,
       &Functions::Slice});
  add({"REVERSE", "AQL_REVERSE", ".", true, true, false, true,
       true});  // note: REVERSE() can be applied on strings, too
  add({"FIRST", "AQL_FIRST", ".", true, true, false, true, true,
       &Functions::First});
  add({"LAST", "AQL_LAST", ".", true, true, false, true, true,
       &Functions::Last});
  add({"NTH", "AQL_NTH", ".,.", true, true, false, true, true,
       &Functions::Nth});
  add({"POSITION", "AQL_POSITION", ".,.|.", true, true, false, true, true,
       &Functions::Position});
  add({"CALL", "AQL_CALL", ".|.+", false, false, true, false, true});
  add({"APPLY", "AQL_APPLY", ".|.", false, false, true, false, false});
  add({"PUSH", "AQL_PUSH", ".,.|.", true, true, false, true, false,
       &Functions::Push});
  add({"APPEND", "AQL_APPEND", ".,.|.", true, true, false, true, true,
       &Functions::Append});
  add({"POP", "AQL_POP", ".", true, true, false, true, true, &Functions::Pop});
  add({"SHIFT", "AQL_SHIFT", ".", true, true, false, true, true,
       &Functions::Shift});
  add({"UNSHIFT", "AQL_UNSHIFT", ".,.|.", true, true, false, true, true,
       &Functions::Unshift});
  add({"REMOVE_VALUE", "AQL_REMOVE_VALUE", ".,.|.", true, true, false, true,
       true, &Functions::RemoveValue});
  add({"REMOVE_VALUES", "AQL_REMOVE_VALUES", ".,.", true, true, false, true,
       true, &Functions::RemoveValues});
  add({"REMOVE_NTH", "AQL_REMOVE_NTH", ".,.", true, true, false, true, true,
       &Functions::RemoveNth});
}

void AqlFunctionFeature::addDocumentFunctions() {
  // document functions
  add({"HAS", "AQL_HAS", ".,.", true, true, false, true, true,
       &Functions::Has});
  add({"ATTRIBUTES", "AQL_ATTRIBUTES", ".|.,.", true, true, false, true, true,
       &Functions::Attributes});
  add({"VALUES", "AQL_VALUES", ".|.", true, true, false, true, true,
       &Functions::Values});
  add({"MERGE", "AQL_MERGE", ".|+", true, true, false, true, true,
       &Functions::Merge});
  add({"MERGE_RECURSIVE", "AQL_MERGE_RECURSIVE", ".,.|+", true, true, false,
       true, true, &Functions::MergeRecursive});
  add({"DOCUMENT", "AQL_DOCUMENT", "h.|.", false, false, true, false, true, &Functions::Document});
  add({"MATCHES", "AQL_MATCHES", ".,.|.", true, true, false, true, true});
  add({"UNSET", "AQL_UNSET", ".,.|+", true, true, false, true, true,
       &Functions::Unset});
  add({"UNSET_RECURSIVE", "AQL_UNSET_RECURSIVE", ".,.|+", true, true, false,
       true, true, &Functions::UnsetRecursive});
  add({"KEEP", "AQL_KEEP", ".,.|+", true, true, false, true, true,
       &Functions::Keep});
  add({"TRANSLATE", "AQL_TRANSLATE", ".,.|.", true, true, false, true, true});
  add({"ZIP", "AQL_ZIP", ".,.", true, true, false, true, true,
       &Functions::Zip});
  add({"JSON_STRINGIFY", "AQL_JSON_STRINGIFY", ".", true, true, false, true,
       true, &Functions::JsonStringify});
  add({"JSON_PARSE", "AQL_JSON_PARSE", ".", true, true, false, true, true,
       &Functions::JsonParse});
}

void AqlFunctionFeature::addGeoFunctions() {
  // geo functions
  add({"DISTANCE", "AQL_DISTANCE", ".,.,.,.", true, true, false, true, true,
       &Functions::Distance});
  add({"WITHIN_RECTANGLE", "AQL_WITHIN_RECTANGLE", "h.,.,.,.,.", true, false,
       true, false, true});
  add({"IS_IN_POLYGON", "AQL_IS_IN_POLYGON", ".,.|.", true, true, false, true,
       true});
}

void AqlFunctionFeature::addDateFunctions() {
  // date functions
  add({"DATE_NOW", "AQL_DATE_NOW", "", false, false, false, true, true});
  add({"DATE_TIMESTAMP", "AQL_DATE_TIMESTAMP", ".|.,.,.,.,.,.", true,
       true, false, true, true});
  add({"DATE_ISO8601", "AQL_DATE_ISO8601", ".|.,.,.,.,.,.", true, true,
       false, true, true});
  add({"DATE_DAYOFWEEK", "AQL_DATE_DAYOFWEEK", ".", true, true, false, true,
       true});
  add({"DATE_YEAR", "AQL_DATE_YEAR", ".", true, true, false, true, true});
  add({"DATE_MONTH", "AQL_DATE_MONTH", ".", true, true, false, true, true});
  add({"DATE_DAY", "AQL_DATE_DAY", ".", true, true, false, true, true});
  add({"DATE_HOUR", "AQL_DATE_HOUR", ".", true, true, false, true, true});
  add({"DATE_MINUTE", "AQL_DATE_MINUTE", ".", true, true, false, true, true});
  add({"DATE_SECOND", "AQL_DATE_SECOND", ".", true, true, false, true, true});
  add({"DATE_MILLISECOND", "AQL_DATE_MILLISECOND", ".", true, true, false,
       true, true});
  add({"DATE_DAYOFYEAR", "AQL_DATE_DAYOFYEAR", ".", true, true, false, true,
       true});
  add({"DATE_ISOWEEK", "AQL_DATE_ISOWEEK", ".", true, true, false, true,
       true});
  add({"DATE_LEAPYEAR", "AQL_DATE_LEAPYEAR", ".", true, true, false, true,
       true});
  add({"DATE_QUARTER", "AQL_DATE_QUARTER", ".", true, true, false, true,
       true});
  add({"DATE_DAYS_IN_MONTH", "AQL_DATE_DAYS_IN_MONTH", ".", true, true, false,
       true, true});
  add({"DATE_ADD", "AQL_DATE_ADD", ".,.|.", true, true, false, true, true});
  add({"DATE_SUBTRACT", "AQL_DATE_SUBTRACT", ".,.|.", true, true, false, true,
       true});
  add({"DATE_DIFF", "AQL_DATE_DIFF", ".,.,.|.", true, true, false, true,
       true});
  add({"DATE_COMPARE", "AQL_DATE_COMPARE", ".,.,.|.", true, true, false, true,
       true});
  add({"DATE_FORMAT", "AQL_DATE_FORMAT", ".,.", true, true, false, true,
       true});
}

void AqlFunctionFeature::addMiscFunctions() {
  // misc functions
  add({"FAIL", "AQL_FAIL", "|.", false, false, true, true, true});
  add({"PASSTHRU", "AQL_PASSTHRU", ".", false, false, false, true, true,
       &Functions::Passthru});
  add({"NOOPT", "AQL_PASSTHRU", ".", false, false, false, true, true,
       &Functions::Passthru});
  add({"V8", "AQL_PASSTHRU", ".", false, true, false, true, true});
  add({"TEST_INTERNAL", "AQL_TEST_INTERNAL", ".,.", false, false, false, true,
       false});
  add({"SLEEP", "AQL_SLEEP", ".", false, false, true, true, true, &Functions::Sleep});
  add({"COLLECTIONS", "AQL_COLLECTIONS", "", false, false, true, false, true});
  add({"NOT_NULL", "AQL_NOT_NULL", ".|+", true, true, false, true, true,
       &Functions::NotNull});
  add({"FIRST_LIST", "AQL_FIRST_LIST", ".|+", true, true, false, true, true,
       &Functions::FirstList});
  add({"FIRST_DOCUMENT", "AQL_FIRST_DOCUMENT", ".|+", true, true, false, true,
       true, &Functions::FirstDocument});
  add({"PARSE_IDENTIFIER", "AQL_PARSE_IDENTIFIER", ".", true, true, false, true,
       true, &Functions::ParseIdentifier});
  add({"IS_SAME_COLLECTION", "AQL_IS_SAME_COLLECTION", ".h,.h", true, true,
       false, true, true, &Functions::IsSameCollection});
  add({"CURRENT_USER", "AQL_CURRENT_USER", "", false, false, false, false,
       true});
  add({"CURRENT_DATABASE", "AQL_CURRENT_DATABASE", "", false, false, false,
       false, true, &Functions::CurrentDatabase});
  add({"COLLECTION_COUNT", "AQL_COLLECTION_COUNT", ".h", false, false, true,
       false, true, &Functions::CollectionCount, NotInCluster});
}

void AqlFunctionFeature::addStorageEngineFunctions() {
  StorageEngine* engine = EngineSelectorFeature::ENGINE; 
  TRI_ASSERT(engine != nullptr); // Engine not loaded. Startup broken
  engine->addAqlFunctions();
}
