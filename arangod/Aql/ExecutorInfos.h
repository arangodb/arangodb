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
  // inputRegister and outputRegister do not generally apply for all blocks.
  // TODO Thus they should not be part of this class, but only of its
  // descendants.
  ExecutorInfos(RegisterId inputRegister, RegisterId outputRegister,
                RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                std::unordered_set<RegisterId> registersToClear)
      : _inReg(inputRegister),
        _outReg(outputRegister),
        _filtered(0),
        _numInRegs(nrInputRegisters),
        _numOutRegs(nrOutputRegisters),
        _registersToKeep(),
        _registersToClear(std::move(registersToClear)) {
    TRI_ASSERT(_inReg < nrInputRegisters);
    TRI_ASSERT(_outReg < nrOutputRegisters);
    TRI_ASSERT(nrInputRegisters <= nrOutputRegisters);
    for (RegisterId i = 0; i < nrInputRegisters; i++) {
      if (_registersToClear.find(i) == _registersToClear.end()) {
        _registersToKeep.emplace(i);
      }
    }
    TRI_ASSERT(_registersToClear.size() + _registersToKeep.size() ==
               nrInputRegisters);
  }

  ExecutorInfos() = default;
  ExecutorInfos(ExecutorInfos &&) = default;
  ExecutorInfos(ExecutorInfos const&) = delete;
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

  size_t numberOfInputRegisters() const { return _numInRegs; }

  size_t numberOfOutputRegisters() const { return _numOutRegs; }

  std::unordered_set<RegisterId> const& registersToKeep() const {
    return _registersToKeep;
  }

  std::unordered_set<RegisterId> const& registersToClear() const {
    return _registersToClear;
  }

 private:

  RegisterId _inReg;

  RegisterId _outReg;

  size_t _filtered;

  size_t _numInRegs;

  size_t _numOutRegs;

  std::unordered_set<RegisterId> _registersToKeep;

  std::unordered_set<RegisterId> const _registersToClear;
};

} // aql
} // arangodb

#endif
