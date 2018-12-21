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
  
  add({"PREGEL_RESULT", ".", false, true,
    true, true, &Functions::PregelResult, NotInCoordinator});
}

void AqlFunctionFeature::unprepare() {
  // Just unlink nothing more todo
  AQLFUNCTIONS = nullptr;
}
  
/// @brief returns a reference to a built-in function
Function const* AqlFunctionFeature::getFunctionByName(std::string const& name) {
  TRI_ASSERT(AQLFUNCTIONS != nullptr);
  return AQLFUNCTIONS->byName(name);
}

void AqlFunctionFeature::add(Function const& func) {
  TRI_ASSERT(_functionNames.find(func.name) == _functionNames.end());
  // add function to the map
  _functionNames.emplace(func.name, func);
}

void AqlFunctionFeature::addAlias(std::string const& alias, std::string const& original) {
  auto it = _functionNames.find(original);
  TRI_ASSERT(it != _functionNames.end());

  // intentionally copy original function, as we want to give it another name
  Function aliasFunction = (*it).second; 
  aliasFunction.name = alias;

  add(aliasFunction);
}

void AqlFunctionFeature::toVelocyPack(VPackBuilder& builder) {
  builder.openArray();
  for (auto const& it : _functionNames) {
    builder.openObject();
    builder.add("name", VPackValue(it.second.name));
    builder.add("arguments", VPackValue(it.second.arguments));
    builder.add("implementations", VPackValue(VPackValueType::Array));
    builder.add(VPackValue("js"));

    if (it.second.implementation != nullptr) {
      builder.add(VPackValue("cxx"));
    }
    builder.close(); // implementations
    builder.add("deterministic", VPackValue(it.second.isDeterministic));
    builder.add("cacheable", VPackValue(it.second.isCacheable()));
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
  add({"IS_NULL", ".", true, false, true, true,
       &Functions::IsNull});
  add({"IS_BOOL", ".", true, false, true, true,
       &Functions::IsBool});
  add({"IS_NUMBER", ".", true, false, true, true,
       &Functions::IsNumber});
  add({"IS_STRING", ".", true, false, true, true,
       &Functions::IsString});
  add({"IS_ARRAY", ".", true, false, true, true,
       &Functions::IsArray});
  // IS_LIST is an alias for IS_ARRAY
  addAlias("IS_LIST", "IS_ARRAY");
  add({"IS_OBJECT", ".", true, false, true, true,
       &Functions::IsObject});
  // IS_DOCUMENT is an alias for IS_OBJECT
  addAlias("IS_DOCUMENT", "IS_OBJECT");

  add({"IS_DATESTRING", ".", true, false, true,
       true});
  add({"TYPENAME", ".", true, false, true, true,
       &Functions::Typename});
}

void AqlFunctionFeature::addTypeCastFunctions() {
  // type cast functions
  add({"TO_NUMBER", ".", true, false, true, true, &Functions::ToNumber});
  add({"TO_STRING", ".", true, false, true, true, &Functions::ToString});
  add({"TO_BOOL", ".", true, false, true, true, &Functions::ToBool});
  add({"TO_ARRAY", ".", true, false, true, true, &Functions::ToArray});
  // TO_LIST is an alias for TO_ARRAY
  addAlias("TO_LIST", "TO_ARRAY");
}

void AqlFunctionFeature::addStringFunctions() {
  // string functions
  add({"CONCAT", ".|+", true, false, true, true, &Functions::Concat});
  add({"CONCAT_SEPARATOR", ".,.|+", true, false,
       true, true, &Functions::ConcatSeparator});
  add({"CHAR_LENGTH", ".", true, false, true, true, &Functions::CharLength});
  add({"LOWER", ".", true, false, true, true, &Functions::Lower});
  add({"UPPER", ".", true, false, true, true, &Functions::Upper});
  add({"SUBSTRING", ".,.|.", true, false, true, true, &Functions::Substring});
  add({"CONTAINS", ".,.|.", true, false, true, true, &Functions::Contains});
  add({"LIKE", ".,.|.", true, false, true, true, &Functions::Like});
  add({"REGEX_TEST", ".,.|.", true, false, true, true, &Functions::RegexTest});
  add({"REGEX_REPLACE", ".,.,.|.", true, false, true,
       true, &Functions::RegexReplace});
  add({"LEFT", ".,.", true, false, true, true, &Functions::Left});
  add({"RIGHT", ".,.", true, false, true, true, &Functions::Right});
  add({"TRIM", ".|.", true, false, true, true, &Functions::Trim});
  add({"LTRIM", ".|.", true, false, true, true});
  add({"RTRIM", ".|.", true, false, true, true});
  add({"FIND_FIRST", ".,.|.,.", true, false, true, true});
  add({"FIND_LAST", ".,.|.,.", true, false, true, true});
  add({"SPLIT", ".|.,.", true, false, true, true});
  add({"SUBSTITUTE", ".,.|.,.", true, false, true, true});
  add({"MD5", ".", true, false, true, true, &Functions::Md5});
  add({"SHA1", ".", true, false, true, true, &Functions::Sha1});
  add({"SHA512", ".", true, false, true, true});
  add({"HASH", ".", true, false, true, true, &Functions::Hash});
  add({"RANDOM_TOKEN", ".", false, true, true, true, &Functions::RandomToken});
}

void AqlFunctionFeature::addNumericFunctions() {
  // numeric functions
  add({"FLOOR", ".", true, false, true, true,
       &Functions::Floor});
  add({"CEIL", ".", true, false, true, true,
       &Functions::Ceil});
  add({"ROUND", ".", true, false, true, true,
       &Functions::Round});
  add({"ABS", ".", true, false, true, true, &Functions::Abs});
  add({"RAND", "", false, false, true, true,
       &Functions::Rand});
  add({"SQRT", ".", true, false, true, true,
       &Functions::Sqrt});
  add({"POW", ".,.", true, false, true, true,
       &Functions::Pow});
  add({"LOG", ".", true, false, true, true, &Functions::Log});
  add({"LOG2", ".", true, false, true, true,
       &Functions::Log2});
  add({"LOG10", ".", true, false, true, true,
       &Functions::Log10});
  add({"EXP", ".", true, false, true, true, &Functions::Exp});
  add({"EXP2", ".", true, false, true, true,
       &Functions::Exp2});
  add({"SIN", ".", true, false, true, true, &Functions::Sin});
  add({"COS", ".", true, false, true, true, &Functions::Cos});
  add({"TAN", ".", true, false, true, true, &Functions::Tan});
  add({"ASIN", ".", true, false, true, true,
       &Functions::Asin});
  add({"ACOS", ".", true, false, true, true,
       &Functions::Acos});
  add({"ATAN", ".", true, false, true, true,
       &Functions::Atan});
  add({"ATAN2", ".,.", true, false, true, true,
       &Functions::Atan2});
  add({"RADIANS", ".", true, false, true, true,
       &Functions::Radians});
  add({"DEGREES", ".", true, false, true, true,
       &Functions::Degrees});
  add({"PI", "", true, false, true, true, &Functions::Pi});
}

void AqlFunctionFeature::addListFunctions() {
  // list functions
  add({"RANGE", ".,.|.", true, false, true, true,
       &Functions::Range});
  add({"UNION", ".,.|+", true, false, true, true,
       &Functions::Union});
  add({"UNION_DISTINCT", ".,.|+", true, false, true,
       true, &Functions::UnionDistinct});
  add({"MINUS", ".,.|+", true, false, true, true,
       &Functions::Minus});
  add({"OUTERSECTION", ".,.|+", true, false, true,
       true, &Functions::Outersection});
  add({"INTERSECTION", ".,.|+", true, false, true,
       true, &Functions::Intersection});
  add({"FLATTEN", ".|.", true, false, true, true,
       &Functions::Flatten});
  add({"LENGTH", ".", true, false, true, true,
       &Functions::Length});
  addAlias("COUNT", "LENGTH");
  add({"MIN", ".", true, false, true, true, &Functions::Min});
  add({"MAX", ".", true, false, true, true, &Functions::Max});
  add({"SUM", ".", true, false, true, true, &Functions::Sum});
  add({"MEDIAN", ".", true, false, true, true,
       &Functions::Median});
  add({"PERCENTILE", ".,.|.", true, false, true, true,
       &Functions::Percentile});
  add({"AVERAGE", ".", true, false, true, true,
       &Functions::Average});
  addAlias("AVG", "AVERAGE");
  add({"VARIANCE_SAMPLE", ".", true, false, true,
       true, &Functions::VarianceSample});
  add({"VARIANCE_POPULATION", ".", true, false,
       true, true, &Functions::VariancePopulation});
  addAlias("VARIANCE", "VARIANCE_POPULATION");
  add({"STDDEV_SAMPLE", ".", true, false, true, true,
       &Functions::StdDevSample});
  add({"STDDEV_POPULATION", ".", true, false,
       true, true, &Functions::StdDevPopulation});
  addAlias("STDDEV", "STDDEV_POPULATION");
  add({"UNIQUE", ".", true, false, true, true,
       &Functions::Unique});
  add({"SORTED_UNIQUE", ".", true, false, true, true,
       &Functions::SortedUnique});
  add({"SLICE", ".,.|.", true, false, true, true,
       &Functions::Slice});
  add({"REVERSE", ".", true, false, true,
       true});  // note: REVERSE() can be applied on strings, too
  add({"FIRST", ".", true, false, true, true,
       &Functions::First});
  add({"LAST", ".", true, false, true, true,
       &Functions::Last});
  add({"NTH", ".,.", true, false, true, true,
       &Functions::Nth});
  add({"POSITION", ".,.|.", true, false, true, true,
       &Functions::Position});
  add({"CALL", ".|.+", false, true, false, true});
  add({"APPLY", ".|.", false, true, false, false});
  add({"PUSH", ".,.|.", true, false, true, false,
       &Functions::Push});
  add({"APPEND", ".,.|.", true, false, true, true,
       &Functions::Append});
  add({"POP", ".", true, false, true, true, &Functions::Pop});
  add({"SHIFT", ".", true, false, true, true,
       &Functions::Shift});
  add({"UNSHIFT", ".,.|.", true, false, true, true,
       &Functions::Unshift});
  add({"REMOVE_VALUE", ".,.|.", true, false, true,
       true, &Functions::RemoveValue});
  add({"REMOVE_VALUES", ".,.", true, false, true,
       true, &Functions::RemoveValues});
  add({"REMOVE_NTH", ".,.", true, false, true, true,
       &Functions::RemoveNth});
}

void AqlFunctionFeature::addDocumentFunctions() {
  // document functions
  add({"HAS", ".,.", true, false, true, true,
       &Functions::Has});
  add({"ATTRIBUTES", ".|.,.", true, false, true, true,
       &Functions::Attributes});
  add({"VALUES", ".|.", true, false, true, true,
       &Functions::Values});
  add({"MERGE", ".|+", true, false, true, true,
       &Functions::Merge});
  add({"MERGE_RECURSIVE", ".,.|+", true, false,
       true, true, &Functions::MergeRecursive});
  add({"DOCUMENT", "h.|.", false, true, false, true, &Functions::Document});
  add({"MATCHES", ".,.|.", true, false, true, true, &Functions::Matches});
  add({"UNSET", ".,.|+", true, false, true, true,
       &Functions::Unset});
  add({"UNSET_RECURSIVE", ".,.|+", true, false,
       true, true, &Functions::UnsetRecursive});
  add({"KEEP", ".,.|+", true, false, true, true,
       &Functions::Keep});
  add({"TRANSLATE", ".,.|.", true, false, true, true});
  add({"ZIP", ".,.", true, false, true, true,
       &Functions::Zip});
  add({"JSON_STRINGIFY", ".", true, false, true,
       true, &Functions::JsonStringify});
  add({"JSON_PARSE", ".", true, false, true, true,
       &Functions::JsonParse});
}

void AqlFunctionFeature::addGeoFunctions() {
  // geo functions
  add({"DISTANCE", ".,.,.,.", true, false, true, true,
       &Functions::Distance});
  add({"WITHIN_RECTANGLE", "h.,.,.,.,.", false,
       true, false, true});
  add({"IS_IN_POLYGON", ".,.|.", true, false, true,
       true});
}

void AqlFunctionFeature::addDateFunctions() {
  // date functions
  add({"DATE_NOW", "", false, false, true, true});
  add({"DATE_TIMESTAMP", ".|.,.,.,.,.,.", 
       true, false, true, true});
  add({"DATE_ISO8601", ".|.,.,.,.,.,.", true,
       false, true, true});
  add({"DATE_DAYOFWEEK", ".", true, false, true,
       true});
  add({"DATE_YEAR", ".", true, false, true, true});
  add({"DATE_MONTH", ".", true, false, true, true});
  add({"DATE_DAY", ".", true, false, true, true});
  add({"DATE_HOUR", ".", true, false, true, true});
  add({"DATE_MINUTE", ".", true, false, true, true});
  add({"DATE_SECOND", ".", true, false, true, true});
  add({"DATE_MILLISECOND", ".", true, false,
       true, true});
  add({"DATE_DAYOFYEAR", ".", true, false, true,
       true});
  add({"DATE_ISOWEEK", ".", true, false, true,
       true});
  add({"DATE_LEAPYEAR", ".", true, false, true,
       true});
  add({"DATE_QUARTER", ".", true, false, true,
       true});
  add({"DATE_DAYS_IN_MONTH", ".", true, false,
       true, true});
  add({"DATE_ADD", ".,.|.", true, false, true, true});
  add({"DATE_SUBTRACT", ".,.|.", true, false, true,
       true});
  add({"DATE_DIFF", ".,.,.|.", true, false, true,
       true});
  add({"DATE_COMPARE", ".,.,.|.", true, false, true,
       true});
  add({"DATE_FORMAT", ".,.", true, false, true,
       true});
}

void AqlFunctionFeature::addMiscFunctions() {
  // misc functions
  add({"FAIL", "|.", false, true, true, true});
  add({"PASSTHRU", ".", false, false, true, true,
       &Functions::Passthru});
  addAlias("NOOPT", "PASSTHRU");
  add({"V8", ".", true, false, true, true});
  add({"TEST_INTERNAL", ".,.", false, false, true,
       false});
  add({"SLEEP", ".", false, true, true, true, &Functions::Sleep});
  add({"COLLECTIONS", "", false, true, false, true, &Functions::Collections});
  add({"NOT_NULL", ".|+", true, false, true, true,
       &Functions::NotNull});
  add({"FIRST_LIST", ".|+", true, false, true, true,
       &Functions::FirstList});
  add({"FIRST_DOCUMENT", ".|+", true, false, true,
       true, &Functions::FirstDocument});
  add({"PARSE_IDENTIFIER", ".", true, false, true,
       true, &Functions::ParseIdentifier});
  add({"IS_SAME_COLLECTION", ".h,.h", true,
       false, true, true, &Functions::IsSameCollection});
  add({"CURRENT_USER", "", false, false, false,
       true});
  add({"CURRENT_DATABASE", "", false, false,
       false, true, &Functions::CurrentDatabase});
  add({"COLLECTION_COUNT", ".h", false, true,
       false, true, &Functions::CollectionCount});
  add({"CHECK_DOCUMENT", ".", false, false,
       true, true, &Functions::CheckDocument});
}

void AqlFunctionFeature::addStorageEngineFunctions() {
  StorageEngine* engine = EngineSelectorFeature::ENGINE; 
  TRI_ASSERT(engine != nullptr); // Engine not loaded. Startup broken
  engine->addAqlFunctions();
}
