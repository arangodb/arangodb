////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_MODIFICATION_EXECUTOR_INFOS_H
#define ARANGOD_AQL_MODIFICATION_EXECUTOR_INFOS_H

#include "Aql/Collection.h"
#include "Aql/RegisterInfos.h"
#include "Aql/RegisterPlan.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace aql {

class QueryContext;

struct BoolWrapper {
  explicit BoolWrapper(bool b) { _value = b; }
  operator bool() const noexcept { return _value; }
  bool _value;
};

struct ProducesResults : BoolWrapper {
  explicit ProducesResults(bool b) : BoolWrapper(b) {}
};
struct ConsultAqlWriteFilter : BoolWrapper {
  explicit ConsultAqlWriteFilter(bool b) : BoolWrapper(b) {}
};
struct IgnoreErrors : BoolWrapper {
  explicit IgnoreErrors(bool b) : BoolWrapper(b) {}
};
struct DoCount : BoolWrapper {
  explicit DoCount(bool b) : BoolWrapper(b) {}
};
struct IsReplace : BoolWrapper {
  explicit IsReplace(bool b) : BoolWrapper(b) {}
};
struct IgnoreDocumentNotFound : BoolWrapper {
  explicit IgnoreDocumentNotFound(bool b) : BoolWrapper(b) {}
};

struct ModificationExecutorInfos {
  ModificationExecutorInfos(RegisterId input1RegisterId, RegisterId input2RegisterId,
                            RegisterId input3RegisterId, RegisterId outputNewRegisterId,
                            RegisterId outputOldRegisterId, RegisterId outputRegisterId,
                            arangodb::aql::QueryContext& query, OperationOptions options,
                            aql::Collection const* aqlCollection, ProducesResults producesResults,
                            ConsultAqlWriteFilter consultAqlWriteFilter,
                            IgnoreErrors ignoreErrors, DoCount doCount, IsReplace isReplace,
                            IgnoreDocumentNotFound ignoreDocumentNotFound);

  ModificationExecutorInfos() = delete;
  ModificationExecutorInfos(ModificationExecutorInfos&&) = default;
  ModificationExecutorInfos(ModificationExecutorInfos const&) = delete;
  ~ModificationExecutorInfos() = default;

  /// @brief the variable produced by Return
  arangodb::aql::QueryContext& _query;
  OperationOptions _options;
  aql::Collection const* _aqlCollection;
  ProducesResults _producesResults;
  ConsultAqlWriteFilter _consultAqlWriteFilter;
  IgnoreErrors _ignoreErrors;
  DoCount _doCount;  // count statisitics
  // bool _returnInheritedResults;
  IsReplace _isReplace;                            // needed for upsert
  IgnoreDocumentNotFound _ignoreDocumentNotFound;  // needed for update replace

  // insert (singleinput) - upsert (inDoc) - update replace (inDoc)
  RegisterId _input1RegisterId;
  // upsert (insertVar) -- update replace (keyVar)
  RegisterId _input2RegisterId;
  // upsert (updateVar)
  RegisterId _input3RegisterId;

  RegisterId _outputNewRegisterId;
  RegisterId _outputOldRegisterId;
  RegisterId _outputRegisterId;  // single remote
};

}  // namespace aql
}  // namespace arangodb

#endif
