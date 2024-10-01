////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Basics/StringUtils.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/ClusterFeaturePhase.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"

using namespace arangodb::application_features;

namespace arangodb {
namespace aql {

using FF = Function::Flags;

AqlFunctionFeature::AqlFunctionFeature(Server& server)
    : ArangodFeature{server, *this} {
  setOptional(false);
#ifdef USE_V8
  startsAfter<V8FeaturePhase>();
#else
  startsAfter<application_features::ClusterFeaturePhase>();
#endif
  startsAfter<AqlFeature>();
}

void AqlFunctionFeature::prepare() {
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

void AqlFunctionFeature::add(Function const& func) {
  TRI_ASSERT(func.name == basics::StringUtils::toupper(func.name));
  TRI_ASSERT(!_functionNames.contains(func.name));
  // add function to the map
  _functionNames.try_emplace(func.name, func);
}

void AqlFunctionFeature::addAlias(std::string const& alias,
                                  std::string const& original) {
  auto it = _functionNames.find(original);
  TRI_ASSERT(it != _functionNames.end());

  // intentionally copy original function, as we want to give it another name
  Function aliasFunction = (*it).second;
  aliasFunction.name = alias;

  add(aliasFunction);
}

void AqlFunctionFeature::toVelocyPack(VPackBuilder& builder) const {
  builder.openArray();
  for (auto const& it : _functionNames) {
    if (it.second.hasFlag(FF::Internal)) {
      // don't serialize internal functions
      continue;
    }
    it.second.toVelocyPack(builder);
  }
  builder.close();
}

bool AqlFunctionFeature::exists(std::string const& name) const {
  return _functionNames.contains(name);
}

Function const* AqlFunctionFeature::byName(std::string const& name) const {
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
// b = argument of any type. if it is a bind parameter it
//     will be marked as a required bind parameter that will
//     be expanded early in the query processing pipeline
// h = collection name, will be converted into string
// , = next argument
// | = separates mandatory from optional arguments
// + = unlimited number of optional arguments of any type

void AqlFunctionFeature::addTypeCheckFunctions() {
  // common flags for all these functions
  constexpr auto flags = Function::makeFlags(
      FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServerCluster,
      FF::CanRunOnDBServerOneShard, FF::CanUseInAnalyzer);

  // type check functions
  add({"IS_NULL", ".", flags, &functions::IsNull});
  add({"IS_BOOL", ".", flags, &functions::IsBool});
  add({"IS_NUMBER", ".", flags, &functions::IsNumber});
  add({"IS_STRING", ".", flags, &functions::IsString});
  add({"IS_ARRAY", ".", flags, &functions::IsArray});
  // IS_LIST is an alias for IS_ARRAY
  addAlias("IS_LIST", "IS_ARRAY");
  add({"IS_OBJECT", ".", flags, &functions::IsObject});
  // IS_DOCUMENT is an alias for IS_OBJECT
  addAlias("IS_DOCUMENT", "IS_OBJECT");

  add({"IS_DATESTRING", ".", flags, &functions::IsDatestring});
  add({"IS_IPV4", ".", flags, &functions::IsIpV4});
  add({"IS_KEY", ".", flags, &functions::IsKey});
  add({"TYPENAME", ".", flags, &functions::Typename});
}

void AqlFunctionFeature::addTypeCastFunctions() {
  // common flags for all these functions
  constexpr auto flags = Function::makeFlags(
      FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServerCluster,
      FF::CanRunOnDBServerOneShard, FF::CanUseInAnalyzer);

  // type cast functions
  add({"TO_NUMBER", ".", flags, &functions::ToNumber});
  add({"TO_STRING", ".", flags, &functions::ToString});
  add({"TO_BOOL", ".", flags, &functions::ToBool});
  add({"TO_ARRAY", ".", flags, &functions::ToArray});
  // TO_LIST is an alias for TO_ARRAY
  addAlias("TO_LIST", "TO_ARRAY");
}

void AqlFunctionFeature::addStringFunctions() {
  // common flags for all these functions
  constexpr auto flags = Function::makeFlags(
      FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServerCluster,
      FF::CanRunOnDBServerOneShard, FF::CanUseInAnalyzer);

  // string functions
  add({"CONCAT", ".|+", flags, &functions::Concat});
  add({"CONCAT_SEPARATOR", ".,.|+", flags, &functions::ConcatSeparator});
  add({"CHAR_LENGTH", ".", flags, &functions::CharLength});
  add({"LOWER", ".", flags, &functions::Lower});
  add({"UPPER", ".", flags, &functions::Upper});
  add({"SUBSTRING", ".,.|.", flags, &functions::Substring});
  add({"SUBSTRING_BYTES", ".,.|.,.,.", flags, &functions::SubstringBytes});
  add({"CONTAINS", ".,.|.", flags, &functions::Contains});
  add({"LIKE", ".,.|.", flags, &functions::Like});
  add({"REGEX_MATCHES", ".,.|.", flags, &functions::RegexMatches});
  add({"REGEX_SPLIT", ".,.|.,.", flags, &functions::RegexSplit});
  add({"REGEX_TEST", ".,.|.", flags, &functions::RegexTest});
  add({"REGEX_REPLACE", ".,.,.|.", flags, &functions::RegexReplace});
  add({"LEFT", ".,.", flags, &functions::Left});
  add({"RIGHT", ".,.", flags, &functions::Right});
  add({"TRIM", ".|.", flags, &functions::Trim});
  add({"LTRIM", ".|.", flags, &functions::LTrim});
  add({"RTRIM", ".|.", flags, &functions::RTrim});
  add({"FIND_FIRST", ".,.|.,.", flags, &functions::FindFirst});
  add({"FIND_LAST", ".,.|.,.", flags, &functions::FindLast});
  add({"SPLIT", ".|.,.", flags, &functions::Split});
  add({"SUBSTITUTE", ".,.|.,.", flags, &functions::Substitute});
  add({"IPV4_TO_NUMBER", ".", flags, &functions::IpV4ToNumber});
  add({"IPV4_FROM_NUMBER", ".", flags, &functions::IpV4FromNumber});
  add({"MD5", ".", flags, &functions::Md5});
  add({"SHA1", ".", flags, &functions::Sha1});
  add({"SHA256", ".", flags, &functions::Sha256});
  add({"SHA512", ".", flags, &functions::Sha512});
  add({"CRC32", ".", flags, &functions::Crc32});
  add({"FNV64", ".", flags, &functions::Fnv64});
  add({"HASH", ".", flags, &functions::Hash});
  add({"TO_BASE64", ".", flags, &functions::ToBase64});
  add({"TO_HEX", ".", flags, &functions::ToHex});
  add({"TO_CHAR", ".", flags, &functions::ToChar});
  add({"REPEAT", ".,.|.", flags, &functions::Repeat});
  add({"ENCODE_URI_COMPONENT", ".", flags, &functions::EncodeURIComponent});
  add({"SOUNDEX", ".", flags, &functions::Soundex});
  add({"LEVENSHTEIN_DISTANCE", ".,.", flags, &functions::LevenshteinDistance});

  // (attribute, target, max distance,
  // [include transpositions, max terms, prefix])
  add({"LEVENSHTEIN_MATCH", ".,.,.|.,.,.", flags,
       &functions::LevenshteinMatch});

  constexpr auto flagsNoAnalyzer = Function::makeFlags(
      FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServerCluster,
      FF::CanRunOnDBServerOneShard);

  // (attribute, target, [threshold, analyzer])
  // OR (attribute, target, [analyzer])
  add({"NGRAM_MATCH", ".,.|.,.", flagsNoAnalyzer, &functions::NgramMatch});
  // (attribute, target, ngram size)
  add({"NGRAM_SIMILARITY", ".,.,.", flags, &functions::NgramSimilarity});

  // (attribute, target, ngram size)
  add({"NGRAM_POSITIONAL_SIMILARITY", ".,.,.", flags,
       &functions::NgramPositionalSimilarity});

  // attribute, target, threshold[, analyzer]
  add({"MINHASH_MATCH", ".,.,.|.", flagsNoAnalyzer, &functions::MinHashMatch});

  // (attribute, lower, upper, include lower, include upper)
  add({"IN_RANGE", ".,.,.,.,.", flags, &functions::InRange});

  // special flags:
  // not deterministic and not cacheable
  constexpr auto nonDeterministicFlags =
      Function::makeFlags(FF::CanRunOnDBServerCluster,
                          FF::CanRunOnDBServerOneShard, FF::CanUseInAnalyzer);
  add({"RANDOM_TOKEN", ".", nonDeterministicFlags, &functions::RandomToken});
  add({"UUID", "", nonDeterministicFlags, &functions::Uuid});
}

void AqlFunctionFeature::addNumericFunctions() {
  // common flags for all these functions
  constexpr auto flags = Function::makeFlags(
      FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServerCluster,
      FF::CanRunOnDBServerOneShard, FF::CanUseInAnalyzer);

  // numeric functions
  add({"FLOOR", ".", flags, &functions::Floor});
  add({"CEIL", ".", flags, &functions::Ceil});
  add({"ROUND", ".", flags, &functions::Round});
  add({"ABS", ".", flags, &functions::Abs});
  add({"SQRT", ".", flags, &functions::Sqrt});
  add({"POW", ".,.", flags, &functions::Pow});
  add({"LOG", ".", flags, &functions::Log});
  add({"LOG2", ".", flags, &functions::Log2});
  add({"LOG10", ".", flags, &functions::Log10});
  add({"EXP", ".", flags, &functions::Exp});
  add({"EXP2", ".", flags, &functions::Exp2});
  add({"SIN", ".", flags, &functions::Sin});
  add({"COS", ".", flags, &functions::Cos});
  add({"TAN", ".", flags, &functions::Tan});
  add({"ASIN", ".", flags, &functions::Asin});
  add({"ACOS", ".", flags, &functions::Acos});
  add({"ATAN", ".", flags, &functions::Atan});
  add({"ATAN2", ".,.", flags, &functions::Atan2});
  add({"RADIANS", ".", flags, &functions::Radians});
  add({"DEGREES", ".", flags, &functions::Degrees});
  add({"PI", "", flags, &functions::Pi});

  add({"BIT_AND", ".|.", flags, &functions::BitAnd});
  add({"BIT_OR", ".|.", flags, &functions::BitOr});
  add({"BIT_XOR", ".|.", flags, &functions::BitXOr});
  add({"BIT_NEGATE", ".,.", flags, &functions::BitNegate});
  add({"BIT_TEST", ".,.", flags, &functions::BitTest});
  add({"BIT_POPCOUNT", ".", flags, &functions::BitPopcount});
  add({"BIT_SHIFT_LEFT", ".,.,.", flags, &functions::BitShiftLeft});
  add({"BIT_SHIFT_RIGHT", ".,.,.", flags, &functions::BitShiftRight});
  add({"BIT_CONSTRUCT", ".", flags, &functions::BitConstruct});
  add({"BIT_DECONSTRUCT", ".", flags, &functions::BitDeconstruct});
  add({"BIT_TO_STRING", ".,.", flags, &functions::BitToString});
  add({"BIT_FROM_STRING", ".", flags, &functions::BitFromString});
  add({"MINHASH_ERROR", ".", flags, &functions::MinHashError});
  add({"MINHASH_COUNT", ".", flags, &functions::MinHashCount});

  // special flags:
  // not deterministic and not cacheable
  constexpr auto nonDeterministicFlags =
      Function::makeFlags(FF::CanRunOnDBServerCluster,
                          FF::CanRunOnDBServerOneShard, FF::CanUseInAnalyzer);
  add({"RAND", "", nonDeterministicFlags, &functions::Rand});
  // RANDOM is an alias for RAND
  addAlias("RANDOM", "RAND");
}

void AqlFunctionFeature::addListFunctions() {
  // common flags for all these functions
  constexpr auto flags = Function::makeFlags(
      FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServerCluster,
      FF::CanRunOnDBServerOneShard, FF::CanUseInAnalyzer);

  // list functions
  add({"RANGE", ".,.|.", flags, &functions::Range});
  add({"UNION", ".,.|+", flags, &functions::Union});
  add({"UNION_DISTINCT", ".,.|+", flags, &functions::UnionDistinct});
  add({"MINUS", ".,.|+", flags, &functions::Minus});
  add({"OUTERSECTION", ".,.|+", flags, &functions::Outersection});
  add({"INTERSECTION", ".,.|+", flags, &functions::Intersection});
  add({"JACCARD", ".,.", flags, &functions::Jaccard});
  add({"FLATTEN", ".|.", flags, &functions::Flatten});
  add({"LENGTH", ".", flags, &functions::Length});
  // COUNT is an alias for LENGTH
  addAlias("COUNT", "LENGTH");
  add({"MIN", ".", flags, &functions::Min});
  add({"MAX", ".", flags, &functions::Max});
  add({"SUM", ".", flags, &functions::Sum});
  add({"MEDIAN", ".", flags, &functions::Median});
  add({"PERCENTILE", ".,.|.", flags, &functions::Percentile});
  add({"AVERAGE", ".", flags, &functions::Average});
  // AVG is an alias for AVERAGE
  addAlias("AVG", "AVERAGE");
  add({"VARIANCE_SAMPLE", ".", flags, &functions::VarianceSample});
  add({"VARIANCE_POPULATION", ".", flags, &functions::VariancePopulation});
  // VARIANCE is an alias for VARIANCE_POPULATION
  addAlias("VARIANCE", "VARIANCE_POPULATION");
  add({"STDDEV_SAMPLE", ".", flags, &functions::StdDevSample});
  add({"STDDEV_POPULATION", ".", flags, &functions::StdDevPopulation});
  // STDDEV is an alias for STDDEV_POPULATION
  addAlias("STDDEV", "STDDEV_POPULATION");
  add({"COUNT_DISTINCT", ".", flags, &functions::CountDistinct});
  // COUNT_UNIQUE is an alias for COUNT_DISTINCT
  addAlias("COUNT_UNIQUE", "COUNT_DISTINCT");
  add({"PRODUCT", ".", flags, &functions::Product});
  add({"UNIQUE", ".", flags, &functions::Unique});
  add({"SORTED_UNIQUE", ".", flags, &functions::SortedUnique});
  add({"SORTED", ".", flags, &functions::Sorted});
  add({"SLICE", ".,.|.", flags, &functions::Slice});
  add({"REVERSE", ".", flags, &functions::Reverse});
  add({"FIRST", ".", flags, &functions::First});
  add({"LAST", ".", flags, &functions::Last});
  add({"NTH", ".,.", flags, &functions::Nth});
  add({"POSITION", ".,.|.", flags, &functions::Position});
  // CONTAINS_ARRAY is an alias for POSITION
  addAlias("CONTAINS_ARRAY", "POSITION");
  add({"PUSH", ".,.|.", flags, &functions::Push});
  add({"APPEND", ".,.|.", flags, &functions::Append});
  add({"POP", ".", flags, &functions::Pop});
  add({"SHIFT", ".", flags, &functions::Shift});
  add({"UNSHIFT", ".,.|.", flags, &functions::Unshift});
  add({"REMOVE_VALUE", ".,.|.", flags, &functions::RemoveValue});
  add({"REMOVE_VALUES", ".,.", flags, &functions::RemoveValues});
  add({"REMOVE_NTH", ".,.", flags, &functions::RemoveNth});
  add({"REPLACE_NTH", ".,.,.|.", flags, &functions::ReplaceNth});
  add({"INTERLEAVE", ".,.|+", flags, &functions::Interleave});

  add({"DECAY_GAUSS", ".,.,.,.,.,", flags, &functions::DecayGauss});
  add({"DECAY_EXP", ".,.,.,.,.,", flags, &functions::DecayExp});
  add({"DECAY_LINEAR", ".,.,.,.,.,", flags, &functions::DecayLinear});
  // Array, NumHashes
  add({"MINHASH", ".,.", flags, &functions::MinHash});

  add({"COSINE_SIMILARITY", ".,.", flags, &functions::CosineSimilarity});
  add({"L1_DISTANCE", ".,.", flags, &functions::L1Distance});
  add({"L2_DISTANCE", ".,.", flags, &functions::L2Distance});
  // special flags:
  // CALL and APPLY will always run on the coordinator and are not deterministic
  // and not cacheable, as we don't know what function is actually gonna be
  // called. in addition, this may call any user-defined function, so we cannot
  // run this on DB servers
  add({"CALL", ".|.+", Function::makeFlags(), &functions::Call});
  add({"APPLY", ".|.", Function::makeFlags(), &functions::Apply});
}

void AqlFunctionFeature::addDocumentFunctions() {
  // common flags for all these functions
  constexpr auto flags = Function::makeFlags(
      FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServerCluster,
      FF::CanRunOnDBServerOneShard, FF::CanUseInAnalyzer);

  // document functions
  add({"HAS", ".,.", flags, &functions::Has});
  add({"ATTRIBUTES", ".|.,.", flags, &functions::Attributes});
  // KEYS is an alias for ATTRIBUTES
  addAlias("KEYS", "ATTRIBUTES");
  add({"VALUES", ".|.", flags, &functions::Values});
  add({"VALUE", ".,.", flags, &functions::Value});
  add({"MERGE", ".|+", flags, &functions::Merge});
  add({"MERGE_RECURSIVE", ".|+", flags, &functions::MergeRecursive});
  add({"MATCHES", ".,.|.", flags, &functions::Matches});
  add({"UNSET", ".,.|+", flags, &functions::Unset});
  add({"UNSET_RECURSIVE", ".,.|+", flags, &functions::UnsetRecursive});
  add({"KEEP", ".,.|+", flags, &functions::Keep});
  add({"KEEP_RECURSIVE", ".,.|+", flags, &functions::KeepRecursive});
  add({"TRANSLATE", ".,.|.", flags, &functions::Translate});
  add({"ZIP", ".,.", flags, &functions::Zip});
  add({"ENTRIES", ".", flags, &functions::Entries});
  add({"JSON_STRINGIFY", ".", flags, &functions::JsonStringify});
  add({"JSON_PARSE", ".", flags, &functions::JsonParse});

  // special flags:
  // not deterministic and non-cacheable, can only run on DB servers in OneShard
  // mode. not usable in analyzers
  constexpr auto documentFlags =
      Function::makeFlags(FF::CanRunOnDBServerOneShard, FF::CanReadDocuments);
  add({"DOCUMENT", "h.|.", documentFlags, &functions::Document});
}

void AqlFunctionFeature::addGeoFunctions() {
  // common flags for all these functions
  constexpr auto flags = Function::makeFlags(
      FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServerCluster,
      FF::CanRunOnDBServerOneShard, FF::CanUseInAnalyzer);

  // geo functions
  add({"DISTANCE", ".,.,.,.", flags, &functions::Distance});
  add({"IS_IN_POLYGON", ".,.|.", flags, &functions::IsInPolygon});
  add({"GEO_DISTANCE", ".,.|.", flags, &functions::GeoDistance});
  add({"GEO_CONTAINS", ".,.", flags, &functions::GeoContains});
  add({"GEO_INTERSECTS", ".,.", flags, &functions::GeoIntersects});
  add({"GEO_EQUALS", ".,.", flags, &functions::GeoEquals});
  add({"GEO_AREA", ".|.", flags, &functions::GeoArea});

  // (point0, point1, lower, upper[, includeLower = true, includeUpper = true,
  // ellipsoid = "shpere"])
  add({"GEO_IN_RANGE", ".,.,.,.|.,.,.", flags, &functions::GeoInRange});
}

void AqlFunctionFeature::addGeometryConstructors() {
  // common flags for all these functions
  constexpr auto flags = Function::makeFlags(
      FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServerCluster,
      FF::CanRunOnDBServerOneShard, FF::CanUseInAnalyzer);

  // geometry types
  add({"GEO_POINT", ".,.", flags, &functions::GeoPoint});
  add({"GEO_MULTIPOINT", ".", flags, &functions::GeoMultiPoint});
  add({"GEO_POLYGON", ".", flags, &functions::GeoPolygon});
  add({"GEO_MULTIPOLYGON", ".", flags, &functions::GeoMultiPolygon});
  add({"GEO_LINESTRING", ".", flags, &functions::GeoLinestring});
  add({"GEO_MULTILINESTRING", ".", flags, &functions::GeoMultiLinestring});
}

void AqlFunctionFeature::addDateFunctions() {
  // common flags for all these functions
  constexpr auto flags = Function::makeFlags(
      FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServerCluster,
      FF::CanRunOnDBServerOneShard, FF::CanUseInAnalyzer);

  // date functions
  add({"DATE_TIMESTAMP", ".|.,.,.,.,.,.", flags, &functions::DateTimestamp});
  add({"DATE_ISO8601", ".|.,.,.,.,.,.", flags, &functions::DateIso8601});
  add({"DATE_DAYOFWEEK", ".|.", flags, &functions::DateDayOfWeek});
  add({"DATE_YEAR", ".|.", flags, &functions::DateYear});
  add({"DATE_MONTH", ".|.", flags, &functions::DateMonth});
  add({"DATE_DAY", ".|.", flags, &functions::DateDay});
  add({"DATE_HOUR", ".|.", flags, &functions::DateHour});
  add({"DATE_MINUTE", ".|.", flags, &functions::DateMinute});
  add({"DATE_SECOND", ".", flags, &functions::DateSecond});
  add({"DATE_MILLISECOND", ".", flags, &functions::DateMillisecond});
  add({"DATE_DAYOFYEAR", ".|.", flags, &functions::DateDayOfYear});
  add({"DATE_ISOWEEK", ".|.", flags, &functions::DateIsoWeek});
  add({"DATE_ISOWEEKYEAR", ".|.", flags, &functions::DateIsoWeekYear});
  add({"DATE_LEAPYEAR", ".|.", flags, &functions::DateLeapYear});
  add({"DATE_QUARTER", ".|.", flags, &functions::DateQuarter});
  add({"DATE_DAYS_IN_MONTH", ".|.", flags, &functions::DateDaysInMonth});
  add({"DATE_ADD", ".,.|.,.", flags, &functions::DateAdd});
  add({"DATE_SUBTRACT", ".,.|.,.", flags, &functions::DateSubtract});
  add({"DATE_DIFF", ".,.,.|.,.,.", flags, &functions::DateDiff});
  add({"DATE_COMPARE", ".,.,.|.,.,.", flags, &functions::DateCompare});
  add({"DATE_FORMAT", ".,.|.", flags, &functions::DateFormat});
  add({"DATE_TRUNC", ".,.|.", flags, &functions::DateTrunc});
  add({"DATE_UTCTOLOCAL", ".,.|.", flags, &functions::DateUtcToLocal});
  add({"DATE_LOCALTOUTC", ".,.|.", flags, &functions::DateLocalToUtc});
  add({"DATE_TIMEZONE", "", flags, &functions::DateTimeZone});
  add({"DATE_TIMEZONES", "", flags, &functions::DateTimeZones});
  add({"DATE_ROUND", ".,.,.|.", flags, &functions::DateRound});

  // special flags:
  add({"DATE_NOW", "",
       Function::makeFlags(FF::Deterministic, FF::CanRunOnDBServerCluster,
                           FF::CanRunOnDBServerOneShard, FF::CanUseInAnalyzer),
       &functions::DateNow});  // deterministic, but not cacheable!
}

void AqlFunctionFeature::addMiscFunctions() {
  // common flags for most of these functions
  constexpr auto flags = Function::makeFlags(
      FF::Deterministic, FF::Cacheable, FF::CanRunOnDBServerCluster,
      FF::CanRunOnDBServerOneShard, FF::CanUseInAnalyzer);

  // misc functions
  add({"PASSTHRU", ".", flags, &functions::Passthru});
  add({"NOT_NULL", ".|+", flags, &functions::NotNull});
  add({"FIRST_LIST", ".|+", flags, &functions::FirstList});
  add({"FIRST_DOCUMENT", ".|+", flags, &functions::FirstDocument});
  add({"PARSE_IDENTIFIER", ".", flags, &functions::ParseIdentifier});
  add({"PARSE_KEY", ".", flags, &functions::ParseKey});
  add({"PARSE_COLLECTION", ".", flags, &functions::ParseCollection});
  add({"IS_SAME_COLLECTION", ".h,.h", flags, &functions::IsSameCollection});
  add({"DECODE_REV", ".", flags, &functions::DecodeRev});

  // cannot be used in analyzers
  add({"SHARD_ID", ".,.",
       Function::makeFlags(FF::CanRunOnDBServerCluster,
                           FF::CanRunOnDBServerOneShard),
       &functions::ShardId});

  // only function without a C++ implementation. not usable in analyzers
#ifdef USE_V8
  add({"V8", ".", Function::makeFlags(FF::Deterministic, FF::Cacheable),
       nullptr});
#endif

  // the following functions are not eligible to run on DB servers and not
  // in analyzers
  auto validationFlags = Function::makeFlags(FF::None);
  add({"SCHEMA_GET", ".", validationFlags, &functions::SchemaGet});
  add({"SCHEMA_VALIDATE", ".,.", validationFlags, &functions::SchemaValidate});

  // special flags:
  // deterministic, not cacheable. only on coordinator. not in analyzers
  add({"VERSION", "", Function::makeFlags(FF::Deterministic),
       &functions::Version});
  add({"FAIL", "|.",
       Function::makeFlags(FF::CanRunOnDBServerCluster,
                           FF::CanRunOnDBServerOneShard, FF::CanUseInAnalyzer),
       &functions::Fail});  // not deterministic and not cacheable
  add({"NOOPT", ".",
       Function::makeFlags(FF::CanRunOnDBServerCluster,
                           FF::CanRunOnDBServerOneShard, FF::NoEval,
                           FF::CanUseInAnalyzer),
       &functions::Passthru});  // prevents all optimizations!
  add({"NOEVAL", ".",
       Function::makeFlags(FF::Deterministic, FF::CanRunOnDBServerCluster,
                           FF::CanRunOnDBServerOneShard, FF::NoEval,
                           FF::CanUseInAnalyzer),
       &functions::Passthru});  // prevents all optimizations!
  add({"SLEEP", ".",
       Function::makeFlags(FF::CanRunOnDBServerCluster,
                           FF::CanRunOnDBServerOneShard),
       &functions::Sleep});  // not deterministic and not cacheable. not in
                             // analyzers
  add({"COLLECTIONS", "", Function::makeFlags(),
       &functions::Collections});  // not deterministic and not cacheable
  add({"CURRENT_USER", "", Function::makeFlags(FF::Deterministic),
       &functions::CurrentUser});  // deterministic, but not cacheable
  add({"CURRENT_DATABASE", "", Function::makeFlags(FF::Deterministic),
       &functions::CurrentDatabase});  // deterministic, but not cacheable
  add({"CHECK_DOCUMENT", ".",
       Function::makeFlags(FF::CanRunOnDBServerCluster,
                           FF::CanRunOnDBServerOneShard, FF::CanUseInAnalyzer),
       &functions::CheckDocument});  // not deterministic and not cacheable
  add({"COLLECTION_COUNT", ".h", Function::makeFlags(FF::CanReadDocuments),
       &functions::CollectionCount});  // not deterministic and not cacheable
  add({"ASSERT", ".,.",
       Function::makeFlags(FF::CanRunOnDBServerCluster,
                           FF::CanRunOnDBServerOneShard, FF::CanUseInAnalyzer),
       &functions::Assert});  // not deterministic and not cacheable
  add({"WARN", ".,.",
       Function::makeFlags(FF::CanRunOnDBServerCluster,
                           FF::CanRunOnDBServerOneShard, FF::CanUseInAnalyzer),
       &functions::Warn});  // not deterministic and not cacheable

  // NEAR, WITHIN, WITHIN_RECTANGLE and FULLTEXT are replaced by the AQL
  // optimizer with collection-/index-based subqueries. they are all
  // marked as deterministic and cacheable here as they are just
  // placeholders for collection/index accesses nowaways.
  add({"NEAR", ".h,.,.|.,.", Function::makeFlags(FF::Cacheable),
       &functions::NotImplemented});
  add({"WITHIN", ".h,.,.,.|.", Function::makeFlags(FF::Cacheable),
       &functions::NotImplemented});
  add({"WITHIN_RECTANGLE", "h.,.,.,.,.", Function::makeFlags(FF::Cacheable),
       &functions::NotImplemented});
  add({"FULLTEXT", ".h,.,.|.", Function::makeFlags(FF::Cacheable),
       &functions::NotImplemented});

  add({"MAKE_DISTRIBUTE_INPUT", ".,.",
       Function::makeFlags(FF::Deterministic, FF::Cacheable, FF::Internal,
                           FF::CanRunOnDBServerCluster,
                           FF::CanRunOnDBServerOneShard),
       &functions::MakeDistributeInput});
  add({"MAKE_DISTRIBUTE_INPUT_WITH_KEY_CREATION", ".,.,.",
       Function::makeFlags(FF::Internal),
       &functions::MakeDistributeInputWithKeyCreation});
  add({"MAKE_DISTRIBUTE_GRAPH_INPUT", ".",
       Function::makeFlags(FF::Deterministic, FF::Cacheable, FF::Internal,
                           FF::CanRunOnDBServerCluster,
                           FF::CanRunOnDBServerOneShard),
       &functions::MakeDistributeGraphInput});
#ifdef USE_ENTERPRISE
  add({"SELECT_SMART_DISTRIBUTE_GRAPH_INPUT", ".,.",
       Function::makeFlags(FF::Deterministic, FF::Cacheable, FF::Internal,
                           FF::CanRunOnDBServerCluster,
                           FF::CanRunOnDBServerOneShard),
       &functions::SelectSmartDistributeGraphInput});
#endif

  // this is an internal function that is only here for testing. it cannot
  // be invoked by end users, because refering to internal functions from user
  // queries will pretend these functions do not exist.
  add({"INTERNAL", "", Function::makeFlags(FF::Internal),
       &functions::NotImplemented});
}

}  // namespace aql
}  // namespace arangodb
