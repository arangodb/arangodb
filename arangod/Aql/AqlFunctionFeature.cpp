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

using namespace arangodb::application_features;

namespace arangodb {
namespace aql {

AqlFunctionFeature* AqlFunctionFeature::AQLFUNCTIONS = nullptr;

AqlFunctionFeature::AqlFunctionFeature(
    application_features::ApplicationServer& server
)
    : application_features::ApplicationFeature(server, "AQLFunctions") {
  setOptional(false);
  startsAfter("V8Phase");

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

  add({"PREGEL_RESULT", ".", false, true, &Functions::PregelResult});
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
    builder.add("canThrow", VPackValue(false));
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
  add({"IS_NULL", ".", true, true, &Functions::IsNull});
  add({"IS_BOOL", ".", true, true, &Functions::IsBool});
  add({"IS_NUMBER", ".", true, true, &Functions::IsNumber});
  add({"IS_STRING", ".", true, true, &Functions::IsString});
  add({"IS_ARRAY", ".", true, true, &Functions::IsArray});
  // IS_LIST is an alias for IS_ARRAY
  addAlias("IS_LIST", "IS_ARRAY");
  add({"IS_OBJECT", ".", true, true, &Functions::IsObject});
  // IS_DOCUMENT is an alias for IS_OBJECT
  addAlias("IS_DOCUMENT", "IS_OBJECT");

  add({"IS_DATESTRING", ".", true, true, &Functions::IsDatestring});
  add({"IS_KEY", ".", true, true, &Functions::IsKey});
  add({"TYPENAME", ".", true, true, &Functions::Typename});
}

void AqlFunctionFeature::addTypeCastFunctions() {
  // type cast functions
  add({"TO_NUMBER", ".", true, true, &Functions::ToNumber});
  add({"TO_STRING", ".", true, true, &Functions::ToString});
  add({"TO_BOOL", ".", true, true, &Functions::ToBool});
  add({"TO_ARRAY", ".", true, true, &Functions::ToArray});
  // TO_LIST is an alias for TO_ARRAY
  addAlias("TO_LIST", "TO_ARRAY");
}

void AqlFunctionFeature::addStringFunctions() {
  // string functions
  add({"CONCAT", ".|+", true, true, &Functions::Concat});
  add({"CONCAT_SEPARATOR", ".,.|+", true, true, &Functions::ConcatSeparator});
  add({"CHAR_LENGTH", ".", true, true, &Functions::CharLength});
  add({"LOWER", ".", true, true, &Functions::Lower});
  add({"UPPER", ".", true, true, &Functions::Upper});
  add({"SUBSTRING", ".,.|.", true, true, &Functions::Substring});
  add({"CONTAINS", ".,.|.", true, true, &Functions::Contains});
  add({"LIKE", ".,.|.", true, true, &Functions::Like});
  add({"REGEX_SPLIT", ".,.|.,.", true, true, &Functions::RegexSplit});
  add({"REGEX_TEST", ".,.|.", true, true, &Functions::RegexTest});
  add({"REGEX_REPLACE", ".,.,.|.", true, true, &Functions::RegexReplace});
  add({"LEFT", ".,.", true, true, &Functions::Left});
  add({"RIGHT", ".,.", true, true, &Functions::Right});
  add({"TRIM", ".|.", true, true, &Functions::Trim});
  add({"LTRIM", ".|.", true, true, &Functions::LTrim});
  add({"RTRIM", ".|.", true, true, &Functions::RTrim});
  add({"FIND_FIRST", ".,.|.,.", true, true, &Functions::FindFirst});
  add({"FIND_LAST", ".,.|.,.", true, true, &Functions::FindLast});
  add({"SPLIT", ".|.,.", true, true, &Functions::Split});
  add({"SUBSTITUTE", ".,.|.,.", true, true, &Functions::Substitute});
  add({"MD5", ".", true, true, &Functions::Md5});
  add({"SHA1", ".", true, true, &Functions::Sha1});
  add({"SHA512", ".", true, true, &Functions::Sha512});
  add({"HASH", ".", true, true, &Functions::Hash});
  add({"RANDOM_TOKEN", ".", false, true, &Functions::RandomToken});
  add({"TO_BASE64", ".", true, true, &Functions::ToBase64});
  add({"TO_HEX", ".", true, true, &Functions::ToHex});
  add({"ENCODE_URI_COMPONENT", ".", true, true, &Functions::EncodeURIComponent});
  add({"UUID", "", true, true, &Functions::UUID});
  add({"SOUNDEX", ".", true, true, &Functions::Soundex});
  add({"LEVENSHTEIN_DISTANCE", ".,.", true, true, &Functions::LevenshteinDistance});
  // FULLTEXT is replaced by the AQL optimizer with an index lookup
  add({"FULLTEXT", ".h,.,.|." , false, false, &Functions::NotImplemented});
}

void AqlFunctionFeature::addNumericFunctions() {
  // numeric functions
  add({"FLOOR", ".", true, true, &Functions::Floor});
  add({"CEIL", ".", true, true, &Functions::Ceil});
  add({"ROUND", ".", true, true, &Functions::Round});
  add({"ABS", ".", true, true, &Functions::Abs});
  add({"RAND", "", false, true, &Functions::Rand});
  add({"SQRT", ".", true, true, &Functions::Sqrt});
  add({"POW", ".,.", true, true, &Functions::Pow});
  add({"LOG", ".", true, true, &Functions::Log});
  add({"LOG2", ".", true, true, &Functions::Log2});
  add({"LOG10", ".", true, true, &Functions::Log10});
  add({"EXP", ".", true, true, &Functions::Exp});
  add({"EXP2", ".", true, true, &Functions::Exp2});
  add({"SIN", ".", true, true, &Functions::Sin});
  add({"COS", ".", true, true, &Functions::Cos});
  add({"TAN", ".", true, true, &Functions::Tan});
  add({"ASIN", ".", true, true, &Functions::Asin});
  add({"ACOS", ".", true, true, &Functions::Acos});
  add({"ATAN", ".", true, true, &Functions::Atan});
  add({"ATAN2", ".,.", true, true, &Functions::Atan2});
  add({"RADIANS", ".", true, true, &Functions::Radians});
  add({"DEGREES", ".", true, true, &Functions::Degrees});
  add({"PI", "", true, true, &Functions::Pi});
}

void AqlFunctionFeature::addListFunctions() {
  // list functions
  add({"RANGE", ".,.|.", true, true, &Functions::Range});
  add({"UNION", ".,.|+", true, true, &Functions::Union});
  add({"UNION_DISTINCT", ".,.|+", true, true, &Functions::UnionDistinct});
  add({"MINUS", ".,.|+", true, true, &Functions::Minus});
  add({"OUTERSECTION", ".,.|+", true, true, &Functions::Outersection});
  add({"INTERSECTION", ".,.|+", true, true, &Functions::Intersection});
  add({"FLATTEN", ".|.", true, true, &Functions::Flatten});
  add({"LENGTH", ".", true, true, &Functions::Length});
  addAlias("COUNT", "LENGTH");
  add({"MIN", ".", true, true, &Functions::Min});
  add({"MAX", ".", true, true, &Functions::Max});
  add({"SUM", ".", true, true, &Functions::Sum});
  add({"MEDIAN", ".", true, true, &Functions::Median});
  add({"PERCENTILE", ".,.|.", true, true, &Functions::Percentile});
  add({"AVERAGE", ".", true, true, &Functions::Average});
  addAlias("AVG", "AVERAGE");
  add({"VARIANCE_SAMPLE", ".", true, true, &Functions::VarianceSample});
  add({"VARIANCE_POPULATION", ".", true, true, &Functions::VariancePopulation});
  addAlias("VARIANCE", "VARIANCE_POPULATION");
  add({"STDDEV_SAMPLE", ".", true, true, &Functions::StdDevSample});
  add({"STDDEV_POPULATION", ".", true, true, &Functions::StdDevPopulation});
  addAlias("STDDEV", "STDDEV_POPULATION");
  add({"COUNT_DISTINCT", ".", true, true, &Functions::CountDistinct});
  addAlias("COUNT_UNIQUE", "COUNT_DISTINCT");
  add({"UNIQUE", ".", true, true, &Functions::Unique});
  add({"SORTED_UNIQUE", ".", true, true, &Functions::SortedUnique});
  add({"SORTED", ".", true, true, &Functions::Sorted});
  add({"SLICE", ".,.|.", true, true, &Functions::Slice});
  add({"REVERSE", ".", true, true, &Functions::Reverse});
  add({"FIRST", ".", true, true, &Functions::First});
  add({"LAST", ".", true, true, &Functions::Last});
  add({"NTH", ".,.", true, true, &Functions::Nth});
  add({"POSITION", ".,.|.", true, true, &Functions::Position});
  add({"CALL", ".|.+", false, false, &Functions::Call});
  add({"APPLY", ".|.", false, false, &Functions::Apply});
  add({"PUSH", ".,.|.", true, true, &Functions::Push});
  add({"APPEND", ".,.|.", true, true, &Functions::Append});
  add({"POP", ".", true, true, &Functions::Pop});
  add({"SHIFT", ".", true, true, &Functions::Shift});
  add({"UNSHIFT", ".,.|.", true, true, &Functions::Unshift});
  add({"REMOVE_VALUE", ".,.|.", true, true, &Functions::RemoveValue});
  add({"REMOVE_VALUES", ".,.", true, true, &Functions::RemoveValues});
  add({"REMOVE_NTH", ".,.", true, true, &Functions::RemoveNth});
}

void AqlFunctionFeature::addDocumentFunctions() {
  // document functions
  add({"HAS", ".,.", true, true, &Functions::Has});
  add({"ATTRIBUTES", ".|.,.", true, true, &Functions::Attributes});
  add({"VALUES", ".|.", true, true, &Functions::Values});
  add({"MERGE", ".|+", true, true, &Functions::Merge});
  add({"MERGE_RECURSIVE", ".,.|+", true, true, &Functions::MergeRecursive});
  add({"DOCUMENT", "h.|.", false, false, &Functions::Document});
  add({"MATCHES", ".,.|.", true, true, &Functions::Matches});
  add({"UNSET", ".,.|+", true, true, &Functions::Unset});
  add({"UNSET_RECURSIVE", ".,.|+", true, true, &Functions::UnsetRecursive});
  add({"KEEP", ".,.|+", true, true, &Functions::Keep});
  add({"TRANSLATE", ".,.|.", true, true, &Functions::Translate});
  add({"ZIP", ".,.", true, true, &Functions::Zip});
  add({"JSON_STRINGIFY", ".", true, true, &Functions::JsonStringify});
  add({"JSON_PARSE", ".", true, true, &Functions::JsonParse});
}

void AqlFunctionFeature::addGeoFunctions() {
  // geo functions
  add({"DISTANCE", ".,.,.,.", true, true, &Functions::Distance});
  add({"IS_IN_POLYGON", ".,.|.", true, true, &Functions::IsInPolygon});
  add({"GEO_DISTANCE", ".,.", true, true, &Functions::GeoDistance});
  add({"GEO_CONTAINS", ".,.", true, true, &Functions::GeoContains});
  add({"GEO_INTERSECTS", ".,.", true, true, &Functions::GeoIntersects});
  add({"GEO_EQUALS", ".,.", true, true, &Functions::GeoEquals});
  // NEAR and WITHIN are replaced by the AQL optimizer with collection-based subqueries
  add({"NEAR", ".h,.,.|.,.", false, false, &Functions::NotImplemented});
  add({"WITHIN", ".h,.,.,.|.", false, false, &Functions::NotImplemented});
  add({"WITHIN_RECTANGLE", "h.,.,.,.,.", false, false, &Functions::NotImplemented });
}

void AqlFunctionFeature::addGeometryConstructors() {
  // geometry types
  add({"GEO_POINT", ".,.", true, true, &Functions::GeoPoint});
  add({"GEO_MULTIPOINT", ".", true, true, &Functions::GeoMultiPoint});
  add({"GEO_POLYGON", ".", true, true, &Functions::GeoPolygon});
  add({"GEO_LINESTRING", ".", true, true, &Functions::GeoLinestring});
  add({"GEO_MULTILINESTRING", ".", true, true, &Functions::GeoMultiLinestring});
}

void AqlFunctionFeature::addDateFunctions() {
  // date functions
  add({"DATE_NOW", "", false, true, &Functions::DateNow});
  add({"DATE_TIMESTAMP", ".|.,.,.,.,.,.", true, true, &Functions::DateTimestamp});
  add({"DATE_ISO8601", ".|.,.,.,.,.,.", true, true, &Functions::DateIso8601});
  add({"DATE_DAYOFWEEK", ".", true, true, &Functions::DateDayOfWeek});
  add({"DATE_YEAR", ".", true, true, &Functions::DateYear});
  add({"DATE_MONTH", ".", true, true, &Functions::DateMonth});
  add({"DATE_DAY", ".", true, true, &Functions::DateDay});
  add({"DATE_HOUR", ".", true, true, &Functions::DateHour});
  add({"DATE_MINUTE", ".", true, true, &Functions::DateMinute});
  add({"DATE_SECOND", ".", true, true, &Functions::DateSecond});
  add({"DATE_MILLISECOND", ".", true, true, &Functions::DateMillisecond});
  add({"DATE_DAYOFYEAR", ".", true, true, &Functions::DateDayOfYear});
  add({"DATE_ISOWEEK", ".", true, true, &Functions::DateIsoWeek});
  add({"DATE_LEAPYEAR", ".", true, true, &Functions::DateLeapYear});
  add({"DATE_QUARTER", ".", true, true, &Functions::DateQuarter});
  add({"DATE_DAYS_IN_MONTH", ".", true, true, &Functions::DateDaysInMonth});
  add({"DATE_ADD", ".,.|.", true, true, &Functions::DateAdd});
  add({"DATE_SUBTRACT", ".,.|.", true, true, &Functions::DateSubtract});
  add({"DATE_DIFF", ".,.,.|.", true, true, &Functions::DateDiff});
  add({"DATE_COMPARE", ".,.,.|.", true, true, &Functions::DateCompare});
  add({"DATE_FORMAT", ".,.", true, true, &Functions::DateFormat});
  add({"DATE_TRUNC",   ".,.", true, true, &Functions::DateTrunc});
}

void AqlFunctionFeature::addMiscFunctions() {
  // misc functions
  add({"FAIL", "|.", false, true, &Functions::Fail});
  add({"PASSTHRU", ".", false, true, &Functions::Passthru});
  addAlias("NOOPT", "PASSTHRU");
  add({"V8", ".", true, true });
  add({"SLEEP", ".", false, true, &Functions::Sleep});
  add({"COLLECTIONS", "", false, false, &Functions::Collections});
  add({"NOT_NULL", ".|+", true, true, &Functions::NotNull});
  add({"FIRST_LIST", ".|+", true, true, &Functions::FirstList});
  add({"FIRST_DOCUMENT", ".|+", true, true, &Functions::FirstDocument});
  add({"PARSE_IDENTIFIER", ".", true, true, &Functions::ParseIdentifier});
  add({"IS_SAME_COLLECTION", ".h,.h", true, true, &Functions::IsSameCollection});
  add({"CURRENT_USER", "", false, false, &Functions::CurrentUser});
  add({"CURRENT_DATABASE", "", false, false, &Functions::CurrentDatabase});
  add({"COLLECTION_COUNT", ".h", false, false, &Functions::CollectionCount});
  add({"ASSERT", ".,.", false, true, &Functions::Assert});
  add({"WARN", ".,.", false, true, &Functions::Warn});
}

} // aql
} // arangodb
