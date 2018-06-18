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
    : application_features::ApplicationFeature(server, "AQLFunctions") {
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
  Functions::init();

  /// @brief Add all AQL functions to the FunctionDefintions map
  addTypeCheckFunctions();
  addTypeCastFunctions();
  addStringFunctions();
  addNumericFunctions();
  addListFunctions();
  addDocumentFunctions();
  addGeoFunctions();
  addGeometryConstructors();
  addDateFunctions();
  addMiscFunctions();
  addStorageEngineFunctions();

  add({"PREGEL_RESULT", ".", false, true,
    true, &Functions::PregelResult});
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
  add({"IS_NULL", ".", true, false, true, &Functions::IsNull});
  add({"IS_BOOL", ".", true, false, true, &Functions::IsBool});
  add({"IS_NUMBER", ".", true, false, true, &Functions::IsNumber});
  add({"IS_STRING", ".", true, false, true, &Functions::IsString});
  add({"IS_ARRAY", ".", true, false, true, &Functions::IsArray});
  // IS_LIST is an alias for IS_ARRAY
  addAlias("IS_LIST", "IS_ARRAY");
  add({"IS_OBJECT", ".", true, false, true, &Functions::IsObject});
  // IS_DOCUMENT is an alias for IS_OBJECT
  addAlias("IS_DOCUMENT", "IS_OBJECT");

  add({"IS_DATESTRING", ".", true, false, true, &Functions::IsDatestring});
  add({"IS_KEY", ".", true, false, true, &Functions::IsKey});
  add({"TYPENAME", ".", true, false, true, &Functions::Typename});
}

void AqlFunctionFeature::addTypeCastFunctions() {
  // type cast functions
  add({"TO_NUMBER", ".", true, false, true, &Functions::ToNumber});
  add({"TO_STRING", ".", true, false, true, &Functions::ToString});
  add({"TO_BOOL", ".", true, false, true, &Functions::ToBool});
  add({"TO_ARRAY", ".", true, false, true, &Functions::ToArray});
  // TO_LIST is an alias for TO_ARRAY
  addAlias("TO_LIST", "TO_ARRAY");
}

void AqlFunctionFeature::addStringFunctions() {
  // string functions
  add({"CONCAT", ".|+", true, false, true, &Functions::Concat});
  add({"CONCAT_SEPARATOR", ".,.|+", true, false,
       true, &Functions::ConcatSeparator});
  add({"CHAR_LENGTH", ".", true, false, true, &Functions::CharLength});
  add({"LOWER", ".", true, false, true, &Functions::Lower});
  add({"UPPER", ".", true, false, true, &Functions::Upper});
  add({"SUBSTRING", ".,.|.", true, false, true, &Functions::Substring});
  add({"CONTAINS", ".,.|.", true, false, true, &Functions::Contains});
  add({"LIKE", ".,.|.", true, false, true, &Functions::Like});
  add({"REGEX_TEST", ".,.|.", true, false, true, &Functions::RegexTest});
  add({"REGEX_REPLACE", ".,.,.|.", true, false, true, &Functions::RegexReplace});
  add({"LEFT", ".,.", true, false, true, &Functions::Left});
  add({"RIGHT", ".,.", true, false, true, &Functions::Right});
  add({"TRIM", ".|.", true, false, true, &Functions::Trim});
  add({"LTRIM", ".|.", true, false, true, &Functions::LTrim});
  add({"RTRIM", ".|.", true, false, true, &Functions::RTrim});
  add({"FIND_FIRST", ".,.|.,.", true, false, true, &Functions::FindFirst});
  add({"FIND_LAST", ".,.|.,.", true, false, true, &Functions::FindLast});
  add({"SPLIT", ".|.,.", true, false, true, &Functions::Split});
  add({"SUBSTITUTE", ".,.|.,.", true, false, true, &Functions::Substitute});
  add({"MD5", ".", true, false, true, &Functions::Md5});
  add({"SHA1", ".", true, false, true, &Functions::Sha1});
  add({"SHA512", ".", true, false, true, &Functions::Sha512});
  add({"HASH", ".", true, false, true, &Functions::Hash});
  add({"RANDOM_TOKEN", ".", false, true, true, &Functions::RandomToken});
  add({"FULLTEXT", ".h,.,.|." , false, true, false, &Functions::Fulltext});
}

void AqlFunctionFeature::addNumericFunctions() {
  // numeric functions
  add({"FLOOR", ".", true, false, true, &Functions::Floor});
  add({"CEIL", ".", true, false, true, &Functions::Ceil});
  add({"ROUND", ".", true, false, true, &Functions::Round});
  add({"ABS", ".", true, false, true, &Functions::Abs});
  add({"RAND", "", false, false, true, &Functions::Rand});
  add({"SQRT", ".", true, false, true, &Functions::Sqrt});
  add({"POW", ".,.", true, false, true, &Functions::Pow});
  add({"LOG", ".", true, false, true, &Functions::Log});
  add({"LOG2", ".", true, false, true, &Functions::Log2});
  add({"LOG10", ".", true, false, true, &Functions::Log10});
  add({"EXP", ".", true, false, true, &Functions::Exp});
  add({"EXP2", ".", true, false, true, &Functions::Exp2});
  add({"SIN", ".", true, false, true, &Functions::Sin});
  add({"COS", ".", true, false, true, &Functions::Cos});
  add({"TAN", ".", true, false, true, &Functions::Tan});
  add({"ASIN", ".", true, false, true, &Functions::Asin});
  add({"ACOS", ".", true, false, true, &Functions::Acos});
  add({"ATAN", ".", true, false, true, &Functions::Atan});
  add({"ATAN2", ".,.", true, false, true, &Functions::Atan2});
  add({"RADIANS", ".", true, false, true, &Functions::Radians});
  add({"DEGREES", ".", true, false, true, &Functions::Degrees});
  add({"PI", "", true, false, true, &Functions::Pi});
}

void AqlFunctionFeature::addListFunctions() {
  // list functions
  add({"RANGE", ".,.|.", true, false, true, &Functions::Range});
  add({"UNION", ".,.|+", true, false, true, &Functions::Union});
  add({"UNION_DISTINCT", ".,.|+", true, false, true, &Functions::UnionDistinct});
  add({"MINUS", ".,.|+", true, false, true, &Functions::Minus});
  add({"OUTERSECTION", ".,.|+", true, false, true, &Functions::Outersection});
  add({"INTERSECTION", ".,.|+", true, false, true, &Functions::Intersection});
  add({"FLATTEN", ".|.", true, false, true, &Functions::Flatten});
  add({"LENGTH", ".", true, false, true, &Functions::Length});
  addAlias("COUNT", "LENGTH");
  add({"MIN", ".", true, false, true, &Functions::Min});
  add({"MAX", ".", true, false, true, &Functions::Max});
  add({"SUM", ".", true, false, true, &Functions::Sum});
  add({"MEDIAN", ".", true, false, true, &Functions::Median});
  add({"PERCENTILE", ".,.|.", true, false, true, &Functions::Percentile});
  add({"AVERAGE", ".", true, false, true, &Functions::Average});
  addAlias("AVG", "AVERAGE");
  add({"VARIANCE_SAMPLE", ".", true, false, true, &Functions::VarianceSample});
  add({"VARIANCE_POPULATION", ".", true, false, true, &Functions::VariancePopulation});
  addAlias("VARIANCE", "VARIANCE_POPULATION");
  add({"STDDEV_SAMPLE", ".", true, false, true, &Functions::StdDevSample});
  add({"STDDEV_POPULATION", ".", true, false, true, &Functions::StdDevPopulation});
  addAlias("STDDEV", "STDDEV_POPULATION");
  add({"UNIQUE", ".", true, false, true, &Functions::Unique});
  add({"SORTED_UNIQUE", ".", true, false, true, &Functions::SortedUnique});
  add({"SORTED", ".", true, false, true, &Functions::Sorted});
  add({"SLICE", ".,.|.", true, false, true, &Functions::Slice});
  add({"REVERSE", ".", true, false, true, &Functions::Reverse});
  add({"FIRST", ".", true, false, true, &Functions::First});
  add({"LAST", ".", true, false, true, &Functions::Last});
  add({"NTH", ".,.", true, false, true, &Functions::Nth});
  add({"POSITION", ".,.|.", true, false, true, &Functions::Position});
  add({"CALL", ".|.+", false, true, false, &Functions::Call});
  add({"APPLY", ".|.", false, true, false, &Functions::Apply});
  add({"PUSH", ".,.|.", true, false, true, &Functions::Push});
  add({"APPEND", ".,.|.", true, false, true, &Functions::Append});
  add({"POP", ".", true, false, true, &Functions::Pop});
  add({"SHIFT", ".", true, false, true, &Functions::Shift});
  add({"UNSHIFT", ".,.|.", true, false, true, &Functions::Unshift});
  add({"REMOVE_VALUE", ".,.|.", true, false, true, &Functions::RemoveValue});
  add({"REMOVE_VALUES", ".,.", true, false, true, &Functions::RemoveValues});
  add({"REMOVE_NTH", ".,.", true, false, true, &Functions::RemoveNth});
}

void AqlFunctionFeature::addDocumentFunctions() {
  // document functions
  add({"HAS", ".,.", true, false, true, &Functions::Has});
  add({"ATTRIBUTES", ".|.,.", true, false, true, &Functions::Attributes});
  add({"VALUES", ".|.", true, false, true, &Functions::Values});
  add({"MERGE", ".|+", true, false, true, &Functions::Merge});
  add({"MERGE_RECURSIVE", ".,.|+", true, false, true, &Functions::MergeRecursive});
  add({"DOCUMENT", "h.|.", false, true, false, &Functions::Document});
  add({"MATCHES", ".,.|.", true, false, true, &Functions::Matches});
  add({"UNSET", ".,.|+", true, false, true, &Functions::Unset});
  add({"UNSET_RECURSIVE", ".,.|+", true, false, true, &Functions::UnsetRecursive});
  add({"KEEP", ".,.|+", true, false, true, &Functions::Keep});
  add({"TRANSLATE", ".,.|.", true, false, true, &Functions::Translate});
  add({"ZIP", ".,.", true, false, true, &Functions::Zip});
  add({"JSON_STRINGIFY", ".", true, false, true, &Functions::JsonStringify});
  add({"JSON_PARSE", ".", true, false, true, &Functions::JsonParse});
}

void AqlFunctionFeature::addGeoFunctions() {
  // geo functions
  add({"DISTANCE", ".,.,.,.", true, false, true, &Functions::Distance});
  add({"WITHIN_RECTANGLE", "h.,.,.,.,.", false, true, false });
  add({"IS_IN_POLYGON", ".,.|.", true, false, true, &Functions::IsInPolygon});
  add({"GEO_DISTANCE", ".,.", true, false, true, &Functions::GeoDistance});
  add({"GEO_CONTAINS", ".,.", true, false, true, &Functions::GeoContains});
  add({"GEO_INTERSECTS", ".,.", true, false, true, &Functions::GeoIntersects});
  add({"GEO_EQUALS", ".,.", true, false, true, &Functions::GeoEquals});
  add({"NEAR", ".h,.,.|.,.", false, true, false, &Functions::Near});
  add({"WITHIN", ".h,.,.,.|.", false, true, false, &Functions::Within});
}

void AqlFunctionFeature::addGeometryConstructors() {
  // geometry types
  add({"GEO_POINT", ".,.", true, false, true, &Functions::GeoPoint});
  add({"GEO_MULTIPOINT", ".", true, false, true, &Functions::GeoMultiPoint});
  add({"GEO_POLYGON", ".", true, false, true, &Functions::GeoPolygon});
  add({"GEO_LINESTRING", ".", true, false, true, &Functions::GeoLinestring});
  add({"GEO_MULTILINESTRING", ".", true, false, true, &Functions::GeoMultiLinestring});
}

void AqlFunctionFeature::addDateFunctions() {
  // date functions
  add({"DATE_NOW", "", false, false, true, &Functions::DateNow});
  add({"DATE_TIMESTAMP", ".|.,.,.,.,.,.", true, false, true, &Functions::DateTimestamp});
  add({"DATE_ISO8601", ".|.,.,.,.,.,.", true, false, true, &Functions::DateIso8601});
  add({"DATE_DAYOFWEEK", ".", true, false, true, &Functions::DateDayOfWeek});
  add({"DATE_YEAR", ".", true, false, true, &Functions::DateYear});
  add({"DATE_MONTH", ".", true, false, true, &Functions::DateMonth});
  add({"DATE_DAY", ".", true, false, true, &Functions::DateDay});
  add({"DATE_HOUR", ".", true, false, true, &Functions::DateHour});
  add({"DATE_MINUTE", ".", true, false, true, &Functions::DateMinute});
  add({"DATE_SECOND", ".", true, false, true, &Functions::DateSecond});
  add({"DATE_MILLISECOND", ".", true, false, true, &Functions::DateMillisecond});
  add({"DATE_DAYOFYEAR", ".", true, false, true, &Functions::DateDayOfYear});
  add({"DATE_ISOWEEK", ".", true, false, true, &Functions::DateIsoWeek});
  add({"DATE_LEAPYEAR", ".", true, false, true, &Functions::DateLeapYear});
  add({"DATE_QUARTER", ".", true, false, true, &Functions::DateQuarter});
  add({"DATE_DAYS_IN_MONTH", ".", true, false, true, &Functions::DateDaysInMonth});
  add({"DATE_ADD", ".,.|.", true, false, true, &Functions::DateAdd});
  add({"DATE_SUBTRACT", ".,.|.", true, false, true, &Functions::DateSubtract});
  add({"DATE_DIFF", ".,.,.|.", true, false, true, &Functions::DateDiff});
  add({"DATE_COMPARE", ".,.,.|.", true, false, true, &Functions::DateCompare});
  add({"DATE_FORMAT", ".,.", true, false, true, &Functions::DateFormat});
  add({"DATE_TRUNC",   ".,.", true, false, true, &Functions::DateTrunc});
}

void AqlFunctionFeature::addMiscFunctions() {
  // misc functions
  add({"FAIL", "|.", false, true, true, &Functions::Fail});
  add({"PASSTHRU", ".", false, false, true, &Functions::Passthru});
  addAlias("NOOPT", "PASSTHRU");
  add({"V8", ".", true, false, true });
  add({"SLEEP", ".", false, true, true, &Functions::Sleep});
  add({"COLLECTIONS", "", false, true, false, &Functions::Collections});
  add({"NOT_NULL", ".|+", true, false, true, &Functions::NotNull});
  add({"FIRST_LIST", ".|+", true, false, true, &Functions::FirstList});
  add({"FIRST_DOCUMENT", ".|+", true, false, true, &Functions::FirstDocument});
  add({"PARSE_IDENTIFIER", ".", true, false, true, &Functions::ParseIdentifier});
  add({"IS_SAME_COLLECTION", ".h,.h", true, false, true, &Functions::IsSameCollection});
  add({"CURRENT_USER", "", false, false, false, &Functions::CurrentUser});
  add({"CURRENT_DATABASE", "", false, false, false, &Functions::CurrentDatabase});
  add({"COLLECTION_COUNT", ".h", false, true, false, &Functions::CollectionCount});
  add({"ASSERT", ".,.", false, true, true, &Functions::Assert});
  add({"WARN", ".,.", false, false, true, &Functions::Warn});
}

void AqlFunctionFeature::addStorageEngineFunctions() {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr); // Engine not loaded. Startup broken
  engine->addAqlFunctions();
}
