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

#include "Basics/Common.h"
#include "Aql/types.h"

namespace arangodb {
namespace aql {

/**
 * @brief Class to be handed into Executors during construction
 *        This class should be independend from AQL internal
 *        knowledge for easy unit-testability.
 */
class ExecutorInfos {
 public:
  ExecutorInfos(RegisterId inputRegister, RegisterId outputRegister,
                RegisterId nrOutputRegisters,
                RegisterId nrInputRegisters,
                std::unordered_set<RegisterId> const registersToClear)
      : _inReg(inputRegister),
        _outReg(outputRegister),
        _filtered(0),
        _numRegs(nrOutputRegisters),
        _registersToKeep(),
        _registersToClear(registersToClear) {
    for (RegisterId i = 0; i < nrInputRegisters; i++) {
      if (_registersToClear.find(i) == _registersToClear.end()) {
        _registersToKeep.emplace(i);
      }
    }
  }

  ~ExecutorInfos() = default;

  /**
   * @brief Get the input register the Executor is allowed to read.
   *
   * @return The index of the input register.
   */
  RegisterId getInput() const { return _inReg; }

  /**
   * @brief Get the output register the Executor is allowed to write to.
   *
   * @return The index of the output register.
   */
  RegisterId getOutput() const { return _outReg; }

  /**
   * @brief Get the number of documents that have been filtered
   *        by the Executor
   *
   * @return Number of filtered documents
   */
  size_t getFiltered() const { return _filtered; };

  /**
   * @brief Increase the counter of filtered documents by one
   */
  void countFiltered() { _filtered++; } 

  size_t numberOfRegisters() { return _numRegs; }

  std::unordered_set<RegisterId> const& registersToKeep() { return _registersToKeep; }

  std::unordered_set<RegisterId> const& registersToClear() { return _registersToClear; }

 private:

  RegisterId _inReg;

  RegisterId _outReg;

  size_t _filtered;

  size_t _numRegs;

  std::unordered_set<RegisterId> _registersToKeep;

  std::unordered_set<RegisterId> const _registersToClear;
};

} // aql
} // arangodb

#endif
