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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_EXECUTOR_INFOS_H
#define ARANGOD_AQL_EXECUTOR_INFOS_H 1

#include "Aql/types.h"
#include "Basics/Common.h"

#include <unordered_set>

namespace arangodb {
namespace aql {

inline std::shared_ptr<std::unordered_set<RegisterId>> make_shared_unordered_set(
    std::initializer_list<RegisterId> const& list = std::initializer_list<RegisterId>{}) {
  return std::make_shared<std::unordered_set<RegisterId>>(list);
}

inline std::shared_ptr<std::unordered_set<RegisterId>> make_shared_unordered_set(RegisterId size) {
  auto set = make_shared_unordered_set();
  for (RegisterId i = 0; i < size; i++) {
    set->insert(i);
  }
  return set;
}

template <typename Iterator>
inline std::shared_ptr<std::unordered_set<RegisterId>> make_shared_unordered_set(Iterator begin,
                                                                                 Iterator end) {
  return std::make_shared<std::unordered_set<RegisterId>>(begin, end);
}

/**
 * @brief Class to be handed into Executors during construction
 *        This class should be independend from AQL internal
 *        knowledge for easy unit-testability.
 */
class ExecutorInfos {
 public:
  /**
   * @brief Input for Executors. Derived classes exist where additional
   *        input is needed.
   * @param readableInputRegisters Registers the Executor may use as input
   * @param writeableOutputRegisters Registers the Executor writes into
   * @param nrInputRegisters Width of input AqlItemBlocks
   * @param nrOutputRegisters Width of output AqlItemBlocks
   * @param registersToClear Registers that are not used after this block, so
   *                         their values can be deleted
   * @param registersToKeep Registers that will be used after this block, so
   *                        their values have to be copied
   *
   * Note that the output registers can be found in the ExecutionNode via
   * getVariablesSetHere() and translated as follows:
   *   auto it = getRegisterPlan()->varInfo.find(varSetHere->id);
   *   TRI_ASSERT(it != getRegisterPlan()->varInfo.end());
   *   RegisterId register = it->second.registerId;
   */

  ExecutorInfos(std::shared_ptr<std::unordered_set<RegisterId>> readableInputRegisters,
                std::shared_ptr<std::unordered_set<RegisterId>> writeableOutputRegisters,
                RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                std::unordered_set<RegisterId> registersToClear,
                std::unordered_set<RegisterId> registersToKeep);

  ExecutorInfos(ExecutorInfos&&) = default;
  ExecutorInfos(ExecutorInfos const&) = delete;
  ~ExecutorInfos() = default;

  /**
   * @brief Get the input registers the Executor is allowed to read. This has
   *        little to do with numberOfInputRegisters(), except that each input
   *        register index returned here is smaller than
   *        numberOfInputRegisters().
   *
   * @return The indices of the input registers.
   */
  std::shared_ptr<std::unordered_set<RegisterId> const> const getInputRegisters() const {
    return _inRegs;
  }

  /**
   * @brief Get the output registers the Executor is allowed to write. This has
   *        little to do with numberOfOutputRegisters(), except that each output
   *        register index returned here is smaller than
   *        numberOfOutputRegisters(). They may or may not be smaller than the
   *        numberOfInputRegisters(), i.e. they may already be allocated in the
   *        input blocks.
   *
   * @return The indices of the output registers.
   */
  std::shared_ptr<std::unordered_set<RegisterId> const> const getOutputRegisters() const {
    return _outRegs;
  }

  /**
   * @brief Total number of registers in input AqlItemBlocks. Not to be confused
   *        with the input registers the current Executor actually reads. See
   *        getInputRegisters() for that.
   */
  RegisterId numberOfInputRegisters() const { return _numInRegs; }

  /**
   * @brief Total number of registers in output AqlItemBlocks. Not to be
   * confused with the output registers the current Executor actually writes.
   * See getOutputRegisters() for that.
   */
  RegisterId numberOfOutputRegisters() const { return _numOutRegs; }

  std::shared_ptr<std::unordered_set<RegisterId> const> const& registersToKeep() const {
    return _registersToKeep;
  }

  std::shared_ptr<std::unordered_set<RegisterId> const> const& registersToClear() const {
    return _registersToClear;
  }

 protected:
  std::shared_ptr<std::unordered_set<RegisterId> const> _inRegs;

  std::shared_ptr<std::unordered_set<RegisterId> const> _outRegs;

  RegisterId _numInRegs;

  RegisterId _numOutRegs;

  std::shared_ptr<std::unordered_set<RegisterId> const> _registersToKeep;

  std::shared_ptr<std::unordered_set<RegisterId> const> const _registersToClear;
};

}  // namespace aql
}  // namespace arangodb

#endif
