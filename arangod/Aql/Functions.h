////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Aql/AqlValue.h"
#include "Basics/ErrorCode.h"

#include <velocypack/Sink.h>

#include "Containers/SmallVector.h"

#include <span>

namespace arangodb {
class Result;
namespace transaction {
class Methods;
}

namespace aql {

struct AstNode;
class ExpressionContext;

using VPackFunctionParameters = containers::SmallVector<AqlValue, 4>;
using VPackFunctionParametersView = std::span<AqlValue const>;

typedef AqlValue (*FunctionImplementation)(arangodb::aql::ExpressionContext*,
                                           AstNode const&,
                                           VPackFunctionParametersView);

void registerError(ExpressionContext* expressionContext,
                   char const* functionName, ErrorCode code);
void registerWarning(ExpressionContext* expressionContext,
                     char const* functionName, ErrorCode code);
void registerWarning(ExpressionContext* expressionContext,
                     char const* functionName, Result const& rr);
void registerInvalidArgumentWarning(ExpressionContext* expressionContext,
                                    char const* functionName);

struct Functions {
 public:
  /// @brief helper function. not callable as a "normal" AQL function
  static void Stringify(velocypack::Options const* vopts,
                        arangodb::velocypack::StringSink& buffer,
                        arangodb::velocypack::Slice const& slice);

  static AqlValue IsNull(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
  static AqlValue IsBool(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
  static AqlValue IsNumber(arangodb::aql::ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView);
  static AqlValue IsString(arangodb::aql::ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView);
  static AqlValue IsArray(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  static AqlValue IsObject(arangodb::aql::ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView);
  static AqlValue Typename(arangodb::aql::ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView);
  static AqlValue ToNumber(arangodb::aql::ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView);
  static AqlValue ToString(arangodb::aql::ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView);
  static AqlValue ToBool(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
  static AqlValue ToArray(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  static AqlValue Length(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
  static AqlValue FindFirst(arangodb::aql::ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView);
  static AqlValue FindLast(arangodb::aql::ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView);
  static AqlValue Reverse(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  static AqlValue First(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue Last(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
  static AqlValue Nth(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
  static AqlValue Contains(arangodb::aql::ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView);
  static AqlValue Concat(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
  static AqlValue ConcatSeparator(arangodb::aql::ExpressionContext*,
                                  AstNode const&, VPackFunctionParametersView);
  static AqlValue CharLength(arangodb::aql::ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView);
  static AqlValue Lower(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue Upper(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue Substring(arangodb::aql::ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView);
  static AqlValue Substitute(arangodb::aql::ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView);
  static AqlValue Left(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
  static AqlValue Right(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue Trim(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
  static AqlValue LTrim(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue RTrim(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue Split(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue Like(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
  static AqlValue RegexMatches(arangodb::aql::ExpressionContext*,
                               AstNode const&, VPackFunctionParametersView);
  static AqlValue RegexTest(arangodb::aql::ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView);
  static AqlValue RegexReplace(arangodb::aql::ExpressionContext*,
                               AstNode const&, VPackFunctionParametersView);
  static AqlValue RegexSplit(arangodb::aql::ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView);
  static AqlValue ToBase64(arangodb::aql::ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView);
  static AqlValue ToHex(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue EncodeURIComponent(arangodb::aql::ExpressionContext*,
                                     AstNode const&,
                                     VPackFunctionParametersView);
  static AqlValue Uuid(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
  static AqlValue Soundex(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  static AqlValue LevenshteinDistance(arangodb::aql::ExpressionContext*,
                                      AstNode const&,
                                      VPackFunctionParametersView);
  static AqlValue LevenshteinMatch(arangodb::aql::ExpressionContext*,
                                   AstNode const&, VPackFunctionParametersView);
  static AqlValue NgramSimilarity(ExpressionContext*, AstNode const&,
                                  VPackFunctionParametersView);
  static AqlValue NgramPositionalSimilarity(ExpressionContext* ctx,
                                            AstNode const&,
                                            VPackFunctionParametersView);
  static AqlValue NgramMatch(ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView);
  static AqlValue InRange(ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  // Date
  static AqlValue DateNow(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  static AqlValue DateIso8601(arangodb::aql::ExpressionContext*, AstNode const&,
                              VPackFunctionParametersView);
  static AqlValue DateTimestamp(arangodb::aql::ExpressionContext*,
                                AstNode const&, VPackFunctionParametersView);
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
  static AqlValue IsDatestring(arangodb::aql::ExpressionContext* query,
                               AstNode const&,
                               VPackFunctionParametersView params);
  static AqlValue DateDayOfWeek(arangodb::aql::ExpressionContext*,
                                AstNode const&, VPackFunctionParametersView);
  static AqlValue DateYear(arangodb::aql::ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView);
  static AqlValue DateMonth(arangodb::aql::ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView);
  static AqlValue DateDay(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  static AqlValue DateHour(arangodb::aql::ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView);
  static AqlValue DateMinute(arangodb::aql::ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView);
  static AqlValue DateSecond(arangodb::aql::ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView);
  static AqlValue DateMillisecond(arangodb::aql::ExpressionContext*,
                                  AstNode const&, VPackFunctionParametersView);
  static AqlValue DateDayOfYear(arangodb::aql::ExpressionContext*,
                                AstNode const&, VPackFunctionParametersView);
  static AqlValue DateIsoWeek(arangodb::aql::ExpressionContext*, AstNode const&,
                              VPackFunctionParametersView);
  static AqlValue DateLeapYear(arangodb::aql::ExpressionContext*,
                               AstNode const&, VPackFunctionParametersView);
  static AqlValue DateQuarter(arangodb::aql::ExpressionContext*, AstNode const&,
                              VPackFunctionParametersView);
  static AqlValue DateDaysInMonth(arangodb::aql::ExpressionContext*,
                                  AstNode const&, VPackFunctionParametersView);
  static AqlValue DateTrunc(arangodb::aql::ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView);
  static AqlValue DateUtcToLocal(arangodb::aql::ExpressionContext*,
                                 AstNode const&, VPackFunctionParametersView);
  static AqlValue DateLocalToUtc(arangodb::aql::ExpressionContext*,
                                 AstNode const&, VPackFunctionParametersView);
  static AqlValue DateTimeZone(arangodb::aql::ExpressionContext*,
                               AstNode const&, VPackFunctionParametersView);
  static AqlValue DateTimeZones(arangodb::aql::ExpressionContext*,
                                AstNode const&, VPackFunctionParametersView);
  static AqlValue DateAdd(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  static AqlValue DateSubtract(arangodb::aql::ExpressionContext*,
                               AstNode const&, VPackFunctionParametersView);
  static AqlValue DateDiff(arangodb::aql::ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView);
  static AqlValue DateRound(arangodb::aql::ExpressionContext*, AstNode const&,
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
  static AqlValue DateCompare(arangodb::aql::ExpressionContext* query,
                              AstNode const&,
                              VPackFunctionParametersView params);

  static AqlValue DateFormat(arangodb::aql::ExpressionContext* query,
                             AstNode const&,
                             VPackFunctionParametersView params);
  static AqlValue Passthru(arangodb::aql::ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView);
  static AqlValue Unset(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue UnsetRecursive(arangodb::aql::ExpressionContext*,
                                 AstNode const&, VPackFunctionParametersView);
  static AqlValue Keep(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
  static AqlValue KeepRecursive(arangodb::aql::ExpressionContext*,
                                AstNode const&, VPackFunctionParametersView);
  static AqlValue Translate(arangodb::aql::ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView);
  static AqlValue Merge(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue MergeRecursive(arangodb::aql::ExpressionContext*,
                                 AstNode const&, VPackFunctionParametersView);
  static AqlValue Has(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
  static AqlValue Attributes(arangodb::aql::ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView);
  static AqlValue Values(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
  static AqlValue Min(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
  static AqlValue Max(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
  static AqlValue Sum(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
  static AqlValue Average(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  static AqlValue Product(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  static AqlValue Sleep(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue Collections(arangodb::aql::ExpressionContext*, AstNode const&,
                              VPackFunctionParametersView);
  static AqlValue RandomToken(arangodb::aql::ExpressionContext*, AstNode const&,
                              VPackFunctionParametersView);
  static AqlValue IpV4FromNumber(arangodb::aql::ExpressionContext*,
                                 AstNode const&, VPackFunctionParametersView);
  static AqlValue IpV4ToNumber(arangodb::aql::ExpressionContext*,
                               AstNode const&, VPackFunctionParametersView);
  static AqlValue IsIpV4(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
  static AqlValue Md5(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
  static AqlValue Sha1(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
  static AqlValue Sha512(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
  static AqlValue Crc32(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue Fnv64(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue Hash(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
  static AqlValue IsKey(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue CountDistinct(arangodb::aql::ExpressionContext*,
                                AstNode const&, VPackFunctionParametersView);
  static AqlValue CheckDocument(arangodb::aql::ExpressionContext*,
                                AstNode const&, VPackFunctionParametersView);
  static AqlValue Unique(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
  static AqlValue SortedUnique(arangodb::aql::ExpressionContext*,
                               AstNode const&, VPackFunctionParametersView);
  static AqlValue Sorted(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
  static AqlValue Union(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue UnionDistinct(arangodb::aql::ExpressionContext*,
                                AstNode const&, VPackFunctionParametersView);
  static AqlValue Intersection(arangodb::aql::ExpressionContext*,
                               AstNode const&, VPackFunctionParametersView);
  static AqlValue Outersection(arangodb::aql::ExpressionContext*,
                               AstNode const&, VPackFunctionParametersView);
  static AqlValue Jaccard(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  static AqlValue Distance(arangodb::aql::ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView);
  static AqlValue GeoInRange(arangodb::aql::ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView);
  static AqlValue GeoDistance(arangodb::aql::ExpressionContext*, AstNode const&,
                              VPackFunctionParametersView);
  static AqlValue GeoContains(arangodb::aql::ExpressionContext*, AstNode const&,
                              VPackFunctionParametersView);
  static AqlValue GeoIntersects(arangodb::aql::ExpressionContext*,
                                AstNode const&, VPackFunctionParametersView);
  static AqlValue GeoEquals(arangodb::aql::ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView);
  static AqlValue GeoArea(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  static AqlValue IsInPolygon(arangodb::aql::ExpressionContext*, AstNode const&,
                              VPackFunctionParametersView);
  static AqlValue GeoPoint(arangodb::aql::ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView);
  static AqlValue GeoMultiPoint(arangodb::aql::ExpressionContext*,
                                AstNode const&, VPackFunctionParametersView);
  static AqlValue GeoPolygon(arangodb::aql::ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView);
  static AqlValue GeoMultiPolygon(arangodb::aql::ExpressionContext*,
                                  AstNode const&, VPackFunctionParametersView);
  static AqlValue GeoLinestring(arangodb::aql::ExpressionContext*,
                                AstNode const&, VPackFunctionParametersView);
  static AqlValue GeoMultiLinestring(arangodb::aql::ExpressionContext*,
                                     AstNode const&,
                                     VPackFunctionParametersView);
  static AqlValue Flatten(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  static AqlValue Zip(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
  static AqlValue JsonStringify(arangodb::aql::ExpressionContext*,
                                AstNode const&, VPackFunctionParametersView);
  static AqlValue JsonParse(arangodb::aql::ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView);
  static AqlValue ParseIdentifier(arangodb::aql::ExpressionContext*,
                                  AstNode const&, VPackFunctionParametersView);
  static AqlValue Slice(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue Minus(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue Document(arangodb::aql::ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView);
  static AqlValue Matches(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  static AqlValue Round(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue Abs(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
  static AqlValue Ceil(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
  static AqlValue Floor(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue Sqrt(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
  static AqlValue Pow(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
  static AqlValue Log(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
  static AqlValue Log2(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
  static AqlValue Log10(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue Exp(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
  static AqlValue Exp2(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
  static AqlValue Sin(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
  static AqlValue Cos(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
  static AqlValue Tan(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
  static AqlValue Asin(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
  static AqlValue Acos(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
  static AqlValue Atan(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
  static AqlValue Atan2(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue Radians(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  static AqlValue Degrees(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  static AqlValue Pi(arangodb::aql::ExpressionContext*, AstNode const&,
                     VPackFunctionParametersView);
  static AqlValue BitAnd(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
  static AqlValue BitOr(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue BitXOr(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
  static AqlValue BitNegate(arangodb::aql::ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView);
  static AqlValue BitTest(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  static AqlValue BitPopcount(arangodb::aql::ExpressionContext*, AstNode const&,
                              VPackFunctionParametersView);
  static AqlValue BitShiftLeft(arangodb::aql::ExpressionContext*,
                               AstNode const&, VPackFunctionParametersView);
  static AqlValue BitShiftRight(arangodb::aql::ExpressionContext*,
                                AstNode const&, VPackFunctionParametersView);
  static AqlValue BitConstruct(arangodb::aql::ExpressionContext*,
                               AstNode const&, VPackFunctionParametersView);
  static AqlValue BitDeconstruct(arangodb::aql::ExpressionContext*,
                                 AstNode const&, VPackFunctionParametersView);
  static AqlValue BitFromString(arangodb::aql::ExpressionContext*,
                                AstNode const&, VPackFunctionParametersView);
  static AqlValue BitToString(arangodb::aql::ExpressionContext*, AstNode const&,
                              VPackFunctionParametersView);
  static AqlValue Rand(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
  static AqlValue FirstDocument(arangodb::aql::ExpressionContext*,
                                AstNode const&, VPackFunctionParametersView);
  static AqlValue FirstList(arangodb::aql::ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView);
  static AqlValue Push(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
  static AqlValue Pop(arangodb::aql::ExpressionContext*, AstNode const&,
                      VPackFunctionParametersView);
  static AqlValue Append(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
  static AqlValue Unshift(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  static AqlValue Shift(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue RemoveValue(arangodb::aql::ExpressionContext*, AstNode const&,
                              VPackFunctionParametersView);
  static AqlValue RemoveValues(arangodb::aql::ExpressionContext*,
                               AstNode const&, VPackFunctionParametersView);
  static AqlValue RemoveNth(arangodb::aql::ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView);
  static AqlValue ReplaceNth(arangodb::aql::ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView);
  static AqlValue Interleave(arangodb::aql::ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView);
  static AqlValue NotNull(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  static AqlValue CurrentDatabase(arangodb::aql::ExpressionContext*,
                                  AstNode const&, VPackFunctionParametersView);
  static AqlValue CollectionCount(arangodb::aql::ExpressionContext*,
                                  AstNode const&, VPackFunctionParametersView);
  static AqlValue VarianceSample(arangodb::aql::ExpressionContext*,
                                 AstNode const&, VPackFunctionParametersView);
  static AqlValue PregelResult(arangodb::aql::ExpressionContext*,
                               AstNode const&, VPackFunctionParametersView);
  static AqlValue VariancePopulation(arangodb::aql::ExpressionContext*,
                                     AstNode const&,
                                     VPackFunctionParametersView);
  static AqlValue StdDevSample(arangodb::aql::ExpressionContext*,
                               AstNode const&, VPackFunctionParametersView);
  static AqlValue StdDevPopulation(arangodb::aql::ExpressionContext*,
                                   AstNode const&, VPackFunctionParametersView);
  static AqlValue Median(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
  static AqlValue Percentile(arangodb::aql::ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView);
  static AqlValue Range(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue Position(arangodb::aql::ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView);
  static AqlValue Call(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
  static AqlValue Apply(arangodb::aql::ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView);
  static AqlValue Version(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  static AqlValue IsSameCollection(arangodb::aql::ExpressionContext*,
                                   AstNode const&, VPackFunctionParametersView);
  static AqlValue Assert(arangodb::aql::ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView);
  static AqlValue Warn(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
  static AqlValue Fail(arangodb::aql::ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView);
  static AqlValue DecodeRev(arangodb::aql::ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView);
  static AqlValue ShardId(arangodb::aql::ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView);
  static AqlValue CurrentUser(arangodb::aql::ExpressionContext*, AstNode const&,
                              VPackFunctionParametersView);

  static AqlValue SchemaGet(arangodb::aql::ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView);
  static AqlValue SchemaValidate(arangodb::aql::ExpressionContext*,
                                 AstNode const&, VPackFunctionParametersView);

  static AqlValue CallGreenspun(arangodb::aql::ExpressionContext*,
                                AstNode const&, VPackFunctionParametersView);

  static AqlValue MakeDistributeInput(arangodb::aql::ExpressionContext*,
                                      AstNode const&,
                                      VPackFunctionParametersView);
  static AqlValue MakeDistributeInputWithKeyCreation(
      arangodb::aql::ExpressionContext*, AstNode const&,
      VPackFunctionParametersView);
  static AqlValue MakeDistributeGraphInput(arangodb::aql::ExpressionContext*,
                                           AstNode const&,
                                           VPackFunctionParametersView);

  static AqlValue DecayGauss(arangodb::aql::ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView);

  static AqlValue DecayExp(arangodb::aql::ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView);

  static AqlValue DecayLinear(arangodb::aql::ExpressionContext*, AstNode const&,
                              VPackFunctionParametersView);

  static AqlValue CosineSimilarity(arangodb::aql::ExpressionContext*,
                                   AstNode const&, VPackFunctionParametersView);

  static AqlValue L1Distance(arangodb::aql::ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView);

  static AqlValue L2Distance(arangodb::aql::ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView);

  /// @brief dummy function that will only throw an error when called
  static AqlValue NotImplemented(arangodb::aql::ExpressionContext*,
                                 AstNode const&, VPackFunctionParametersView);

  /// @brief maximum precision for bit operations
  static constexpr uint64_t bitFunctionsMaxSupportedBits = 32;
  static constexpr uint64_t bitFunctionsMaxSupportedValue =
      ((uint64_t(1) << bitFunctionsMaxSupportedBits) - uint64_t(1));
};

}  // namespace aql
}  // namespace arangodb
