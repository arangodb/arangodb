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

using FF = Function::Flags;

AqlFunctionFeature* AqlFunctionFeature::AQLFUNCTIONS = nullptr;

AqlFunctionFeature::AqlFunctionFeature(application_features::ApplicationServer& server)
    : application_features::ApplicationFeature(server, "AQLFunctions") {
  setOptional(false);
  startsAfter("V8Phase");
  startsAfter("Aql");
}

// This feature does not have any options
void AqlFunctionFeature::collectOptions(std::shared_ptr<options::ProgramOptions>) {}

// This feature does not have any options
void AqlFunctionFeature::validateOptions(std::shared_ptr<options::ProgramOptions>) {}

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
    if (it.second.implementation == nullptr) {
      builder.add(VPackValue("js"));
    } else {
      builder.add(VPackValue("cxx"));
    }
    builder.close();  // implementations
    builder.add("deterministic", VPackValue(it.second.hasFlag(FF::Deterministic)));
    builder.add("cacheable", VPackValue(it.second.hasFlag(FF::Cacheable)));
    builder.add("canRunOnDBServer", VPackValue(it.second.hasFlag(FF::CanRunOnDBServer)));
    builder.close();
  }
  builder.close();
}

bool AqlFunctionFeature::exists(std::string const& name) const {
  auto it = _functionNames.find(name);

  return it != _functionNames.end();
}

Function const* AqlFunctionFeature::byName(std::string const& name) {
  auto it = _functionNames.find(name);

  if (it == _functionNames.end()) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_NAME_UNKNOWN, name.c_str());
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
  // common flags for all these functions
  auto flags = Function::makeFlags(FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServer);

  // type check functions
  add({"IS_NULL", ".", flags, &Functions::IsNull});
  add({"IS_BOOL", ".", flags, &Functions::IsBool});
  add({"IS_NUMBER", ".", flags, &Functions::IsNumber});
  add({"IS_STRING", ".", flags, &Functions::IsString});
  add({"IS_ARRAY", ".", flags, &Functions::IsArray});
  // IS_LIST is an alias for IS_ARRAY
  addAlias("IS_LIST", "IS_ARRAY");
  add({"IS_OBJECT", ".", flags, &Functions::IsObject});
  // IS_DOCUMENT is an alias for IS_OBJECT
  addAlias("IS_DOCUMENT", "IS_OBJECT");

  add({"IS_DATESTRING", ".", flags, &Functions::IsDatestring});
  add({"IS_KEY", ".", flags, &Functions::IsKey});
  add({"TYPENAME", ".", flags, &Functions::Typename});
}

void AqlFunctionFeature::addTypeCastFunctions() {
  // common flags for all these functions
  auto flags = Function::makeFlags(FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServer);

  // type cast functions
  add({"TO_NUMBER", ".", flags, &Functions::ToNumber});
  add({"TO_STRING", ".", flags, &Functions::ToString});
  add({"TO_BOOL", ".", flags, &Functions::ToBool});
  add({"TO_ARRAY", ".", flags, &Functions::ToArray});
  // TO_LIST is an alias for TO_ARRAY
  addAlias("TO_LIST", "TO_ARRAY");
}

void AqlFunctionFeature::addStringFunctions() {
  // common flags for all these functions
  auto flags = Function::makeFlags(FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServer);

  // string functions
  add({"CONCAT", ".|+", flags, &Functions::Concat});
  add({"CONCAT_SEPARATOR", ".,.|+", flags, &Functions::ConcatSeparator});
  add({"CHAR_LENGTH", ".", flags, &Functions::CharLength});
  add({"LOWER", ".", flags, &Functions::Lower});
  add({"UPPER", ".", flags, &Functions::Upper});
  add({"SUBSTRING", ".,.|.", flags, &Functions::Substring});
  add({"CONTAINS", ".,.|.", flags, &Functions::Contains});
  add({"LIKE", ".,.|.", flags, &Functions::Like});
  add({"REGEX_MATCHES", ".,.|.", flags, &Functions::RegexMatches});
  add({"REGEX_SPLIT", ".,.|.,.", flags, &Functions::RegexSplit});
  add({"REGEX_TEST", ".,.|.", flags, &Functions::RegexTest});
  add({"REGEX_REPLACE", ".,.,.|.", flags, &Functions::RegexReplace});
  add({"LEFT", ".,.", flags, &Functions::Left});
  add({"RIGHT", ".,.", flags, &Functions::Right});
  add({"TRIM", ".|.", flags, &Functions::Trim});
  add({"LTRIM", ".|.", flags, &Functions::LTrim});
  add({"RTRIM", ".|.", flags, &Functions::RTrim});
  add({"FIND_FIRST", ".,.|.,.", flags, &Functions::FindFirst});
  add({"FIND_LAST", ".,.|.,.", flags, &Functions::FindLast});
  add({"SPLIT", ".|.,.", flags, &Functions::Split});
  add({"SUBSTITUTE", ".,.|.,.", flags, &Functions::Substitute});
  add({"MD5", ".", flags, &Functions::Md5});
  add({"SHA1", ".", flags, &Functions::Sha1});
  add({"SHA512", ".", flags, &Functions::Sha512});
  add({"HASH", ".", flags, &Functions::Hash});
  add({"TO_BASE64", ".", flags, &Functions::ToBase64});
  add({"TO_HEX", ".", flags, &Functions::ToHex});
  add({"ENCODE_URI_COMPONENT", ".", flags, &Functions::EncodeURIComponent});
  add({"SOUNDEX", ".", flags, &Functions::Soundex});
  add({"LEVENSHTEIN_DISTANCE", ".,.", flags, &Functions::LevenshteinDistance});

  // special flags:
  add({"RANDOM_TOKEN", ".", Function::makeFlags(FF::CanRunOnDBServer),
       &Functions::RandomToken});  // not deterministic and not cacheable
  add({"UUID", "", Function::makeFlags(FF::CanRunOnDBServer), &Functions::Uuid});  // not deterministic and not cacheable
}

void AqlFunctionFeature::addNumericFunctions() {
  // common flags for all these functions
  auto flags = Function::makeFlags(FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServer);

  // numeric functions
  add({"FLOOR", ".", flags, &Functions::Floor});
  add({"CEIL", ".", flags, &Functions::Ceil});
  add({"ROUND", ".", flags, &Functions::Round});
  add({"ABS", ".", flags, &Functions::Abs});
  add({"SQRT", ".", flags, &Functions::Sqrt});
  add({"POW", ".,.", flags, &Functions::Pow});
  add({"LOG", ".", flags, &Functions::Log});
  add({"LOG2", ".", flags, &Functions::Log2});
  add({"LOG10", ".", flags, &Functions::Log10});
  add({"EXP", ".", flags, &Functions::Exp});
  add({"EXP2", ".", flags, &Functions::Exp2});
  add({"SIN", ".", flags, &Functions::Sin});
  add({"COS", ".", flags, &Functions::Cos});
  add({"TAN", ".", flags, &Functions::Tan});
  add({"ASIN", ".", flags, &Functions::Asin});
  add({"ACOS", ".", flags, &Functions::Acos});
  add({"ATAN", ".", flags, &Functions::Atan});
  add({"ATAN2", ".,.", flags, &Functions::Atan2});
  add({"RADIANS", ".", flags, &Functions::Radians});
  add({"DEGREES", ".", flags, &Functions::Degrees});
  add({"PI", "", flags, &Functions::Pi});

  // special flags:
  add({"RAND", "", Function::makeFlags(FF::CanRunOnDBServer), &Functions::Rand});  // not deterministic and not cacheable
}

void AqlFunctionFeature::addListFunctions() {
  // common flags for all these functions
  auto flags = Function::makeFlags(FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServer);

  // list functions
  add({"RANGE", ".,.|.", flags, &Functions::Range});
  add({"UNION", ".,.|+", flags, &Functions::Union});
  add({"UNION_DISTINCT", ".,.|+", flags, &Functions::UnionDistinct});
  add({"MINUS", ".,.|+", flags, &Functions::Minus});
  add({"OUTERSECTION", ".,.|+", flags, &Functions::Outersection});
  add({"INTERSECTION", ".,.|+", flags, &Functions::Intersection});
  add({"FLATTEN", ".|.", flags, &Functions::Flatten});
  add({"LENGTH", ".", flags, &Functions::Length});
  // COUNT is an alias for LENGTH
  addAlias("COUNT", "LENGTH");
  add({"MIN", ".", flags, &Functions::Min});
  add({"MAX", ".", flags, &Functions::Max});
  add({"SUM", ".", flags, &Functions::Sum});
  add({"MEDIAN", ".", flags, &Functions::Median});
  add({"PERCENTILE", ".,.|.", flags, &Functions::Percentile});
  add({"AVERAGE", ".", flags, &Functions::Average});
  // AVG is an alias for AVERAGE
  addAlias("AVG", "AVERAGE");
  add({"VARIANCE_SAMPLE", ".", flags, &Functions::VarianceSample});
  add({"VARIANCE_POPULATION", ".", flags, &Functions::VariancePopulation});
  // VARIANCE is an alias for VARIANCE_POPULATION
  addAlias("VARIANCE", "VARIANCE_POPULATION");
  add({"STDDEV_SAMPLE", ".", flags, &Functions::StdDevSample});
  add({"STDDEV_POPULATION", ".", flags, &Functions::StdDevPopulation});
  // STDDEV is an alias for STDDEV_POPULATION
  addAlias("STDDEV", "STDDEV_POPULATION");
  add({"COUNT_DISTINCT", ".", flags, &Functions::CountDistinct});
  // COUNT_UNIQUE is an alias for COUNT_DISTINCT
  addAlias("COUNT_UNIQUE", "COUNT_DISTINCT");
  add({"UNIQUE", ".", flags, &Functions::Unique});
  add({"SORTED_UNIQUE", ".", flags, &Functions::SortedUnique});
  add({"SORTED", ".", flags, &Functions::Sorted});
  add({"SLICE", ".,.|.", flags, &Functions::Slice});
  add({"REVERSE", ".", flags, &Functions::Reverse});
  add({"FIRST", ".", flags, &Functions::First});
  add({"LAST", ".", flags, &Functions::Last});
  add({"NTH", ".,.", flags, &Functions::Nth});
  add({"POSITION", ".,.|.", flags, &Functions::Position});
  // CONTAINS_ARRAY is an alias for POSITION
  addAlias("CONTAINS_ARRAY", "POSITION");
  add({"PUSH", ".,.|.", flags, &Functions::Push});
  add({"APPEND", ".,.|.", flags, &Functions::Append});
  add({"POP", ".", flags, &Functions::Pop});
  add({"SHIFT", ".", flags, &Functions::Shift});
  add({"UNSHIFT", ".,.|.", flags, &Functions::Unshift});
  add({"REMOVE_VALUE", ".,.|.", flags, &Functions::RemoveValue});
  add({"REMOVE_VALUES", ".,.", flags, &Functions::RemoveValues});
  add({"REMOVE_NTH", ".,.", flags, &Functions::RemoveNth});

  // special flags:
  // CALL and APPLY will always run on the coordinator and are not deterministic
  // and not cacheable, as we don't know what function is actually gonna be
  // called
  add({"CALL", ".|.+", Function::makeFlags(), &Functions::Call});
  add({"APPLY", ".|.", Function::makeFlags(), &Functions::Apply});
}

void AqlFunctionFeature::addDocumentFunctions() {
  // common flags for all these functions
  auto flags = Function::makeFlags(FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServer);

  // document functions
  add({"HAS", ".,.", flags, &Functions::Has});
  add({"ATTRIBUTES", ".|.,.", flags, &Functions::Attributes});
  // KEYS is an alias for ATTRIBUTES
  addAlias("KEYS", "ATTRIBUTES");
  add({"VALUES", ".|.", flags, &Functions::Values});
  add({"MERGE", ".|+", flags, &Functions::Merge});
  add({"MERGE_RECURSIVE", ".,.|+", flags, &Functions::MergeRecursive});
  add({"MATCHES", ".,.|.", flags, &Functions::Matches});
  add({"UNSET", ".,.|+", flags, &Functions::Unset});
  add({"UNSET_RECURSIVE", ".,.|+", flags, &Functions::UnsetRecursive});
  add({"KEEP", ".,.|+", flags, &Functions::Keep});
  add({"TRANSLATE", ".,.|.", flags, &Functions::Translate});
  add({"ZIP", ".,.", flags, &Functions::Zip});
  add({"JSON_STRINGIFY", ".", flags, &Functions::JsonStringify});
  add({"JSON_PARSE", ".", flags, &Functions::JsonParse});

  // special flags:
  add({"DOCUMENT", "h.|.", Function::makeFlags(), &Functions::Document});  // not deterministic and non-cacheable
}

void AqlFunctionFeature::addGeoFunctions() {
  // geo functions
  add({"DISTANCE", ".,.,.,.",
       Function::makeFlags(FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServer),
       &Functions::Distance});
  add({"IS_IN_POLYGON", ".,.|.",
       Function::makeFlags(FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServer),
       &Functions::IsInPolygon});
  add({"GEO_DISTANCE", ".,.",
       Function::makeFlags(FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServer),
       &Functions::GeoDistance});
  add({"GEO_CONTAINS", ".,.",
       Function::makeFlags(FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServer),
       &Functions::GeoContains});
  add({"GEO_INTERSECTS", ".,.",
       Function::makeFlags(FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServer),
       &Functions::GeoIntersects});
  add({"GEO_EQUALS", ".,.",
       Function::makeFlags(FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServer),
       &Functions::GeoEquals});
}

void AqlFunctionFeature::addGeometryConstructors() {
  // common flags for all these functions
  auto flags = Function::makeFlags(FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServer);

  // geometry types
  add({"GEO_POINT", ".,.", flags, &Functions::GeoPoint});
  add({"GEO_MULTIPOINT", ".", flags, &Functions::GeoMultiPoint});
  add({"GEO_POLYGON", ".", flags, &Functions::GeoPolygon});
  add({"GEO_MULTIPOLYGON", ".", flags, &Functions::GeoMultiPolygon});
  add({"GEO_LINESTRING", ".", flags, &Functions::GeoLinestring});
  add({"GEO_MULTILINESTRING", ".", flags, &Functions::GeoMultiLinestring});
}

void AqlFunctionFeature::addDateFunctions() {
  // common flags for all these functions
  auto flags = Function::makeFlags(FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServer);

  // date functions
  add({"DATE_TIMESTAMP", ".|.,.,.,.,.,.", flags, &Functions::DateTimestamp});
  add({"DATE_ISO8601", ".|.,.,.,.,.,.", flags, &Functions::DateIso8601});
  add({"DATE_DAYOFWEEK", ".", flags, &Functions::DateDayOfWeek});
  add({"DATE_YEAR", ".", flags, &Functions::DateYear});
  add({"DATE_MONTH", ".", flags, &Functions::DateMonth});
  add({"DATE_DAY", ".", flags, &Functions::DateDay});
  add({"DATE_HOUR", ".", flags, &Functions::DateHour});
  add({"DATE_MINUTE", ".", flags, &Functions::DateMinute});
  add({"DATE_SECOND", ".", flags, &Functions::DateSecond});
  add({"DATE_MILLISECOND", ".", flags, &Functions::DateMillisecond});
  add({"DATE_DAYOFYEAR", ".", flags, &Functions::DateDayOfYear});
  add({"DATE_ISOWEEK", ".", flags, &Functions::DateIsoWeek});
  add({"DATE_LEAPYEAR", ".", flags, &Functions::DateLeapYear});
  add({"DATE_QUARTER", ".", flags, &Functions::DateQuarter});
  add({"DATE_DAYS_IN_MONTH", ".", flags, &Functions::DateDaysInMonth});
  add({"DATE_ADD", ".,.|.", flags, &Functions::DateAdd});
  add({"DATE_SUBTRACT", ".,.|.", flags, &Functions::DateSubtract});
  add({"DATE_DIFF", ".,.,.|.", flags, &Functions::DateDiff});
  add({"DATE_COMPARE", ".,.,.|.", flags, &Functions::DateCompare});
  add({"DATE_FORMAT", ".,.", flags, &Functions::DateFormat});
  add({"DATE_TRUNC", ".,.", flags, &Functions::DateTrunc});

  // special flags:
  add({"DATE_NOW", "", Function::makeFlags(FF::Deterministic, FF::CanRunOnDBServer),
       &Functions::DateNow});  // deterministic, but not cacheable!
}

void AqlFunctionFeature::addMiscFunctions() {
  // common flags for most of these functions
  auto flags = Function::makeFlags(FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServer);

  // misc functions
  add({"PASSTHRU", ".", flags, &Functions::Passthru});
  add({"NOT_NULL", ".|+", flags, &Functions::NotNull});
  add({"FIRST_LIST", ".|+", flags, &Functions::FirstList});
  add({"FIRST_DOCUMENT", ".|+", flags, &Functions::FirstDocument});
  add({"PARSE_IDENTIFIER", ".", flags, &Functions::ParseIdentifier});
  add({"IS_SAME_COLLECTION", ".h,.h", flags, &Functions::IsSameCollection});
  add({"V8", ".", Function::makeFlags(FF::Deterministic, FF::Cacheable)});  // only native function without a
                                                                            // C++ implementation

  // special flags:
  add({"VERSION", "", Function::makeFlags(FF::Deterministic), &Functions::Version});  // deterministic, not cacheable. only on
                                                                                      // coordinator
  add({"FAIL", "|.", Function::makeFlags(FF::CanRunOnDBServer), &Functions::Fail});  // not deterministic and not cacheable
  add({"NOOPT", ".", Function::makeFlags(FF::CanRunOnDBServer), &Functions::Passthru});  // prevents all optimizations!
  add({"SLEEP", ".", Function::makeFlags(FF::CanRunOnDBServer), &Functions::Sleep});  // not deterministic and not cacheable
  add({"COLLECTIONS", "", Function::makeFlags(), &Functions::Collections});  // not deterministic and not cacheable
  add({"CURRENT_USER", "", Function::makeFlags(FF::Deterministic),
       &Functions::CurrentUser});  // deterministic, but not cacheable
  add({"CURRENT_DATABASE", "", Function::makeFlags(FF::Deterministic),
       &Functions::CurrentDatabase});  // deterministic, but not cacheable
  add({"COLLECTION_COUNT", ".h", Function::makeFlags(), &Functions::CollectionCount});  // not deterministic and not cacheable
  add({"PREGEL_RESULT", ".", Function::makeFlags(FF::CanRunOnDBServer),
       &Functions::PregelResult});  // not deterministic and not cacheable
  add({"ASSERT", ".,.", Function::makeFlags(FF::CanRunOnDBServer), &Functions::Assert});  // not deterministic and not cacheable
  add({"WARN", ".,.", Function::makeFlags(FF::CanRunOnDBServer), &Functions::Warn});  // not deterministic and not cacheable

  // NEAR, WITHIN, WITHIN_RECTANGLE and FULLTEXT are replaced by the AQL
  // optimizer with collection-based subqueries they are all not marked as
  // non-deterministic and non-cacheable here as they refer to documents
  add({"NEAR", ".h,.,.|.,.", Function::makeFlags(), &Functions::NotImplemented});
  add({"WITHIN", ".h,.,.,.|.", Function::makeFlags(), &Functions::NotImplemented});
  add({"WITHIN_RECTANGLE", "h.,.,.,.,.", Function::makeFlags(), &Functions::NotImplemented});
  add({"FULLTEXT", ".h,.,.|.", Function::makeFlags(), &Functions::NotImplemented});
}

}  // namespace aql
}  // namespace arangodb
