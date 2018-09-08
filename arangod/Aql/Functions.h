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

#include "Basics/datetime.h"
#include "Basics/Common.h"
#include "Basics/SmallVector.h"
#include "Aql/AqlValue.h"

namespace arangodb {
namespace transaction {
class Methods;
}

namespace basics {
class VPackStringBufferAdapter;
}

namespace aql {

class Query;

typedef SmallVector<AqlValue> VPackFunctionParameters;

typedef std::function<AqlValue(arangodb::aql::Query*, transaction::Methods*,
                                VPackFunctionParameters const&)>
    FunctionImplementation;

struct Functions {

 public:
   static void init();

   /// @brief extract a function parameter from the arguments
   static AqlValue ExtractFunctionParameterValue(
       VPackFunctionParameters const& parameters, size_t position);

   /// @brief helper function. not callable as a "normal" AQL function
   static void Stringify(transaction::Methods* trx,
                         arangodb::basics::VPackStringBufferAdapter& buffer,
                         VPackSlice const& slice);

   static AqlValue IsNull(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
   static AqlValue IsBool(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
   static AqlValue IsNumber(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&);
   static AqlValue IsString(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&);
   static AqlValue IsArray(arangodb::aql::Query*, transaction::Methods*,
                           VPackFunctionParameters const&);
   static AqlValue IsObject(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&);
   static AqlValue Typename(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&);
   static AqlValue ToNumber(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&);
   static AqlValue ToString(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&);
   static AqlValue ToBool(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
   static AqlValue ToArray(arangodb::aql::Query*, transaction::Methods*,
                           VPackFunctionParameters const&);
   static AqlValue Length(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
   static AqlValue FindFirst(arangodb::aql::Query*, transaction::Methods*,
                             VPackFunctionParameters const&);
   static AqlValue FindLast(arangodb::aql::Query*, transaction::Methods*,
                             VPackFunctionParameters const&);
   static AqlValue Reverse(arangodb::aql::Query*, transaction::Methods*,
                           VPackFunctionParameters const&);
   static AqlValue First(arangodb::aql::Query*, transaction::Methods*,
                         VPackFunctionParameters const&);
   static AqlValue Last(arangodb::aql::Query*, transaction::Methods*,
                        VPackFunctionParameters const&);
   static AqlValue Nth(arangodb::aql::Query*, transaction::Methods*,
                       VPackFunctionParameters const&);
   static AqlValue Contains(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&);
   static AqlValue Concat(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
   static AqlValue ConcatSeparator(arangodb::aql::Query*,
                                   transaction::Methods*,
                                   VPackFunctionParameters const&);
   static AqlValue CharLength(arangodb::aql::Query*,
                              transaction::Methods*,
                              VPackFunctionParameters const&);
   static AqlValue Lower(arangodb::aql::Query*,
                              transaction::Methods*,
                              VPackFunctionParameters const&);
   static AqlValue Upper(arangodb::aql::Query*,
                              transaction::Methods*,
                              VPackFunctionParameters const&);
   static AqlValue Substring(arangodb::aql::Query*,
                              transaction::Methods*,
                              VPackFunctionParameters const&);
   static AqlValue Substitute(arangodb::aql::Query*,
                              transaction::Methods*,
                              VPackFunctionParameters const&);
   static AqlValue Left(arangodb::aql::Query*,
                         transaction::Methods*,
                         VPackFunctionParameters const&);
   static AqlValue Right(arangodb::aql::Query*,
                         transaction::Methods*,
                         VPackFunctionParameters const&);
   static AqlValue Trim(arangodb::aql::Query*,
                        transaction::Methods*,
                        VPackFunctionParameters const&);
   static AqlValue LTrim(arangodb::aql::Query*,
                         transaction::Methods*,
                         VPackFunctionParameters const&);
   static AqlValue RTrim(arangodb::aql::Query*,
                         transaction::Methods*,
                         VPackFunctionParameters const&);
   static AqlValue Split(arangodb::aql::Query*, transaction::Methods*,
                         VPackFunctionParameters const&);
   static AqlValue Like(arangodb::aql::Query*, transaction::Methods*,
                        VPackFunctionParameters const&);
    static AqlValue RegexMatches(arangodb::aql::Query*, transaction::Methods*,
                                 VPackFunctionParameters const&);
   static AqlValue RegexTest(arangodb::aql::Query*, transaction::Methods*,
                             VPackFunctionParameters const&);
   static AqlValue RegexReplace(arangodb::aql::Query*, transaction::Methods*,
                                VPackFunctionParameters const&);
   static AqlValue RegexSplit(arangodb::aql::Query*, transaction::Methods*,
                                VPackFunctionParameters const&);
   static AqlValue ToBase64(arangodb::aql::Query*,transaction::Methods*,
                            VPackFunctionParameters const&);
   static AqlValue ToHex(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
   static AqlValue EncodeURIComponent(arangodb::aql::Query*, transaction::Methods*,
                                       VPackFunctionParameters const&);
   static AqlValue Uuid(arangodb::aql::Query*, transaction::Methods*,
                                       VPackFunctionParameters const&);
   static AqlValue Soundex(arangodb::aql::Query*, transaction::Methods*,
                         VPackFunctionParameters const&);
   static AqlValue LevenshteinDistance(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&);
   // Date
   static AqlValue DateNow(arangodb::aql::Query*, transaction::Methods*,
                                VPackFunctionParameters const&);
   static AqlValue DateIso8601(arangodb::aql::Query*, transaction::Methods*,
                               VPackFunctionParameters const&);
   static AqlValue DateTimestamp(arangodb::aql::Query*, transaction::Methods*,
                                 VPackFunctionParameters const&);
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
   static AqlValue IsDatestring(arangodb::aql::Query* query,
                                transaction::Methods* trx,
                                VPackFunctionParameters const& params);
   static AqlValue DateDayOfWeek(arangodb::aql::Query*, transaction::Methods*,
                                 VPackFunctionParameters const&);
   static AqlValue DateYear(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&);
   static AqlValue DateMonth(arangodb::aql::Query*, transaction::Methods*,
                             VPackFunctionParameters const&);
   static AqlValue DateDay(arangodb::aql::Query*, transaction::Methods*,
                           VPackFunctionParameters const&);
   static AqlValue DateHour(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&);
   static AqlValue DateMinute(arangodb::aql::Query*, transaction::Methods*,
                              VPackFunctionParameters const&);
   static AqlValue DateSecond(arangodb::aql::Query*, transaction::Methods*,
                              VPackFunctionParameters const&);
   static AqlValue DateMillisecond(arangodb::aql::Query*, transaction::Methods*,
                                   VPackFunctionParameters const&);
   static AqlValue DateDayOfYear(arangodb::aql::Query*, transaction::Methods*,
                                 VPackFunctionParameters const&);
   static AqlValue DateIsoWeek(arangodb::aql::Query*, transaction::Methods*,
                               VPackFunctionParameters const&);
    static AqlValue DateLeapYear(arangodb::aql::Query*, transaction::Methods*,
                                 VPackFunctionParameters const&);
    static AqlValue DateQuarter(arangodb::aql::Query*, transaction::Methods*,
                                VPackFunctionParameters const&);
    static AqlValue DateDaysInMonth(arangodb::aql::Query*, transaction::Methods*,
                                    VPackFunctionParameters const&);
    static AqlValue DateTrunc(arangodb::aql::Query*, transaction::Methods*,
                              VPackFunctionParameters const&);
    static AqlValue DateAdd(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&);
    static AqlValue DateSubtract(arangodb::aql::Query*, transaction::Methods*,
                                 VPackFunctionParameters const&);
    static AqlValue DateDiff(arangodb::aql::Query*, transaction::Methods*,
                             VPackFunctionParameters const&);
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
    static AqlValue DateCompare(arangodb::aql::Query* query,
                                transaction::Methods* trx,
                                VPackFunctionParameters const& params);

    static AqlValue DateFormat(arangodb::aql::Query* query,
                               transaction::Methods* trx,
                               VPackFunctionParameters const& params);
    static AqlValue Passthru(arangodb::aql::Query*, transaction::Methods*,
                             VPackFunctionParameters const&);
    static AqlValue Unset(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
    static AqlValue UnsetRecursive(arangodb::aql::Query*, transaction::Methods*,
                                   VPackFunctionParameters const&);
    static AqlValue Keep(arangodb::aql::Query*, transaction::Methods*,
                         VPackFunctionParameters const&);
    static AqlValue Translate(arangodb::aql::Query*, transaction::Methods*,
                              VPackFunctionParameters const&);
    static AqlValue Merge(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
    static AqlValue MergeRecursive(arangodb::aql::Query*, transaction::Methods*,
                                   VPackFunctionParameters const&);
    static AqlValue Has(arangodb::aql::Query*, transaction::Methods*,
                        VPackFunctionParameters const&);
    static AqlValue Attributes(arangodb::aql::Query*, transaction::Methods*,
                               VPackFunctionParameters const&);
    static AqlValue Values(arangodb::aql::Query*, transaction::Methods*,
                           VPackFunctionParameters const&);
    static AqlValue Min(arangodb::aql::Query*, transaction::Methods*,
                        VPackFunctionParameters const&);
    static AqlValue Max(arangodb::aql::Query*, transaction::Methods*,
                        VPackFunctionParameters const&);
    static AqlValue Sum(arangodb::aql::Query*, transaction::Methods*,
                        VPackFunctionParameters const&);
    static AqlValue Average(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&);
    static AqlValue Sleep(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
    static AqlValue Collections(arangodb::aql::Query*, transaction::Methods*,
                                VPackFunctionParameters const&);
    static AqlValue RandomToken(arangodb::aql::Query*, transaction::Methods*,
                                VPackFunctionParameters const&);
    static AqlValue Md5(arangodb::aql::Query*, transaction::Methods*,
                        VPackFunctionParameters const&);
    static AqlValue Sha1(arangodb::aql::Query*, transaction::Methods*,
                         VPackFunctionParameters const&);
    static AqlValue Sha512(arangodb::aql::Query*, transaction::Methods*,
                           VPackFunctionParameters const&);
    static AqlValue Hash(arangodb::aql::Query*, transaction::Methods*,
                         VPackFunctionParameters const&);
    static AqlValue IsKey(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
    static AqlValue CountDistinct(arangodb::aql::Query*, transaction::Methods*,
                                  VPackFunctionParameters const&);
    static AqlValue Unique(arangodb::aql::Query*, transaction::Methods*,
                           VPackFunctionParameters const&);
    static AqlValue SortedUnique(arangodb::aql::Query*, transaction::Methods*,
                                 VPackFunctionParameters const&);
    static AqlValue Sorted(arangodb::aql::Query*, transaction::Methods*,
                           VPackFunctionParameters const&);
    static AqlValue Union(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
    static AqlValue UnionDistinct(arangodb::aql::Query*, transaction::Methods*,
                                  VPackFunctionParameters const&);
    static AqlValue Intersection(arangodb::aql::Query*, transaction::Methods*,
                                 VPackFunctionParameters const&);
    static AqlValue Outersection(arangodb::aql::Query*, transaction::Methods*,
                                 VPackFunctionParameters const&);
    static AqlValue Distance(arangodb::aql::Query*, transaction::Methods*,
                             VPackFunctionParameters const&);
    static AqlValue GeoDistance(arangodb::aql::Query*, transaction::Methods*,
                                VPackFunctionParameters const&);
    static AqlValue GeoContains(arangodb::aql::Query*, transaction::Methods*,
                                VPackFunctionParameters const&);
    static AqlValue GeoIntersects(arangodb::aql::Query*, transaction::Methods*,
                                  VPackFunctionParameters const&);
    static AqlValue GeoEquals(arangodb::aql::Query*, transaction::Methods*,
                              VPackFunctionParameters const&);
    static AqlValue IsInPolygon(arangodb::aql::Query*, transaction::Methods*,
                                VPackFunctionParameters const&);
    static AqlValue GeoPoint(arangodb::aql::Query*, transaction::Methods*,
                             VPackFunctionParameters const&);
    static AqlValue GeoMultiPoint(arangodb::aql::Query*, transaction::Methods*,
                                  VPackFunctionParameters const&);
    static AqlValue GeoPolygon(arangodb::aql::Query*, transaction::Methods*,
                               VPackFunctionParameters const&);
    static AqlValue GeoLinestring(arangodb::aql::Query*, transaction::Methods*,
                                  VPackFunctionParameters const&);
    static AqlValue GeoMultiLinestring(arangodb::aql::Query*, transaction::Methods*,
                                       VPackFunctionParameters const&);
    static AqlValue Flatten(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&);
    static AqlValue Zip(arangodb::aql::Query*, transaction::Methods*,
                        VPackFunctionParameters const&);
    static AqlValue JsonStringify(arangodb::aql::Query*, transaction::Methods*,
                                  VPackFunctionParameters const&);
    static AqlValue JsonParse(arangodb::aql::Query*, transaction::Methods*,
                              VPackFunctionParameters const&);
    static AqlValue ParseIdentifier(arangodb::aql::Query*,
                                    transaction::Methods*,
                                    VPackFunctionParameters const&);
    static AqlValue Slice(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
    static AqlValue Minus(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
    static AqlValue Document(arangodb::aql::Query*, transaction::Methods*,
                             VPackFunctionParameters const&);
    static AqlValue Matches(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&);
    static AqlValue Round(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
    static AqlValue Abs(arangodb::aql::Query*, transaction::Methods*,
                        VPackFunctionParameters const&);
    static AqlValue Ceil(arangodb::aql::Query*, transaction::Methods*,
                         VPackFunctionParameters const&);
    static AqlValue Floor(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
    static AqlValue Sqrt(arangodb::aql::Query*, transaction::Methods*,
                         VPackFunctionParameters const&);
    static AqlValue Pow(arangodb::aql::Query*, transaction::Methods*,
                        VPackFunctionParameters const&);
    static AqlValue Log(arangodb::aql::Query*, transaction::Methods*,
                        VPackFunctionParameters const&);
    static AqlValue Log2(arangodb::aql::Query*, transaction::Methods*,
                         VPackFunctionParameters const&);
    static AqlValue Log10(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
    static AqlValue Exp(arangodb::aql::Query*, transaction::Methods*,
                        VPackFunctionParameters const&);
    static AqlValue Exp2(arangodb::aql::Query*, transaction::Methods*,
                         VPackFunctionParameters const&);
    static AqlValue Sin(arangodb::aql::Query*, transaction::Methods*,
                        VPackFunctionParameters const&);
    static AqlValue Cos(arangodb::aql::Query*, transaction::Methods*,
                        VPackFunctionParameters const&);
    static AqlValue Tan(arangodb::aql::Query*, transaction::Methods*,
                        VPackFunctionParameters const&);
    static AqlValue Asin(arangodb::aql::Query*, transaction::Methods*,
                         VPackFunctionParameters const&);
    static AqlValue Acos(arangodb::aql::Query*, transaction::Methods*,
                         VPackFunctionParameters const&);
    static AqlValue Atan(arangodb::aql::Query*, transaction::Methods*,
                         VPackFunctionParameters const&);
    static AqlValue Atan2(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
    static AqlValue Radians(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&);
    static AqlValue Degrees(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&);
    static AqlValue Pi(arangodb::aql::Query*, transaction::Methods*,
                       VPackFunctionParameters const&);
    static AqlValue Rand(arangodb::aql::Query*, transaction::Methods*,
                         VPackFunctionParameters const&);
    static AqlValue FirstDocument(arangodb::aql::Query*, transaction::Methods*,
                                  VPackFunctionParameters const&);
    static AqlValue FirstList(arangodb::aql::Query*, transaction::Methods*,
                              VPackFunctionParameters const&);
    static AqlValue Push(arangodb::aql::Query*, transaction::Methods*,
                         VPackFunctionParameters const&);
    static AqlValue Pop(arangodb::aql::Query*, transaction::Methods*,
                        VPackFunctionParameters const&);
    static AqlValue Append(arangodb::aql::Query*, transaction::Methods*,
                           VPackFunctionParameters const&);
    static AqlValue Unshift(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&);
    static AqlValue Shift(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
    static AqlValue RemoveValue(arangodb::aql::Query*, transaction::Methods*,
                                VPackFunctionParameters const&);
    static AqlValue RemoveValues(arangodb::aql::Query*, transaction::Methods*,
                                 VPackFunctionParameters const&);
    static AqlValue RemoveNth(arangodb::aql::Query*, transaction::Methods*,
                              VPackFunctionParameters const&);
    static AqlValue NotNull(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&);
    static AqlValue CurrentDatabase(arangodb::aql::Query*,
                                    transaction::Methods*,
                                    VPackFunctionParameters const&);
    static AqlValue CollectionCount(arangodb::aql::Query*,
                                    transaction::Methods*,
                                    VPackFunctionParameters const&);
    static AqlValue VarianceSample(arangodb::aql::Query*, transaction::Methods*,
                                   VPackFunctionParameters const&);
    static AqlValue PregelResult(arangodb::aql::Query*, transaction::Methods*,
                                 VPackFunctionParameters const&);
    static AqlValue VariancePopulation(arangodb::aql::Query*,
                                       transaction::Methods*,
                                       VPackFunctionParameters const&);
    static AqlValue StdDevSample(arangodb::aql::Query*, transaction::Methods*,
                                 VPackFunctionParameters const&);
    static AqlValue StdDevPopulation(arangodb::aql::Query*,
                                     transaction::Methods*,
                                     VPackFunctionParameters const&);
    static AqlValue Median(arangodb::aql::Query*, transaction::Methods*,
                           VPackFunctionParameters const&);
    static AqlValue Percentile(arangodb::aql::Query*, transaction::Methods*,
                               VPackFunctionParameters const&);
    static AqlValue Range(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
    static AqlValue Position(arangodb::aql::Query*, transaction::Methods*,
                             VPackFunctionParameters const&);
    static AqlValue Call(arangodb::aql::Query*, transaction::Methods*,
                         VPackFunctionParameters const&);
    static AqlValue Apply(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
    static AqlValue Version(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&);
    static AqlValue IsSameCollection(arangodb::aql::Query*,
                                     transaction::Methods*,
                                     VPackFunctionParameters const&);
    static AqlValue Assert(arangodb::aql::Query*, transaction::Methods*,
                           VPackFunctionParameters const&);
    static AqlValue Warn(arangodb::aql::Query*, transaction::Methods*,
                         VPackFunctionParameters const&);
    static AqlValue Fail(arangodb::aql::Query*, transaction::Methods*,
                         VPackFunctionParameters const&);

    static AqlValue CurrentUser(arangodb::aql::Query*,
                                transaction::Methods*,
                                VPackFunctionParameters const&);

    /// @brief dummy function that will only throw an error when called
    static AqlValue NotImplemented(arangodb::aql::Query*, transaction::Methods*,
                                   VPackFunctionParameters const&);
};

}
}

#endif
