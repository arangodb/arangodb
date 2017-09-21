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

typedef std::function<bool()> ExecutionCondition;

typedef SmallVector<AqlValue> VPackFunctionParameters;

typedef std::function<AqlValue(arangodb::aql::Query*, transaction::Methods*,
                                VPackFunctionParameters const&)>
    FunctionImplementation;

struct Functions {
  protected:

  /// @brief validate the number of parameters
   static void ValidateParameters(VPackFunctionParameters const& parameters,
                                  char const* function, int minParams,
                                  int maxParams);

   static void ValidateParameters(VPackFunctionParameters const& parameters,
                                  char const* function, int minParams);

   /// @brief extract a function parameter from the arguments
   static AqlValue ExtractFunctionParameterValue(
       transaction::Methods*, VPackFunctionParameters const& parameters,
       size_t position);

   /// @brief extra a collection name from an AqlValue
   static std::string ExtractCollectionName(
       transaction::Methods* trx, VPackFunctionParameters const& parameters,
       size_t position);

   /// @brief extract attribute names from the arguments
   static void ExtractKeys(std::unordered_set<std::string>& names,
                           arangodb::aql::Query* query,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters,
                           size_t startParameter, char const* functionName);

   /// @brief Helper function to merge given parameters
   ///        Works for an array of objects as first parameter or arbitrary many
   ///        object parameters
   static AqlValue MergeParameters(arangodb::aql::Query* query,
                                   transaction::Methods* trx,
                                   VPackFunctionParameters const& parameters,
                                   char const* funcName, bool recursive);

  public:
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
   static AqlValue Like(arangodb::aql::Query*, transaction::Methods*,
                        VPackFunctionParameters const&);
   static AqlValue RegexTest(arangodb::aql::Query*, transaction::Methods*,
                             VPackFunctionParameters const&);
   static AqlValue RegexReplace(arangodb::aql::Query*, transaction::Methods*,
                                VPackFunctionParameters const&);
   static AqlValue Passthru(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&);
   static AqlValue Unset(arangodb::aql::Query*, transaction::Methods*,
                         VPackFunctionParameters const&);
   static AqlValue UnsetRecursive(arangodb::aql::Query*, transaction::Methods*,
                                  VPackFunctionParameters const&);
   static AqlValue Keep(arangodb::aql::Query*, transaction::Methods*,
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
   static AqlValue RandomToken(arangodb::aql::Query*, transaction::Methods*,
                               VPackFunctionParameters const&);
   static AqlValue Md5(arangodb::aql::Query*, transaction::Methods*,
                       VPackFunctionParameters const&);
   static AqlValue Sha1(arangodb::aql::Query*, transaction::Methods*,
                        VPackFunctionParameters const&);
   static AqlValue Hash(arangodb::aql::Query*, transaction::Methods*,
                        VPackFunctionParameters const&);
   static AqlValue Unique(arangodb::aql::Query*, transaction::Methods*,
                          VPackFunctionParameters const&);
   static AqlValue SortedUnique(arangodb::aql::Query*, transaction::Methods*,
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
   static AqlValue IsSameCollection(arangodb::aql::Query*,
                                    transaction::Methods*,
                                    VPackFunctionParameters const&);
};
}
}

#endif
