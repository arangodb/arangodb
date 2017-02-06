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

#include "FunctionDefinitions.h"
#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Cluster/ServerState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

/// @brief determines if code is executed in cluster or not
static ExecutionCondition const NotInCluster =
    [] { return !arangodb::ServerState::instance()->isRunningInCluster(); };

/// @brief determines if code is executed on coordinator or not
static ExecutionCondition const NotInCoordinator = [] {
  return !arangodb::ServerState::instance()->isRunningInCluster() ||
         !arangodb::ServerState::instance()->isCoordinator();
};

/// @brief internal functions used in execution
std::unordered_map<int,
                   std::string const> const FunctionDefinitions::InternalFunctionNames{
    {static_cast<int>(NODE_TYPE_OPERATOR_UNARY_PLUS), "UNARY_PLUS"},
    {static_cast<int>(NODE_TYPE_OPERATOR_UNARY_MINUS), "UNARY_MINUS"},
    {static_cast<int>(NODE_TYPE_OPERATOR_UNARY_NOT), "LOGICAL_NOT"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_EQ), "RELATIONAL_EQUAL"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_NE), "RELATIONAL_UNEQUAL"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_GT), "RELATIONAL_GREATER"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_GE), "RELATIONAL_GREATEREQUAL"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_LT), "RELATIONAL_LESS"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_LE), "RELATIONAL_LESSEQUAL"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_IN), "RELATIONAL_IN"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_NIN), "RELATIONAL_NOT_IN"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_PLUS), "ARITHMETIC_PLUS"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_MINUS), "ARITHMETIC_MINUS"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_TIMES), "ARITHMETIC_TIMES"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_DIV), "ARITHMETIC_DIVIDE"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_MOD), "ARITHMETIC_MODULUS"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_AND), "LOGICAL_AND"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_OR), "LOGICAL_OR"},
    {static_cast<int>(NODE_TYPE_OPERATOR_TERNARY), "TERNARY_OPERATOR"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ), "RELATIONAL_ARRAY_EQUAL"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_ARRAY_NE), "RELATIONAL_ARRAY_UNEQUAL"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_ARRAY_GT), "RELATIONAL_ARRAY_GREATER"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_ARRAY_GE), "RELATIONAL_ARRAY_GREATEREQUAL"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_ARRAY_LT), "RELATIONAL_ARRAY_LESS"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_ARRAY_LE), "RELATIONAL_ARRAY_LESSEQUAL"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_ARRAY_IN), "RELATIONAL_ARRAY_IN"},
    {static_cast<int>(NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN), "RELATIONAL_ARRAY_NOT_IN"}};

/// @brief user-accessible functions
std::unordered_map<std::string, Function const> FunctionDefinitions::FunctionNames;
  
void FunctionDefinitions::toVelocyPack(VPackBuilder& builder) {
  builder.openArray();
  for (auto const& it : FunctionDefinitions::FunctionNames) {
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

void FunctionDefinitions::add(Function const& func) {
  TRI_ASSERT(FunctionNames.find(func.externalName) == FunctionNames.end());
  // add function to the map
  FunctionNames.emplace(func.externalName, func);
}


/// @brief this struct will add all AQL functions to the FunctionDefintions map
struct FunctionDefiner {
  FunctionDefiner() {
    addTypeCheckFunctions();
    addTypeCastFunctions();
    addStringFunctions();
    addNumericFunctions();
    addListFunctions();
    addDocumentFunctions();
    addGeoFunctions();
    addDateFunctions();
    addMiscFunctions();
  }

  // meanings of the symbols in the function arguments list
  // ------------------------------------------------------
  //
  // . = argument of any type (except collection)
  // c = collection name, will be converted into list with documents
  // h = collection name, will be converted into string
  // z = null
  // b = bool
  // n = number
  // s = string
  // p = primitive
  // l = array
  // a = object / document
  // r = regex (a string with a special format). note: the regex type is
  // mutually exclusive with all other types

  void addTypeCheckFunctions() {
    // type check functions
    FunctionDefinitions::add({"IS_NULL", "AQL_IS_NULL", ".", true, true, false,
                              true, true, &Functions::IsNull});
    FunctionDefinitions::add({"IS_BOOL", "AQL_IS_BOOL", ".", true, true, false,
                              true, true, &Functions::IsBool});
    FunctionDefinitions::add({"IS_NUMBER", "AQL_IS_NUMBER", ".", true, true,
                              false, true, true, &Functions::IsNumber});
    FunctionDefinitions::add({"IS_STRING", "AQL_IS_STRING", ".", true, true,
                              false, true, true, &Functions::IsString});
    FunctionDefinitions::add({"IS_ARRAY", "AQL_IS_ARRAY", ".", true, true,
                              false, true, true, &Functions::IsArray});
    // IS_LIST is an alias for IS_ARRAY
    FunctionDefinitions::add({"IS_LIST", "AQL_IS_LIST", ".", true, true, false,
                              true, true, &Functions::IsArray});
    FunctionDefinitions::add({"IS_OBJECT", "AQL_IS_OBJECT", ".", true, true,
                              false, true, true, &Functions::IsObject});
    // IS_DOCUMENT is an alias for IS_OBJECT
    FunctionDefinitions::add({"IS_DOCUMENT", "AQL_IS_DOCUMENT", ".", true, true,
                              false, true, true, &Functions::IsObject});

    FunctionDefinitions::add({"IS_DATESTRING", "AQL_IS_DATESTRING", ".", true,
                              true, false, true, true});
    FunctionDefinitions::add({"TYPENAME", "AQL_TYPENAME", ".", true, true,
                              false, true, true, &Functions::Typename});
  }

  void addTypeCastFunctions() {
    // type cast functions
    FunctionDefinitions::add({"TO_NUMBER", "AQL_TO_NUMBER", ".", true, true,
                              false, true, true, &Functions::ToNumber});
    FunctionDefinitions::add({"TO_STRING", "AQL_TO_STRING", ".", true, true,
                              false, true, true, &Functions::ToString});
    FunctionDefinitions::add({"TO_BOOL", "AQL_TO_BOOL", ".", true, true, false,
                              true, true, &Functions::ToBool});
    FunctionDefinitions::add({"TO_ARRAY", "AQL_TO_ARRAY", ".", true, true,
                              false, true, true, &Functions::ToArray});
    // TO_LIST is an alias for TO_ARRAY
    FunctionDefinitions::add({"TO_LIST", "AQL_TO_LIST", ".", true, true, false,
                              true, true, &Functions::ToArray});
  }

  void addStringFunctions() {
    // string functions
    FunctionDefinitions::add({"CONCAT", "AQL_CONCAT", "szl|+", true, true,
                              false, true, true, &Functions::Concat});
    FunctionDefinitions::add({"CONCAT_SEPARATOR", "AQL_CONCAT_SEPARATOR",
                              "s,szl|+", true, true, false, true, true,
                              &Functions::ConcatSeparator});
    FunctionDefinitions::add(
        {"CHAR_LENGTH", "AQL_CHAR_LENGTH", "s", true, true, false, true, true});
    FunctionDefinitions::add(
        {"LOWER", "AQL_LOWER", "s", true, true, false, true, true});
    FunctionDefinitions::add(
        {"UPPER", "AQL_UPPER", "s", true, true, false, true, true});
    FunctionDefinitions::add(
        {"SUBSTRING", "AQL_SUBSTRING", "s,n|n", true, true, false, true, true});
    FunctionDefinitions::add({"CONTAINS", "AQL_CONTAINS", "s,s|b", true, true,
                              false, true, true, &Functions::Contains});
    FunctionDefinitions::add({"LIKE", "AQL_LIKE", "s,r|b", true, true, false,
                              true, true, &Functions::Like});
    FunctionDefinitions::add({"REGEX_TEST", "AQL_REGEX_TEST", "s,r|b", true,
                              true, false, true, true, &Functions::RegexTest});
    FunctionDefinitions::add({"REGEX_REPLACE", "AQL_REGEX_REPLACE", "s,r,s|b",
                              true, true, false, true, true,
                              &Functions::RegexReplace});
    FunctionDefinitions::add(
        {"LEFT", "AQL_LEFT", "s,n", true, true, false, true, true});
    FunctionDefinitions::add(
        {"RIGHT", "AQL_RIGHT", "s,n", true, true, false, true, true});
    FunctionDefinitions::add(
        {"TRIM", "AQL_TRIM", "s|ns", true, true, false, true, true});
    FunctionDefinitions::add(
        {"LTRIM", "AQL_LTRIM", "s|s", true, true, false, true, true});
    FunctionDefinitions::add(
        {"RTRIM", "AQL_RTRIM", "s|s", true, true, false, true, true});
    FunctionDefinitions::add({"FIND_FIRST", "AQL_FIND_FIRST", "s,s|zn,zn", true,
                              true, false, true, true});
    FunctionDefinitions::add({"FIND_LAST", "AQL_FIND_LAST", "s,s|zn,zn", true,
                              true, false, true, true});
    FunctionDefinitions::add(
        {"SPLIT", "AQL_SPLIT", "s|sl,n", true, true, false, true, true});
    FunctionDefinitions::add({"SUBSTITUTE", "AQL_SUBSTITUTE", "s,las|lsn,n",
                              true, true, false, true, true});
    FunctionDefinitions::add({"MD5", "AQL_MD5", "s", true, true, false, true,
                              true, &Functions::Md5});
    FunctionDefinitions::add({"SHA1", "AQL_SHA1", "s", true, true, false, true,
                              true, &Functions::Sha1});
    FunctionDefinitions::add({"HASH", "AQL_HASH", ".", true, true, false, true,
                              true, &Functions::Hash});
    FunctionDefinitions::add({"RANDOM_TOKEN", "AQL_RANDOM_TOKEN", "n", false,
                              false, true, true, true,
                              &Functions::RandomToken});
  }

  void addNumericFunctions() {
    // numeric functions
    FunctionDefinitions::add({"FLOOR", "AQL_FLOOR", "n", true, true, false,
                              true, true, &Functions::Floor});
    FunctionDefinitions::add({"CEIL", "AQL_CEIL", "n", true, true, false, true,
                              true, &Functions::Ceil});
    FunctionDefinitions::add({"ROUND", "AQL_ROUND", "n", true, true, false,
                              true, true, &Functions::Round});
    FunctionDefinitions::add({"ABS", "AQL_ABS", "n", true, true, false, true,
                              true, &Functions::Abs});
    FunctionDefinitions::add({"RAND", "AQL_RAND", "", false, false, false, true,
                              true, &Functions::Rand});
    FunctionDefinitions::add({"SQRT", "AQL_SQRT", "n", true, true, false, true,
                              true, &Functions::Sqrt});
    FunctionDefinitions::add({"POW", "AQL_POW", "n,n", true, true, false, true,
                              true, &Functions::Pow});
    FunctionDefinitions::add({"LOG", "AQL_LOG", "n", true, true, false, true,
                              true, &Functions::Log});
    FunctionDefinitions::add({"LOG2", "AQL_LOG2", "n", true, true, false, true,
                              true, &Functions::Log2});
    FunctionDefinitions::add({"LOG10", "AQL_LOG10", "n", true, true, false,
                              true, true, &Functions::Log10});
    FunctionDefinitions::add({"EXP", "AQL_EXP", "n", true, true, false, true,
                              true, &Functions::Exp});
    FunctionDefinitions::add({"EXP2", "AQL_EXP2", "n", true, true, false, true,
                              true, &Functions::Exp2});
    FunctionDefinitions::add({"SIN", "AQL_SIN", "n", true, true, false, true,
                              true, &Functions::Sin});
    FunctionDefinitions::add({"COS", "AQL_COS", "n", true, true, false, true,
                              true, &Functions::Cos});
    FunctionDefinitions::add({"TAN", "AQL_TAN", "n", true, true, false, true,
                              true, &Functions::Tan});
    FunctionDefinitions::add({"ASIN", "AQL_ASIN", "n", true, true, false, true,
                              true, &Functions::Asin});
    FunctionDefinitions::add({"ACOS", "AQL_ACOS", "n", true, true, false, true,
                              true, &Functions::Acos});
    FunctionDefinitions::add({"ATAN", "AQL_ATAN", "n", true, true, false, true,
                              true, &Functions::Atan});
    FunctionDefinitions::add({"ATAN2", "AQL_ATAN2", "n,n", true, true, false,
                              true, true, &Functions::Atan2});
    FunctionDefinitions::add({"RADIANS", "AQL_RADIANS", "n", true, true, false,
                              true, true, &Functions::Radians});
    FunctionDefinitions::add({"DEGREES", "AQL_DEGREES", "n", true, true, false,
                              true, true, &Functions::Degrees});
    FunctionDefinitions::add(
        {"PI", "AQL_PI", "", true, true, false, true, true, &Functions::Pi});
  }

  void addListFunctions() { 
    // list functions
    FunctionDefinitions::add({"RANGE", "AQL_RANGE", "n,n|n", true, true, false,
                              true, true, &Functions::Range});
    FunctionDefinitions::add({"UNION", "AQL_UNION", "l,l|+", true, true, false,
                              true, true, &Functions::Union});
    FunctionDefinitions::add({"UNION_DISTINCT", "AQL_UNION_DISTINCT", "l,l|+",
                              true, true, false, true, true,
                              &Functions::UnionDistinct});
    FunctionDefinitions::add({"MINUS", "AQL_MINUS", "l,l|+", true, true, false,
                              true, true, &Functions::Minus});
    FunctionDefinitions::add({"OUTERSECTION", "AQL_OUTERSECTION", "l,l|+", true,
                              true, false, true, true,
                              &Functions::Outersection});
    FunctionDefinitions::add({"INTERSECTION", "AQL_INTERSECTION", "l,l|+", true,
                              true, false, true, true,
                              &Functions::Intersection});
    FunctionDefinitions::add({"FLATTEN", "AQL_FLATTEN", "l|n", true, true,
                              false, true, true, &Functions::Flatten});
    FunctionDefinitions::add({"LENGTH", "AQL_LENGTH", "las", true, true, false,
                              true, true, &Functions::Length});
    FunctionDefinitions::add({"COUNT", "AQL_LENGTH", "las", true, true, false,
                              true, true,
                              &Functions::Length});  // alias for LENGTH()
    FunctionDefinitions::add({"MIN", "AQL_MIN", "l", true, true, false, true,
                              true, &Functions::Min});
    FunctionDefinitions::add({"MAX", "AQL_MAX", "l", true, true, false, true,
                              true, &Functions::Max});
    FunctionDefinitions::add({"SUM", "AQL_SUM", "l", true, true, false, true,
                              true, &Functions::Sum});
    FunctionDefinitions::add({"MEDIAN", "AQL_MEDIAN", "l", true, true, false,
                              true, true, &Functions::Median});
    FunctionDefinitions::add({"PERCENTILE", "AQL_PERCENTILE", "l,n|s", true,
                              true, false, true, true, &Functions::Percentile});
    FunctionDefinitions::add({"AVERAGE", "AQL_AVERAGE", "l", true, true, false,
                              true, true, &Functions::Average});
    FunctionDefinitions::add({"AVG", "AQL_AVERAGE", "l", true, true, false,
                              true, true,
                              &Functions::Average});  // alias for AVERAGE()
    FunctionDefinitions::add({"VARIANCE_SAMPLE", "AQL_VARIANCE_SAMPLE", "l",
                              true, true, false, true, true,
                              &Functions::VarianceSample});
    FunctionDefinitions::add({"VARIANCE_POPULATION", "AQL_VARIANCE_POPULATION",
                              "l", true, true, false, true, true,
                              &Functions::VariancePopulation});
    FunctionDefinitions::add(
        {"VARIANCE", "AQL_VARIANCE_POPULATION", "l", true, true, false, true,
         true,
         &Functions::VariancePopulation});  // alias for VARIANCE_POPULATION()
    FunctionDefinitions::add({"STDDEV_SAMPLE", "AQL_STDDEV_SAMPLE", "l", true,
                              true, false, true, true,
                              &Functions::StdDevSample});
    FunctionDefinitions::add({"STDDEV_POPULATION", "AQL_STDDEV_POPULATION", "l",
                              true, true, false, true, true,
                              &Functions::StdDevPopulation});
    FunctionDefinitions::add(
        {"STDDEV", "AQL_STDDEV_POPULATION", "l", true, true, false, true, true,
         &Functions::StdDevPopulation});  // alias for STDDEV_POPULATION()
    FunctionDefinitions::add({"UNIQUE", "AQL_UNIQUE", "l", true, true, false,
                              true, true, &Functions::Unique});
    FunctionDefinitions::add({"SORTED_UNIQUE", "AQL_SORTED_UNIQUE", "l", true,
                              true, false, true, true,
                              &Functions::SortedUnique});
    FunctionDefinitions::add({"SLICE", "AQL_SLICE", "l,n|n", true, true, false,
                              true, true, &Functions::Slice});
    FunctionDefinitions::add(
        {"REVERSE", "AQL_REVERSE", "ls", true, true, false, true,
         true});  // note: REVERSE() can be applied on strings, too
    FunctionDefinitions::add({"FIRST", "AQL_FIRST", "l", true, true, false,
                              true, true, &Functions::First});
    FunctionDefinitions::add({"LAST", "AQL_LAST", "l", true, true, false, true,
                              true, &Functions::Last});
    FunctionDefinitions::add({"NTH", "AQL_NTH", "l,n", true, true, false, true,
                              true, &Functions::Nth});
    FunctionDefinitions::add({"POSITION", "AQL_POSITION", "l,.|b", true, true,
                              false, true, true, &Functions::Position});
    FunctionDefinitions::add(
        {"CALL", "AQL_CALL", "s|.+", false, false, true, false, true});
    FunctionDefinitions::add(
        {"APPLY", "AQL_APPLY", "s|l", false, false, true, false, false});
    FunctionDefinitions::add({"PUSH", "AQL_PUSH", "l,.|b", true, true, false,
                              true, false, &Functions::Push});
    FunctionDefinitions::add({"APPEND", "AQL_APPEND", "l,lz|b", true, true,
                              false, true, true, &Functions::Append});
    FunctionDefinitions::add({"POP", "AQL_POP", "l", true, true, false, true,
                              true, &Functions::Pop});
    FunctionDefinitions::add({"SHIFT", "AQL_SHIFT", "l", true, true, false,
                              true, true, &Functions::Shift});
    FunctionDefinitions::add({"UNSHIFT", "AQL_UNSHIFT", "l,.|b", true, true,
                              false, true, true, &Functions::Unshift});
    FunctionDefinitions::add({"REMOVE_VALUE", "AQL_REMOVE_VALUE", "l,.|n", true,
                              true, false, true, true,
                              &Functions::RemoveValue});
    FunctionDefinitions::add({"REMOVE_VALUES", "AQL_REMOVE_VALUES", "l,lz",
                              true, true, false, true, true,
                              &Functions::RemoveValues});
    FunctionDefinitions::add({"REMOVE_NTH", "AQL_REMOVE_NTH", "l,n", true, true,
                              false, true, true, &Functions::RemoveNth});
  }

  void addDocumentFunctions() {
    // document functions
    FunctionDefinitions::add({"HAS", "AQL_HAS", "az,s", true, true, false, true,
                              true, &Functions::Has});
    FunctionDefinitions::add({"ATTRIBUTES", "AQL_ATTRIBUTES", "a|b,b", true,
                              true, false, true, true, &Functions::Attributes});
    FunctionDefinitions::add({"VALUES", "AQL_VALUES", "a|b", true, true, false,
                              true, true, &Functions::Values});
    FunctionDefinitions::add({"MERGE", "AQL_MERGE", "la|+", true, true, false,
                              true, true, &Functions::Merge});
    FunctionDefinitions::add({"MERGE_RECURSIVE", "AQL_MERGE_RECURSIVE", "a,a|+",
                              true, true, false, true, true,
                              &Functions::MergeRecursive});
    FunctionDefinitions::add({"DOCUMENT", "AQL_DOCUMENT", "h.|.", false, false,
                              true, false, true, &Functions::Document,
                              NotInCluster});
    FunctionDefinitions::add(
        {"MATCHES", "AQL_MATCHES", ".,l|b", true, true, false, true, true});
    FunctionDefinitions::add({"UNSET", "AQL_UNSET", "a,sl|+", true, true, false,
                              true, true, &Functions::Unset});
    FunctionDefinitions::add({"UNSET_RECURSIVE", "AQL_UNSET_RECURSIVE",
                              "a,sl|+", true, true, false, true, true,
                              &Functions::UnsetRecursive});
    FunctionDefinitions::add({"KEEP", "AQL_KEEP", "a,sl|+", true, true, false,
                              true, true, &Functions::Keep});
    FunctionDefinitions::add(
        {"TRANSLATE", "AQL_TRANSLATE", ".,a|.", true, true, false, true, true});
    FunctionDefinitions::add({"ZIP", "AQL_ZIP", "l,l", true, true, false, true,
                              true, &Functions::Zip});
    FunctionDefinitions::add({"JSON_STRINGIFY", "AQL_JSON_STRINGIFY", ".", true,
                              true, false, true, true,
                              &Functions::JsonStringify});
    FunctionDefinitions::add({"JSON_PARSE", "AQL_JSON_PARSE", ".", true, true,
                              false, true, true, &Functions::JsonParse});
  }

  void addGeoFunctions() {
    // geo functions
   FunctionDefinitions::add({"DISTANCE", "AQL_DISTANCE", "n,n,n,n", true, true,
                              false, true, true, &Functions::Distance});
    FunctionDefinitions::add({"WITHIN_RECTANGLE", "AQL_WITHIN_RECTANGLE",
                              "hs,d,d,d,d", true, false, true, false, true});
    FunctionDefinitions::add({"IS_IN_POLYGON", "AQL_IS_IN_POLYGON", "l,ln|nb",
                              true, true, false, true, true});
  }

  void addDateFunctions() {
    // date functions
    FunctionDefinitions::add(
        {"DATE_NOW", "AQL_DATE_NOW", "", false, false, false, true, true});
    FunctionDefinitions::add({"DATE_TIMESTAMP", "AQL_DATE_TIMESTAMP",
                              "ns|ns,ns,ns,ns,ns,ns", true, true, false, true,
                              true});
    FunctionDefinitions::add({"DATE_ISO8601", "AQL_DATE_ISO8601",
                              "ns|ns,ns,ns,ns,ns,ns", true, true, false, true,
                              true});
    FunctionDefinitions::add({"DATE_DAYOFWEEK", "AQL_DATE_DAYOFWEEK", "ns",
                              true, true, false, true, true});
    FunctionDefinitions::add(
        {"DATE_YEAR", "AQL_DATE_YEAR", "ns", true, true, false, true, true});
    FunctionDefinitions::add(
        {"DATE_MONTH", "AQL_DATE_MONTH", "ns", true, true, false, true, true});
    FunctionDefinitions::add(
        {"DATE_DAY", "AQL_DATE_DAY", "ns", true, true, false, true, true});
    FunctionDefinitions::add(
        {"DATE_HOUR", "AQL_DATE_HOUR", "ns", true, true, false, true, true});
    FunctionDefinitions::add({"DATE_MINUTE", "AQL_DATE_MINUTE", "ns", true,
                              true, false, true, true});
    FunctionDefinitions::add({"DATE_SECOND", "AQL_DATE_SECOND", "ns", true,
                              true, false, true, true});
    FunctionDefinitions::add({"DATE_MILLISECOND", "AQL_DATE_MILLISECOND", "ns",
                              true, true, false, true, true});
    FunctionDefinitions::add({"DATE_DAYOFYEAR", "AQL_DATE_DAYOFYEAR", "ns",
                              true, true, false, true, true});
    FunctionDefinitions::add({"DATE_ISOWEEK", "AQL_DATE_ISOWEEK", "ns", true,
                              true, false, true, true});
    FunctionDefinitions::add({"DATE_LEAPYEAR", "AQL_DATE_LEAPYEAR", "ns", true,
                              true, false, true, true});
    FunctionDefinitions::add({"DATE_QUARTER", "AQL_DATE_QUARTER", "ns", true,
                              true, false, true, true});
    FunctionDefinitions::add({"DATE_DAYS_IN_MONTH", "AQL_DATE_DAYS_IN_MONTH",
                              "ns", true, true, false, true, true});
    FunctionDefinitions::add(
        {"DATE_ADD", "AQL_DATE_ADD", "ns,ns|n", true, true, false, true, true});
    FunctionDefinitions::add({"DATE_SUBTRACT", "AQL_DATE_SUBTRACT", "ns,ns|n",
                              true, true, false, true, true});
    FunctionDefinitions::add({"DATE_DIFF", "AQL_DATE_DIFF", "ns,ns,s|b", true,
                              true, false, true, true});
    FunctionDefinitions::add({"DATE_COMPARE", "AQL_DATE_COMPARE", "ns,ns,s|s",
                              true, true, false, true, true});
    FunctionDefinitions::add({"DATE_FORMAT", "AQL_DATE_FORMAT", "ns,s", true,
                              true, false, true, true});
  }

  void addMiscFunctions() {
    // misc functions
    FunctionDefinitions::add(
        {"FAIL", "AQL_FAIL", "|s", false, false, true, true, true});
    FunctionDefinitions::add({"PASSTHRU", "AQL_PASSTHRU", ".", false, false,
                              false, true, true, &Functions::Passthru});
    FunctionDefinitions::add({"NOOPT", "AQL_PASSTHRU", ".", false, false, false,
                              true, true, &Functions::Passthru});
    FunctionDefinitions::add(
        {"V8", "AQL_PASSTHRU", ".", false, true, false, true, true});
    FunctionDefinitions::add({"TEST_INTERNAL", "AQL_TEST_INTERNAL", "s,.",
                              false, false, false, true, false});
    FunctionDefinitions::add(
        {"SLEEP", "AQL_SLEEP", "n", false, false, true, true, true});
    FunctionDefinitions::add({"COLLECTIONS", "AQL_COLLECTIONS", "", false,
                              false, true, false, true});
    FunctionDefinitions::add({"NOT_NULL", "AQL_NOT_NULL", ".|+", true, true,
                              false, true, true, &Functions::NotNull});
    FunctionDefinitions::add({"FIRST_LIST", "AQL_FIRST_LIST", ".|+", true, true,
                              false, true, true, &Functions::FirstList});
    FunctionDefinitions::add({"FIRST_DOCUMENT", "AQL_FIRST_DOCUMENT", ".|+",
                              true, true, false, true, true,
                              &Functions::FirstDocument});
    FunctionDefinitions::add({"PARSE_IDENTIFIER", "AQL_PARSE_IDENTIFIER", ".",
                              true, true, false, true, true,
                              &Functions::ParseIdentifier});
    FunctionDefinitions::add({"IS_SAME_COLLECTION", "AQL_IS_SAME_COLLECTION",
                              "chas,chas", true, true, false, true, true,
                              &Functions::IsSameCollection});
    FunctionDefinitions::add({"CURRENT_USER", "AQL_CURRENT_USER", "", false,
                              false, false, false, true});
    FunctionDefinitions::add({"CURRENT_DATABASE", "AQL_CURRENT_DATABASE", "",
                              false, false, false, false, true,
                              &Functions::CurrentDatabase});
    FunctionDefinitions::add({"COLLECTION_COUNT", "AQL_COLLECTION_COUNT", "chs",
                              false, false, true, false, true,
                              &Functions::CollectionCount, NotInCluster});
  }

};

/// @brief this definer instance will add all AQL functions to the map
static FunctionDefiner definer;

