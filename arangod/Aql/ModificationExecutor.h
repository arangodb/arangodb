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

#include <boost/optional.hpp>

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

using wrap = std::reference_wrapper<boost::optional<RegisterId>>;
inline std::shared_ptr<std::unordered_set<RegisterId>> makeSet(std::initializer_list<wrap> reg_wrap) {
  auto rv = make_shared_unordered_set();
  for (auto wrap : reg_wrap) {
    auto const& opt = wrap.get();
    if (opt.has_value()) {
      rv->insert(opt.get());
    }
  }
  return rv;
}

class ModificationExecutorInfos : public ExecutorInfos {
 public:
  ModificationExecutorInfos(boost::optional<RegisterId> input1RegisterId,
                            boost::optional<RegisterId> input2RegisterId,
                            boost::optional<RegisterId> input3RegisterId,
                            boost::optional<RegisterId> outputNewRegisterId,
                            boost::optional<RegisterId> outputOldRegisterId,
                            RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                            std::unordered_set<RegisterId> registersToClear,
                            std::unordered_set<RegisterId> registersToKeep,
                            transaction::Methods* trx, OperationOptions options,
                            aql::Collection const* aqlCollection,
                            bool readCompleteInput, bool producesResults,
                            bool consultAqlWriteFilter, bool ignoreErrors,
                            bool doCount, /*bool returnInheritedResults,*/
                            bool isReplace, bool ignoreDocumentNotFound)
      : ExecutorInfos(makeSet({wrap(input1RegisterId), wrap(input2RegisterId),
                               wrap(input3RegisterId)}) /*input registers*/,
                      makeSet({wrap(outputOldRegisterId), wrap(outputNewRegisterId)}) /*output registers*/,
                      nrInputRegisters, nrOutputRegisters,
                      std::move(registersToClear), std::move(registersToKeep)),
        _trx(trx),
        _options(options),
        _aqlCollection(aqlCollection),
        _readCompleteInput(readCompleteInput),
        _producesResults(producesResults || !_options.silent),
        _consultAqlWriteFilter(consultAqlWriteFilter),
        _ignoreErrors(ignoreErrors),
        _doCount(doCount),
        //_returnInheritedResults(returnInheritedResults),
        _isReplace(isReplace),
        _ignoreDocumentNotFound(ignoreDocumentNotFound),
        _input1RegisterId(input1RegisterId),
        _input2RegisterId(input2RegisterId),
        _input3RegisterId(input3RegisterId),
        _outputNewRegisterId(outputNewRegisterId),
        _outputOldRegisterId(outputOldRegisterId) {}

  ModificationExecutorInfos() = delete;
  ModificationExecutorInfos(ModificationExecutorInfos&&) = default;
  ModificationExecutorInfos(ModificationExecutorInfos const&) = delete;
  ~ModificationExecutorInfos() = default;

  /// @brief the variable produced by Return
  transaction::Methods* _trx;
  OperationOptions _options;
  aql::Collection const* _aqlCollection;
  bool _readCompleteInput;
  bool _producesResults;
  bool _consultAqlWriteFilter;
  bool _ignoreErrors;
  bool _doCount;  // count statisitics
  // bool _returnInheritedResults;
  bool _isReplace;               // needed for upsert
  bool _ignoreDocumentNotFound;  // needed for update replace

  // insert (singleinput) - upsert (inDoc) - update replace (inDoc)
  boost::optional<RegisterId> _input1RegisterId;
  // upsert (insertVar) -- update replace (keyVar)
  boost::optional<RegisterId> _input2RegisterId;
  // upsert (updateVar)
  boost::optional<RegisterId> _input3RegisterId;

  boost::optional<RegisterId> _outputNewRegisterId;
  boost::optional<RegisterId> _outputOldRegisterId;
};

struct ModificationExecutorBase {
  struct Properties {
    static const bool preservesOrder = true;
    static const bool allowsBlockPassthrough = false;
    static const bool inputSizeRestrictsOutputSize =
        false;  // Disabled because prefetch does not work in the Cluster
                // Maybe This should ask for a 1:1 relation.
  };
  using Infos = ModificationExecutorInfos;
  using Fetcher = SingleBlockFetcher<Properties::allowsBlockPassthrough>;
  using Stats = ModificationStats;

  ModificationExecutorBase(Fetcher&, Infos&);

 protected:
  ModificationExecutorInfos& _infos;
  Fetcher& _fetcher;
  bool _prepared = false;
};

template <typename Modifier>
class ModificationExecutor : public ModificationExecutorBase {
  friend struct Insert;
  friend struct Remove;
  friend struct Upsert;
  friend struct UpdateReplace<Update>;
  friend struct UpdateReplace<Replace>;

 public:
  using Modification = Modifier;

  ModificationExecutor(Fetcher&, Infos&);
  ~ModificationExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);

 private:
  Modifier _modifier;
};

}  // namespace aql
}  // namespace arangodb

#endif
