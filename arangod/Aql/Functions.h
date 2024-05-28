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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/AqlValue.h"
#include "Basics/ErrorCode.h"
#include "Containers/SmallVector.h"

#include <span>
#include <string_view>

namespace arangodb {
class Result;

namespace transaction {
class Methods;
}

namespace velocypack {
class Slice;
}

namespace aql {
struct AstNode;
class ExpressionContext;

namespace functions {

using VPackFunctionParameters = containers::SmallVector<AqlValue, 4>;
using VPackFunctionParametersView = std::span<AqlValue const>;

typedef AqlValue (*FunctionImplementation)(arangodb::aql::ExpressionContext*,
                                           AstNode const&,
                                           VPackFunctionParametersView);

void registerError(ExpressionContext* expressionContext,
                   std::string_view functionName, ErrorCode code);
void registerWarning(ExpressionContext* expressionContext,
                     std::string_view functionName, ErrorCode code);
void registerWarning(ExpressionContext* expressionContext,
                     std::string_view functionName, Result const& rr);
void registerInvalidArgumentWarning(ExpressionContext* expressionContext,
                                    std::string_view functionName);

// Returns zero-terminated function name from the given FCALL node.
std::string_view getFunctionName(AstNode const& node) noexcept;

bool getBooleanParameter(VPackFunctionParametersView parameters,
                         size_t startParameter, bool defaultValue);

AqlValue const& extractFunctionParameterValue(
    VPackFunctionParametersView parameters, size_t position);

AqlValue numberValue(double value, bool nullify);

std::string extractCollectionName(transaction::Methods* trx,
                                  VPackFunctionParametersView parameters,
                                  size_t position);

template<typename T>
void appendAsString(velocypack::Options const& vopts, T& buffer,
                    AqlValue const& value);

/// @brief helper function. not callable as a "normal" AQL function
template<typename T>
void stringify(velocypack::Options const* vopts, T& buffer,
               arangodb::velocypack::Slice slice);

AqlValue IsNull(arangodb::aql::ExpressionContext*, AstNode const&,
                VPackFunctionParametersView);
AqlValue IsBool(arangodb::aql::ExpressionContext*, AstNode const&,
                VPackFunctionParametersView);
AqlValue IsNumber(arangodb::aql::ExpressionContext*, AstNode const&,
                  VPackFunctionParametersView);
AqlValue IsString(arangodb::aql::ExpressionContext*, AstNode const&,
                  VPackFunctionParametersView);
AqlValue IsArray(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue IsObject(arangodb::aql::ExpressionContext*, AstNode const&,
                  VPackFunctionParametersView);
AqlValue Typename(arangodb::aql::ExpressionContext*, AstNode const&,
                  VPackFunctionParametersView);
AqlValue ToNumber(arangodb::aql::ExpressionContext*, AstNode const&,
                  VPackFunctionParametersView);
AqlValue ToString(arangodb::aql::ExpressionContext*, AstNode const&,
                  VPackFunctionParametersView);
AqlValue ToBool(arangodb::aql::ExpressionContext*, AstNode const&,
                VPackFunctionParametersView);
AqlValue ToArray(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue Length(arangodb::aql::ExpressionContext*, AstNode const&,
                VPackFunctionParametersView);
AqlValue FindFirst(arangodb::aql::ExpressionContext*, AstNode const&,
                   VPackFunctionParametersView);
AqlValue FindLast(arangodb::aql::ExpressionContext*, AstNode const&,
                  VPackFunctionParametersView);
AqlValue Reverse(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue First(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue Last(arangodb::aql::ExpressionContext*, AstNode const&,
              VPackFunctionParametersView);
AqlValue Nth(arangodb::aql::ExpressionContext*, AstNode const&,
             VPackFunctionParametersView);
AqlValue Contains(arangodb::aql::ExpressionContext*, AstNode const&,
                  VPackFunctionParametersView);
AqlValue Concat(arangodb::aql::ExpressionContext*, AstNode const&,
                VPackFunctionParametersView);
AqlValue ConcatSeparator(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
AqlValue CharLength(arangodb::aql::ExpressionContext*, AstNode const&,
                    VPackFunctionParametersView);
AqlValue Lower(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue Upper(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue Substring(arangodb::aql::ExpressionContext*, AstNode const&,
                   VPackFunctionParametersView);
AqlValue SubstringBytes(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
AqlValue Substitute(arangodb::aql::ExpressionContext*, AstNode const&,
                    VPackFunctionParametersView);
AqlValue Left(arangodb::aql::ExpressionContext*, AstNode const&,
              VPackFunctionParametersView);
AqlValue Right(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue Trim(arangodb::aql::ExpressionContext*, AstNode const&,
              VPackFunctionParametersView);
AqlValue LTrim(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue RTrim(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue Split(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue Like(arangodb::aql::ExpressionContext*, AstNode const&,
              VPackFunctionParametersView);
AqlValue RegexMatches(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
AqlValue RegexTest(arangodb::aql::ExpressionContext*, AstNode const&,
                   VPackFunctionParametersView);
AqlValue RegexReplace(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
AqlValue RegexSplit(arangodb::aql::ExpressionContext*, AstNode const&,
                    VPackFunctionParametersView);
AqlValue ToBase64(arangodb::aql::ExpressionContext*, AstNode const&,
                  VPackFunctionParametersView);
AqlValue ToHex(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue ToChar(arangodb::aql::ExpressionContext*, AstNode const&,
                VPackFunctionParametersView);
AqlValue Repeat(arangodb::aql::ExpressionContext*, AstNode const&,
                VPackFunctionParametersView);
AqlValue EncodeURIComponent(arangodb::aql::ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView);
AqlValue Uuid(arangodb::aql::ExpressionContext*, AstNode const&,
              VPackFunctionParametersView);
AqlValue Soundex(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue LevenshteinDistance(arangodb::aql::ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView);
AqlValue LevenshteinMatch(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
AqlValue NgramSimilarity(ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
AqlValue NgramPositionalSimilarity(ExpressionContext* ctx, AstNode const&,
                                   VPackFunctionParametersView);
AqlValue NgramMatch(ExpressionContext*, AstNode const&,
                    VPackFunctionParametersView);
AqlValue InRange(ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
// Date
AqlValue DateNow(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue DateIso8601(arangodb::aql::ExpressionContext*, AstNode const&,
                     VPackFunctionParametersView);
AqlValue DateTimestamp(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
/**
 * @brief Tests is the first given parameter is a valid date string
 * format.
 *
 * @param query The AQL Query
 * @param trx The ongoing transaction
 * @param params List of input parameters
 *
 * @return AQLValue(true) iff the first input parameter is a string,r
 *         and is of valid date format. (May return true on invalid dates
 *         like 2018-02-31)
 */
AqlValue IsDatestring(arangodb::aql::ExpressionContext* query, AstNode const&,
                      VPackFunctionParametersView params);
AqlValue DateDayOfWeek(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
AqlValue DateYear(arangodb::aql::ExpressionContext*, AstNode const&,
                  VPackFunctionParametersView);
AqlValue DateMonth(arangodb::aql::ExpressionContext*, AstNode const&,
                   VPackFunctionParametersView);
AqlValue DateDay(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue DateHour(arangodb::aql::ExpressionContext*, AstNode const&,
                  VPackFunctionParametersView);
AqlValue DateMinute(arangodb::aql::ExpressionContext*, AstNode const&,
                    VPackFunctionParametersView);
AqlValue DateSecond(arangodb::aql::ExpressionContext*, AstNode const&,
                    VPackFunctionParametersView);
AqlValue DateMillisecond(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
AqlValue DateDayOfYear(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
AqlValue DateIsoWeek(arangodb::aql::ExpressionContext*, AstNode const&,
                     VPackFunctionParametersView);
AqlValue DateIsoWeekYear(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
AqlValue DateLeapYear(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
AqlValue DateQuarter(arangodb::aql::ExpressionContext*, AstNode const&,
                     VPackFunctionParametersView);
AqlValue DateDaysInMonth(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
AqlValue DateTrunc(arangodb::aql::ExpressionContext*, AstNode const&,
                   VPackFunctionParametersView);
AqlValue DateUtcToLocal(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
AqlValue DateLocalToUtc(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
AqlValue DateTimeZone(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
AqlValue DateTimeZones(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
AqlValue DateAdd(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue DateSubtract(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
AqlValue DateDiff(arangodb::aql::ExpressionContext*, AstNode const&,
                  VPackFunctionParametersView);
AqlValue DateRound(arangodb::aql::ExpressionContext*, AstNode const&,
                   VPackFunctionParametersView);
/**
 * @brief Compares two dates given as the first two arguments.
 *        Third argument defines the highest signficant part,
 *        Forth (optional) defines the lowest significant part.
 *
 * @param query The AQL query
 * @param trx The ongoing transaction
 * @param params 3 or 4 Parameters.
 *         1. A date string
 *         2. Another date string.
 *         3. Modifier year/day/houer etc. Will compare 1 to 2
 *            based on this value.
 *         4. Another modifier (like 3), optional. If given
 *            will compare all parts starting from modifier
 *            in (3) to this modifier.
 *
 * Example:
 * (DateA, DateB, month, minute)
 * Will compare A to B considering month, day, hour and minute.
 * It will ignore year, second and millisecond.
 *
 * @return AQLValue(TRUE) if the dates are equal. AQLValue(FALSE)
 * if they differ. May return AQLValue(NULL) on invalid input
 */
AqlValue DateCompare(arangodb::aql::ExpressionContext* query, AstNode const&,
                     VPackFunctionParametersView params);

AqlValue DateFormat(arangodb::aql::ExpressionContext* query, AstNode const&,
                    VPackFunctionParametersView params);
AqlValue Passthru(arangodb::aql::ExpressionContext*, AstNode const&,
                  VPackFunctionParametersView);
AqlValue Unset(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue UnsetRecursive(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
AqlValue Keep(arangodb::aql::ExpressionContext*, AstNode const&,
              VPackFunctionParametersView);
AqlValue KeepRecursive(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
AqlValue Translate(arangodb::aql::ExpressionContext*, AstNode const&,
                   VPackFunctionParametersView);
AqlValue Merge(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue MergeRecursive(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
AqlValue Has(arangodb::aql::ExpressionContext*, AstNode const&,
             VPackFunctionParametersView);
AqlValue Attributes(arangodb::aql::ExpressionContext*, AstNode const&,
                    VPackFunctionParametersView);
AqlValue Values(arangodb::aql::ExpressionContext*, AstNode const&,
                VPackFunctionParametersView);
AqlValue Value(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue Min(arangodb::aql::ExpressionContext*, AstNode const&,
             VPackFunctionParametersView);
AqlValue Max(arangodb::aql::ExpressionContext*, AstNode const&,
             VPackFunctionParametersView);
AqlValue Sum(arangodb::aql::ExpressionContext*, AstNode const&,
             VPackFunctionParametersView);
AqlValue Average(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue Product(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue Sleep(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue Collections(arangodb::aql::ExpressionContext*, AstNode const&,
                     VPackFunctionParametersView);
AqlValue RandomToken(arangodb::aql::ExpressionContext*, AstNode const&,
                     VPackFunctionParametersView);
AqlValue IpV4FromNumber(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
AqlValue IpV4ToNumber(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
AqlValue IsIpV4(arangodb::aql::ExpressionContext*, AstNode const&,
                VPackFunctionParametersView);
AqlValue Md5(arangodb::aql::ExpressionContext*, AstNode const&,
             VPackFunctionParametersView);
AqlValue Sha1(arangodb::aql::ExpressionContext*, AstNode const&,
              VPackFunctionParametersView);
AqlValue Sha256(arangodb::aql::ExpressionContext*, AstNode const&,
                VPackFunctionParametersView);
AqlValue Sha512(arangodb::aql::ExpressionContext*, AstNode const&,
                VPackFunctionParametersView);
AqlValue Crc32(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue Fnv64(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue Hash(arangodb::aql::ExpressionContext*, AstNode const&,
              VPackFunctionParametersView);
AqlValue IsKey(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue CountDistinct(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
AqlValue CheckDocument(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
AqlValue Unique(arangodb::aql::ExpressionContext*, AstNode const&,
                VPackFunctionParametersView);
AqlValue SortedUnique(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
AqlValue Sorted(arangodb::aql::ExpressionContext*, AstNode const&,
                VPackFunctionParametersView);
AqlValue Union(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue UnionDistinct(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
AqlValue Intersection(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
AqlValue Outersection(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
AqlValue Jaccard(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue Distance(arangodb::aql::ExpressionContext*, AstNode const&,
                  VPackFunctionParametersView);
AqlValue GeoInRange(arangodb::aql::ExpressionContext*, AstNode const&,
                    VPackFunctionParametersView);
AqlValue GeoDistance(arangodb::aql::ExpressionContext*, AstNode const&,
                     VPackFunctionParametersView);
AqlValue GeoContains(arangodb::aql::ExpressionContext*, AstNode const&,
                     VPackFunctionParametersView);
AqlValue GeoIntersects(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
AqlValue GeoEquals(arangodb::aql::ExpressionContext*, AstNode const&,
                   VPackFunctionParametersView);
AqlValue GeoArea(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue IsInPolygon(arangodb::aql::ExpressionContext*, AstNode const&,
                     VPackFunctionParametersView);
AqlValue GeoPoint(arangodb::aql::ExpressionContext*, AstNode const&,
                  VPackFunctionParametersView);
AqlValue GeoMultiPoint(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
AqlValue GeoPolygon(arangodb::aql::ExpressionContext*, AstNode const&,
                    VPackFunctionParametersView);
AqlValue GeoMultiPolygon(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
AqlValue GeoLinestring(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
AqlValue GeoMultiLinestring(arangodb::aql::ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView);
AqlValue Flatten(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue Zip(arangodb::aql::ExpressionContext*, AstNode const&,
             VPackFunctionParametersView);
AqlValue Entries(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue JsonStringify(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
AqlValue JsonParse(arangodb::aql::ExpressionContext*, AstNode const&,
                   VPackFunctionParametersView);
AqlValue ParseIdentifier(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
AqlValue ParseKey(arangodb::aql::ExpressionContext*, AstNode const&,
                  VPackFunctionParametersView);
AqlValue ParseCollection(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
AqlValue Slice(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue Minus(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue Document(arangodb::aql::ExpressionContext*, AstNode const&,
                  VPackFunctionParametersView);
AqlValue Matches(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue Round(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue Abs(arangodb::aql::ExpressionContext*, AstNode const&,
             VPackFunctionParametersView);
AqlValue Ceil(arangodb::aql::ExpressionContext*, AstNode const&,
              VPackFunctionParametersView);
AqlValue Floor(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue Sqrt(arangodb::aql::ExpressionContext*, AstNode const&,
              VPackFunctionParametersView);
AqlValue Pow(arangodb::aql::ExpressionContext*, AstNode const&,
             VPackFunctionParametersView);
AqlValue Log(arangodb::aql::ExpressionContext*, AstNode const&,
             VPackFunctionParametersView);
AqlValue Log2(arangodb::aql::ExpressionContext*, AstNode const&,
              VPackFunctionParametersView);
AqlValue Log10(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue Exp(arangodb::aql::ExpressionContext*, AstNode const&,
             VPackFunctionParametersView);
AqlValue Exp2(arangodb::aql::ExpressionContext*, AstNode const&,
              VPackFunctionParametersView);
AqlValue Sin(arangodb::aql::ExpressionContext*, AstNode const&,
             VPackFunctionParametersView);
AqlValue Cos(arangodb::aql::ExpressionContext*, AstNode const&,
             VPackFunctionParametersView);
AqlValue Tan(arangodb::aql::ExpressionContext*, AstNode const&,
             VPackFunctionParametersView);
AqlValue Asin(arangodb::aql::ExpressionContext*, AstNode const&,
              VPackFunctionParametersView);
AqlValue Acos(arangodb::aql::ExpressionContext*, AstNode const&,
              VPackFunctionParametersView);
AqlValue Atan(arangodb::aql::ExpressionContext*, AstNode const&,
              VPackFunctionParametersView);
AqlValue Atan2(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue Radians(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue Degrees(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue Pi(arangodb::aql::ExpressionContext*, AstNode const&,
            VPackFunctionParametersView);
AqlValue BitAnd(arangodb::aql::ExpressionContext*, AstNode const&,
                VPackFunctionParametersView);
AqlValue BitOr(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue BitXOr(arangodb::aql::ExpressionContext*, AstNode const&,
                VPackFunctionParametersView);
AqlValue BitNegate(arangodb::aql::ExpressionContext*, AstNode const&,
                   VPackFunctionParametersView);
AqlValue BitTest(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue BitPopcount(arangodb::aql::ExpressionContext*, AstNode const&,
                     VPackFunctionParametersView);
AqlValue BitShiftLeft(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
AqlValue BitShiftRight(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
AqlValue BitConstruct(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
AqlValue BitDeconstruct(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
AqlValue BitFromString(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
AqlValue BitToString(arangodb::aql::ExpressionContext*, AstNode const&,
                     VPackFunctionParametersView);
AqlValue Rand(arangodb::aql::ExpressionContext*, AstNode const&,
              VPackFunctionParametersView);
AqlValue FirstDocument(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
AqlValue FirstList(arangodb::aql::ExpressionContext*, AstNode const&,
                   VPackFunctionParametersView);
AqlValue Push(arangodb::aql::ExpressionContext*, AstNode const&,
              VPackFunctionParametersView);
AqlValue Pop(arangodb::aql::ExpressionContext*, AstNode const&,
             VPackFunctionParametersView);
AqlValue Append(arangodb::aql::ExpressionContext*, AstNode const&,
                VPackFunctionParametersView);
AqlValue Unshift(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue Shift(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue RemoveValue(arangodb::aql::ExpressionContext*, AstNode const&,
                     VPackFunctionParametersView);
AqlValue RemoveValues(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
AqlValue RemoveNth(arangodb::aql::ExpressionContext*, AstNode const&,
                   VPackFunctionParametersView);
AqlValue ReplaceNth(arangodb::aql::ExpressionContext*, AstNode const&,
                    VPackFunctionParametersView);
AqlValue Interleave(arangodb::aql::ExpressionContext*, AstNode const&,
                    VPackFunctionParametersView);
AqlValue NotNull(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue CurrentDatabase(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
AqlValue CollectionCount(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
AqlValue VarianceSample(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
AqlValue VariancePopulation(arangodb::aql::ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView);
AqlValue StdDevSample(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
AqlValue StdDevPopulation(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
AqlValue Median(arangodb::aql::ExpressionContext*, AstNode const&,
                VPackFunctionParametersView);
AqlValue Percentile(arangodb::aql::ExpressionContext*, AstNode const&,
                    VPackFunctionParametersView);
AqlValue Range(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue Position(arangodb::aql::ExpressionContext*, AstNode const&,
                  VPackFunctionParametersView);
AqlValue Call(arangodb::aql::ExpressionContext*, AstNode const&,
              VPackFunctionParametersView);
AqlValue Apply(arangodb::aql::ExpressionContext*, AstNode const&,
               VPackFunctionParametersView);
AqlValue Version(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue IsSameCollection(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
AqlValue Assert(arangodb::aql::ExpressionContext*, AstNode const&,
                VPackFunctionParametersView);
AqlValue Warn(arangodb::aql::ExpressionContext*, AstNode const&,
              VPackFunctionParametersView);
AqlValue Fail(arangodb::aql::ExpressionContext*, AstNode const&,
              VPackFunctionParametersView);
AqlValue DecodeRev(arangodb::aql::ExpressionContext*, AstNode const&,
                   VPackFunctionParametersView);
AqlValue ShardId(arangodb::aql::ExpressionContext*, AstNode const&,
                 VPackFunctionParametersView);
AqlValue CurrentUser(arangodb::aql::ExpressionContext*, AstNode const&,
                     VPackFunctionParametersView);

AqlValue SchemaGet(arangodb::aql::ExpressionContext*, AstNode const&,
                   VPackFunctionParametersView);
AqlValue SchemaValidate(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);

AqlValue MakeDistributeInput(arangodb::aql::ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView);
AqlValue MakeDistributeInputWithKeyCreation(arangodb::aql::ExpressionContext*,
                                            AstNode const&,
                                            VPackFunctionParametersView);
AqlValue MakeDistributeGraphInput(arangodb::aql::ExpressionContext*,
                                  AstNode const&, VPackFunctionParametersView);

#ifdef USE_ENTERPRISE
AqlValue SelectSmartDistributeGraphInput(arangodb::aql::ExpressionContext*,
                                         AstNode const&,
                                         VPackFunctionParametersView);
#endif

AqlValue DecayGauss(arangodb::aql::ExpressionContext*, AstNode const&,
                    VPackFunctionParametersView);

AqlValue DecayExp(arangodb::aql::ExpressionContext*, AstNode const&,
                  VPackFunctionParametersView);

AqlValue DecayLinear(arangodb::aql::ExpressionContext*, AstNode const&,
                     VPackFunctionParametersView);

AqlValue CosineSimilarity(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);

AqlValue L1Distance(arangodb::aql::ExpressionContext*, AstNode const&,
                    VPackFunctionParametersView);

AqlValue L2Distance(arangodb::aql::ExpressionContext*, AstNode const&,
                    VPackFunctionParametersView);

/// @brief dummy function that will only throw an error when called
AqlValue NotImplemented(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);

aql::AqlValue NotImplementedEE(aql::ExpressionContext*, aql::AstNode const&,
                               std::span<aql::AqlValue const>);

aql::AqlValue MinHash(aql::ExpressionContext*, aql::AstNode const&,
                      std::span<aql::AqlValue const>);

aql::AqlValue MinHashError(aql::ExpressionContext*, aql::AstNode const&,
                           std::span<aql::AqlValue const>);

aql::AqlValue MinHashCount(aql::ExpressionContext*, aql::AstNode const&,
                           std::span<aql::AqlValue const>);

aql::AqlValue MinHashMatch(aql::ExpressionContext*, aql::AstNode const&,
                           std::span<aql::AqlValue const>);

/// @brief maximum precision for bit operations
constexpr uint64_t bitFunctionsMaxSupportedBits = 32;
constexpr uint64_t bitFunctionsMaxSupportedValue =
    ((uint64_t(1) << bitFunctionsMaxSupportedBits) - uint64_t(1));

}  // namespace functions
}  // namespace aql
}  // namespace arangodb
