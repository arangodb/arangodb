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

#ifndef ARANGOD_AQL_FUNCTIONS_H
#define ARANGOD_AQL_FUNCTIONS_H 1

#include "Aql/AqlValue.h"
#include "Containers/SmallVector.h"

namespace arangodb {
class Result;
namespace transaction {
class Methods;
}

namespace basics {
class VPackStringBufferAdapter;
}

namespace aql {

class ExpressionContext;

typedef ::arangodb::containers::SmallVector<AqlValue> VPackFunctionParameters;

typedef AqlValue (*FunctionImplementation)(arangodb::aql::ExpressionContext*,
                                           transaction::Methods*,
                                           VPackFunctionParameters const&);

void registerError(ExpressionContext* expressionContext, char const* functionName, int code);
void registerWarning(ExpressionContext* expressionContext, char const* functionName, int code);
void registerWarning(ExpressionContext* expressionContext, char const* functionName, Result const& rr);
void registerInvalidArgumentWarning(ExpressionContext* expressionContext, char const* functionName);

struct Functions {
 public:
  /// @brief helper function. not callable as a "normal" AQL function
  static void Stringify(transaction::Methods* trx,
                        arangodb::basics::VPackStringBufferAdapter& buffer,
                        arangodb::velocypack::Slice const& slice);

  static AqlValue IsNull(arangodb::aql::ExpressionContext*,
                         transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue IsBool(arangodb::aql::ExpressionContext*,
                         transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue IsNumber(arangodb::aql::ExpressionContext*,
                           transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue IsString(arangodb::aql::ExpressionContext*,
                           transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue IsArray(arangodb::aql::ExpressionContext*,
                          transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue IsObject(arangodb::aql::ExpressionContext*,
                           transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Typename(arangodb::aql::ExpressionContext*,
                           transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue ToNumber(arangodb::aql::ExpressionContext*,
                           transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue ToString(arangodb::aql::ExpressionContext*,
                           transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue ToBool(arangodb::aql::ExpressionContext*,
                         transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue ToArray(arangodb::aql::ExpressionContext*,
                          transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Length(arangodb::aql::ExpressionContext*,
                         transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue FindFirst(arangodb::aql::ExpressionContext*,
                            transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue FindLast(arangodb::aql::ExpressionContext*,
                           transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Reverse(arangodb::aql::ExpressionContext*,
                          transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue First(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Last(arangodb::aql::ExpressionContext*, transaction::Methods*,
                       VPackFunctionParameters const&);
  static AqlValue Nth(arangodb::aql::ExpressionContext*, transaction::Methods*,
                      VPackFunctionParameters const&);
  static AqlValue Contains(arangodb::aql::ExpressionContext*,
                           transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Concat(arangodb::aql::ExpressionContext*,
                         transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue ConcatSeparator(arangodb::aql::ExpressionContext*,
                                  transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue CharLength(arangodb::aql::ExpressionContext*,
                             transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Lower(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Upper(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Substring(arangodb::aql::ExpressionContext*,
                            transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Substitute(arangodb::aql::ExpressionContext*,
                             transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Left(arangodb::aql::ExpressionContext*, transaction::Methods*,
                       VPackFunctionParameters const&);
  static AqlValue Right(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Trim(arangodb::aql::ExpressionContext*, transaction::Methods*,
                       VPackFunctionParameters const&);
  static AqlValue LTrim(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue RTrim(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Split(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Like(arangodb::aql::ExpressionContext*, transaction::Methods*,
                       VPackFunctionParameters const&);
  static AqlValue RegexMatches(arangodb::aql::ExpressionContext*,
                               transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue RegexTest(arangodb::aql::ExpressionContext*,
                            transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue RegexReplace(arangodb::aql::ExpressionContext*,
                               transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue RegexSplit(arangodb::aql::ExpressionContext*,
                             transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue ToBase64(arangodb::aql::ExpressionContext*,
                           transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue ToHex(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue EncodeURIComponent(arangodb::aql::ExpressionContext*,
                                     transaction::Methods*,
                                     VPackFunctionParameters const&);
  static AqlValue Uuid(arangodb::aql::ExpressionContext*, transaction::Methods*,
                       VPackFunctionParameters const&);
  static AqlValue Soundex(arangodb::aql::ExpressionContext*,
                          transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue LevenshteinDistance(arangodb::aql::ExpressionContext*,
                                      transaction::Methods*,
                                      VPackFunctionParameters const&);
  static AqlValue LevenshteinMatch(arangodb::aql::ExpressionContext*,
                                      transaction::Methods*,
                                      VPackFunctionParameters const&);
  static AqlValue NgramSimilarity(ExpressionContext*, transaction::Methods*,
                                  VPackFunctionParameters const&);
  static AqlValue NgramPositionalSimilarity(ExpressionContext* ctx,
                                            transaction::Methods*,
                                            VPackFunctionParameters const&);
  static AqlValue NgramMatch(ExpressionContext*, transaction::Methods*,
                             VPackFunctionParameters const&);
  static AqlValue InRange(ExpressionContext*, transaction::Methods*,
                          VPackFunctionParameters const&);
  // Date
  static AqlValue DateNow(arangodb::aql::ExpressionContext*,
                          transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue DateIso8601(arangodb::aql::ExpressionContext*,
                              transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue DateTimestamp(arangodb::aql::ExpressionContext*,
                                transaction::Methods*, VPackFunctionParameters const&);
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
                               transaction::Methods* trx,
                               VPackFunctionParameters const& params);
  static AqlValue DateDayOfWeek(arangodb::aql::ExpressionContext*,
                                transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue DateYear(arangodb::aql::ExpressionContext*,
                           transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue DateMonth(arangodb::aql::ExpressionContext*,
                            transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue DateDay(arangodb::aql::ExpressionContext*,
                          transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue DateHour(arangodb::aql::ExpressionContext*,
                           transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue DateMinute(arangodb::aql::ExpressionContext*,
                             transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue DateSecond(arangodb::aql::ExpressionContext*,
                             transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue DateMillisecond(arangodb::aql::ExpressionContext*,
                                  transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue DateDayOfYear(arangodb::aql::ExpressionContext*,
                                transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue DateIsoWeek(arangodb::aql::ExpressionContext*,
                              transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue DateLeapYear(arangodb::aql::ExpressionContext*,
                               transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue DateQuarter(arangodb::aql::ExpressionContext*,
                              transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue DateDaysInMonth(arangodb::aql::ExpressionContext*,
                                  transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue DateTrunc(arangodb::aql::ExpressionContext*,
                            transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue DateAdd(arangodb::aql::ExpressionContext*,
                          transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue DateSubtract(arangodb::aql::ExpressionContext*,
                               transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue DateDiff(arangodb::aql::ExpressionContext*,
                           transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue DateRound(arangodb::aql::ExpressionContext*,
                            transaction::Methods*, VPackFunctionParameters const&);
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
                              transaction::Methods* trx,
                              VPackFunctionParameters const& params);

  static AqlValue DateFormat(arangodb::aql::ExpressionContext* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& params);
  static AqlValue Passthru(arangodb::aql::ExpressionContext*,
                           transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Unset(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue UnsetRecursive(arangodb::aql::ExpressionContext*,
                                 transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Keep(arangodb::aql::ExpressionContext*, transaction::Methods*,
                       VPackFunctionParameters const&);
  static AqlValue Translate(arangodb::aql::ExpressionContext*,
                            transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Merge(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue MergeRecursive(arangodb::aql::ExpressionContext*,
                                 transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Has(arangodb::aql::ExpressionContext*, transaction::Methods*,
                      VPackFunctionParameters const&);
  static AqlValue Attributes(arangodb::aql::ExpressionContext*,
                             transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Values(arangodb::aql::ExpressionContext*,
                         transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Min(arangodb::aql::ExpressionContext*, transaction::Methods*,
                      VPackFunctionParameters const&);
  static AqlValue Max(arangodb::aql::ExpressionContext*, transaction::Methods*,
                      VPackFunctionParameters const&);
  static AqlValue Sum(arangodb::aql::ExpressionContext*, transaction::Methods*,
                      VPackFunctionParameters const&);
  static AqlValue Average(arangodb::aql::ExpressionContext*,
                          transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Product(arangodb::aql::ExpressionContext*, transaction::Methods*,
                          VPackFunctionParameters const&);
  static AqlValue Sleep(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Collections(arangodb::aql::ExpressionContext*,
                              transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue RandomToken(arangodb::aql::ExpressionContext*,
                              transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue IpV4FromNumber(arangodb::aql::ExpressionContext*, transaction::Methods*,
                                 VPackFunctionParameters const&);
  static AqlValue IpV4ToNumber(arangodb::aql::ExpressionContext*, transaction::Methods*,
                               VPackFunctionParameters const&);
  static AqlValue IsIpV4(arangodb::aql::ExpressionContext*, transaction::Methods*,
                         VPackFunctionParameters const&);
  static AqlValue Md5(arangodb::aql::ExpressionContext*, transaction::Methods*,
                      VPackFunctionParameters const&);
  static AqlValue Sha1(arangodb::aql::ExpressionContext*, transaction::Methods*,
                       VPackFunctionParameters const&);
  static AqlValue Sha512(arangodb::aql::ExpressionContext*,
                         transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Crc32(arangodb::aql::ExpressionContext*, transaction::Methods*,
                        VPackFunctionParameters const&);
  static AqlValue Fnv64(arangodb::aql::ExpressionContext*, transaction::Methods*,
                        VPackFunctionParameters const&);
  static AqlValue Hash(arangodb::aql::ExpressionContext*, transaction::Methods*,
                       VPackFunctionParameters const&);
  static AqlValue IsKey(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue CountDistinct(arangodb::aql::ExpressionContext*,
                                transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue CheckDocument(arangodb::aql::ExpressionContext*,
                                transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Unique(arangodb::aql::ExpressionContext*,
                         transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue SortedUnique(arangodb::aql::ExpressionContext*,
                               transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Sorted(arangodb::aql::ExpressionContext*,
                         transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Union(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue UnionDistinct(arangodb::aql::ExpressionContext*,
                                transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Intersection(arangodb::aql::ExpressionContext*,
                               transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Outersection(arangodb::aql::ExpressionContext*,
                               transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Jaccard(arangodb::aql::ExpressionContext*,
                          transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Distance(arangodb::aql::ExpressionContext*,
                           transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue GeoDistance(arangodb::aql::ExpressionContext*,
                              transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue GeoContains(arangodb::aql::ExpressionContext*,
                              transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue GeoIntersects(arangodb::aql::ExpressionContext*,
                                transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue GeoEquals(arangodb::aql::ExpressionContext*,
                            transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue GeoArea(arangodb::aql::ExpressionContext*,
                          transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue IsInPolygon(arangodb::aql::ExpressionContext*,
                              transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue GeoPoint(arangodb::aql::ExpressionContext*,
                           transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue GeoMultiPoint(arangodb::aql::ExpressionContext*,
                                transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue GeoPolygon(arangodb::aql::ExpressionContext*,
                             transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue GeoMultiPolygon(arangodb::aql::ExpressionContext*,
                                  transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue GeoLinestring(arangodb::aql::ExpressionContext*,
                                transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue GeoMultiLinestring(arangodb::aql::ExpressionContext*,
                                     transaction::Methods*,
                                     VPackFunctionParameters const&);
  static AqlValue Flatten(arangodb::aql::ExpressionContext*,
                          transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Zip(arangodb::aql::ExpressionContext*, transaction::Methods*,
                      VPackFunctionParameters const&);
  static AqlValue JsonStringify(arangodb::aql::ExpressionContext*,
                                transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue JsonParse(arangodb::aql::ExpressionContext*,
                            transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue ParseIdentifier(arangodb::aql::ExpressionContext*,
                                  transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Slice(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Minus(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Document(arangodb::aql::ExpressionContext*,
                           transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Matches(arangodb::aql::ExpressionContext*,
                          transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Round(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Abs(arangodb::aql::ExpressionContext*, transaction::Methods*,
                      VPackFunctionParameters const&);
  static AqlValue Ceil(arangodb::aql::ExpressionContext*, transaction::Methods*,
                       VPackFunctionParameters const&);
  static AqlValue Floor(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Sqrt(arangodb::aql::ExpressionContext*, transaction::Methods*,
                       VPackFunctionParameters const&);
  static AqlValue Pow(arangodb::aql::ExpressionContext*, transaction::Methods*,
                      VPackFunctionParameters const&);
  static AqlValue Log(arangodb::aql::ExpressionContext*, transaction::Methods*,
                      VPackFunctionParameters const&);
  static AqlValue Log2(arangodb::aql::ExpressionContext*, transaction::Methods*,
                       VPackFunctionParameters const&);
  static AqlValue Log10(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Exp(arangodb::aql::ExpressionContext*, transaction::Methods*,
                      VPackFunctionParameters const&);
  static AqlValue Exp2(arangodb::aql::ExpressionContext*, transaction::Methods*,
                       VPackFunctionParameters const&);
  static AqlValue Sin(arangodb::aql::ExpressionContext*, transaction::Methods*,
                      VPackFunctionParameters const&);
  static AqlValue Cos(arangodb::aql::ExpressionContext*, transaction::Methods*,
                      VPackFunctionParameters const&);
  static AqlValue Tan(arangodb::aql::ExpressionContext*, transaction::Methods*,
                      VPackFunctionParameters const&);
  static AqlValue Asin(arangodb::aql::ExpressionContext*, transaction::Methods*,
                       VPackFunctionParameters const&);
  static AqlValue Acos(arangodb::aql::ExpressionContext*, transaction::Methods*,
                       VPackFunctionParameters const&);
  static AqlValue Atan(arangodb::aql::ExpressionContext*, transaction::Methods*,
                       VPackFunctionParameters const&);
  static AqlValue Atan2(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Radians(arangodb::aql::ExpressionContext*,
                          transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Degrees(arangodb::aql::ExpressionContext*,
                          transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Pi(arangodb::aql::ExpressionContext*, transaction::Methods*,
                     VPackFunctionParameters const&);
  static AqlValue Rand(arangodb::aql::ExpressionContext*, transaction::Methods*,
                       VPackFunctionParameters const&);
  static AqlValue FirstDocument(arangodb::aql::ExpressionContext*,
                                transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue FirstList(arangodb::aql::ExpressionContext*,
                            transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Push(arangodb::aql::ExpressionContext*, transaction::Methods*,
                       VPackFunctionParameters const&);
  static AqlValue Pop(arangodb::aql::ExpressionContext*, transaction::Methods*,
                      VPackFunctionParameters const&);
  static AqlValue Append(arangodb::aql::ExpressionContext*,
                         transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Unshift(arangodb::aql::ExpressionContext*,
                          transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Shift(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue RemoveValue(arangodb::aql::ExpressionContext*,
                              transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue RemoveValues(arangodb::aql::ExpressionContext*,
                               transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue RemoveNth(arangodb::aql::ExpressionContext*,
                            transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue ReplaceNth(arangodb::aql::ExpressionContext*,
                             transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Interleave(arangodb::aql::ExpressionContext*,
                             transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue NotNull(arangodb::aql::ExpressionContext*,
                          transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue CurrentDatabase(arangodb::aql::ExpressionContext*,
                                  transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue CollectionCount(arangodb::aql::ExpressionContext*,
                                  transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue VarianceSample(arangodb::aql::ExpressionContext*,
                                 transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue PregelResult(arangodb::aql::ExpressionContext*,
                               transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue VariancePopulation(arangodb::aql::ExpressionContext*,
                                     transaction::Methods*,
                                     VPackFunctionParameters const&);
  static AqlValue StdDevSample(arangodb::aql::ExpressionContext*,
                               transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue StdDevPopulation(arangodb::aql::ExpressionContext*,
                                   transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Median(arangodb::aql::ExpressionContext*,
                         transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Percentile(arangodb::aql::ExpressionContext*,
                             transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Range(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Position(arangodb::aql::ExpressionContext*,
                           transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Call(arangodb::aql::ExpressionContext*, transaction::Methods*,
                       VPackFunctionParameters const&);
  static AqlValue Apply(arangodb::aql::ExpressionContext*,
                        transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Version(arangodb::aql::ExpressionContext*,
                          transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue IsSameCollection(arangodb::aql::ExpressionContext*,
                                   transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Assert(arangodb::aql::ExpressionContext*,
                         transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue Warn(arangodb::aql::ExpressionContext*, transaction::Methods*,
                       VPackFunctionParameters const&);
  static AqlValue Fail(arangodb::aql::ExpressionContext*, transaction::Methods*,
                       VPackFunctionParameters const&);
  static AqlValue DecodeRev(arangodb::aql::ExpressionContext*,
                            transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue CurrentUser(arangodb::aql::ExpressionContext*,
                              transaction::Methods*, VPackFunctionParameters const&);

  static AqlValue SchemaGet(arangodb::aql::ExpressionContext*,
                            transaction::Methods*, VPackFunctionParameters const&);
  static AqlValue SchemaValidate(arangodb::aql::ExpressionContext*,
                                 transaction::Methods*, VPackFunctionParameters const&);

  /// @brief dummy function that will only throw an error when called
  static AqlValue NotImplemented(arangodb::aql::ExpressionContext*,
                                 transaction::Methods*, VPackFunctionParameters const&);
};

}  // namespace aql
}  // namespace arangodb

#endif
