////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_MODIFICATION_EXECUTOR_H
#define ARANGOD_AQL_MODIFICATION_EXECUTOR_H

#include "Aql/AllRowsFetcher.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/ModificationNodes.h"
#include "Aql/ModificationOptions.h"
#include "Aql/SingleBlockFetcher.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "Utils/OperationOptions.h"
#include "velocypack/Slice.h"
#include "velocypack/velocypack-aliases.h"

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

class AqlItemMatrix;
class ExecutorInfos;
class NoStats;
class OutputAqlItemRow;
struct SortRegister;

struct Insert;
struct Remove;
struct Upsert;
struct Update;
struct Replace;

template <typename>
struct UpdateReplace;

inline OperationOptions convertOptions(ModificationOptions const& in,
                                       Variable const* outVariableNew,
                                       Variable const* outVariableOld) {
  OperationOptions out;

  // commented out OperationOptions attributesare not provided
  // by the ModificationOptions or the information given by the
  // Variable pointer.

  // in.ignoreErrors;
  out.waitForSync = in.waitForSync;
  out.keepNull = !in.nullMeansRemove;
  out.mergeObjects = in.mergeObjects;
  // in.ignoreDocumentNotFound;
  // in.readCompleteInput;
  out.isRestore = in.useIsRestore;
  // in.consultAqlWriteFilter;
  // in.exclusive;
  out.overwrite = in.overwrite;
  out.ignoreRevs = in.ignoreRevs;

  out.returnNew = (outVariableNew != nullptr);
  out.returnOld = (outVariableOld != nullptr);
  out.silent = !(out.returnNew || out.returnOld);

  return out;
}

inline std::shared_ptr<std::unordered_set<RegisterId>> makeSet(std::initializer_list<RegisterId> regList) {
  auto rv = make_shared_unordered_set();
  for (RegisterId regId : regList) {
    if (regId < ExecutionNode::MaxRegisterId) {
      rv->insert(regId);
    }
  }
  return rv;
}

struct BoolWrapper {
  explicit BoolWrapper(bool b) { _value = b; }
  operator bool() { return _value; }
  bool _value;
};

struct ProducesResults : BoolWrapper {
  explicit ProducesResults(bool b) : BoolWrapper(b){}
};
struct ConsultAqlWriteFilter : BoolWrapper {
  explicit ConsultAqlWriteFilter(bool b) : BoolWrapper(b){}
};
struct IgnoreErrors : BoolWrapper {
  explicit IgnoreErrors(bool b) : BoolWrapper(b){}
};
struct DoCount : BoolWrapper {
  explicit DoCount(bool b) : BoolWrapper(b){}
};
struct IsReplace : BoolWrapper {
  explicit IsReplace(bool b) : BoolWrapper(b){}
};

struct IgnoreDocumentNotFound : BoolWrapper {
  explicit IgnoreDocumentNotFound(bool b) : BoolWrapper(b){}
};

struct ModificationExecutorInfos : public ExecutorInfos {
  ModificationExecutorInfos(
      RegisterId input1RegisterId, RegisterId input2RegisterId, RegisterId input3RegisterId,
      RegisterId outputNewRegisterId, RegisterId outputOldRegisterId,
      RegisterId outputRegisterId, RegisterId nrInputRegisters,
      RegisterId nrOutputRegisters, std::unordered_set<RegisterId> registersToClear,
      std::unordered_set<RegisterId> registersToKeep, transaction::Methods* trx,
      OperationOptions options, aql::Collection const* aqlCollection,
      ProducesResults producesResults, ConsultAqlWriteFilter consultAqlWriteFilter,
      IgnoreErrors ignoreErrors, DoCount doCount, IsReplace isReplace,
      IgnoreDocumentNotFound ignoreDocumentNotFound)
      : ExecutorInfos(makeSet({input1RegisterId, input2RegisterId, input3RegisterId}) /*input registers*/,
                      makeSet({outputOldRegisterId, outputNewRegisterId, outputRegisterId}) /*output registers*/,
                      nrInputRegisters, nrOutputRegisters,
                      std::move(registersToClear), std::move(registersToKeep)),
        _trx(trx),
        _options(options),
        _aqlCollection(aqlCollection),
        _producesResults(ProducesResults(producesResults._value || !_options.silent)),
        _consultAqlWriteFilter(consultAqlWriteFilter),
        _ignoreErrors(ignoreErrors),
        _doCount(doCount),
        _isReplace(isReplace),
        _ignoreDocumentNotFound(ignoreDocumentNotFound),
        _input1RegisterId(input1RegisterId),
        _input2RegisterId(input2RegisterId),
        _input3RegisterId(input3RegisterId),
        _outputNewRegisterId(outputNewRegisterId),
        _outputOldRegisterId(outputOldRegisterId),
        _outputRegisterId(outputRegisterId) {}

  ModificationExecutorInfos() = delete;
  ModificationExecutorInfos(ModificationExecutorInfos&&) = default;
  ModificationExecutorInfos(ModificationExecutorInfos const&) = delete;
  ~ModificationExecutorInfos() = default;

  /// @brief the variable produced by Return
  transaction::Methods* _trx;
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

template <typename FetcherType>
struct ModificationExecutorBase {
  struct Properties {
    static const bool preservesOrder = true;
    static const bool allowsBlockPassthrough = false;
    static const bool inputSizeRestrictsOutputSize =
        false;  // Disabled because prefetch does not work in the Cluster
                // Maybe This should ask for a 1:1 relation.
  };
  using Infos = ModificationExecutorInfos;
  using Fetcher = FetcherType;  // SingleBlockFetcher<Properties::allowsBlockPassthrough>;
  using Stats = ModificationStats;

  ModificationExecutorBase(Fetcher&, Infos&);

 protected:
  ModificationExecutorInfos& _infos;
  Fetcher& _fetcher;
  bool _prepared = false;
};

template <typename Modifier, typename FetcherType>
class ModificationExecutor : public ModificationExecutorBase<FetcherType> {
  friend struct Insert;
  friend struct Remove;
  friend struct Upsert;
  friend struct UpdateReplace<Update>;
  friend struct UpdateReplace<Replace>;

 public:
  using Modification = Modifier;

  // pull in types from template base
  using Fetcher = typename ModificationExecutorBase<FetcherType>::Fetcher;
  using Infos = typename ModificationExecutorBase<FetcherType>::Infos;
  using Stats = typename ModificationExecutorBase<FetcherType>::Stats;

  ModificationExecutor(Fetcher&, Infos&);
  ~ModificationExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);

  /**
   * This executor immedieately  returns every actually consumed row
   * All other rows belong to the fetcher.
   */
  inline size_t numberOfRowsInFlight() const { return 0; }

 private:
  Modifier _modifier;
};

}  // namespace aql
}  // namespace arangodb

#endif
