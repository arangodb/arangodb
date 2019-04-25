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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "ExecutorInfos.h"

using namespace arangodb::aql;

ExecutorInfos::ExecutorInfos(
    // cppcheck-suppress passedByValue
    std::shared_ptr<std::unordered_set<RegisterId>> readableInputRegisters,
    // cppcheck-suppress passedByValue
    std::shared_ptr<std::unordered_set<RegisterId>> writeableOutputRegisters,
    RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    // cppcheck-suppress passedByValue
    std::unordered_set<RegisterId> registersToClear,
    // cppcheck-suppress passedByValue
    std::unordered_set<RegisterId> registersToKeep)
    : _inRegs(std::move(readableInputRegisters)),
      _outRegs(std::move(writeableOutputRegisters)),
      _numInRegs(nrInputRegisters),
      _numOutRegs(nrOutputRegisters),
      _registersToKeep(std::make_shared<std::unordered_set<RegisterId>>(
          std::move(registersToKeep))),
      _registersToClear(std::make_shared<std::unordered_set<RegisterId>>(
          std::move(registersToClear))) {
  // We allow these to be passed as nullptr for ease of use, but do NOT allow
  // the members to be null for simplicity and safety.
  if (_inRegs == nullptr) {
    _inRegs = std::make_shared<decltype(_inRegs)::element_type>();
  }
  if (_outRegs == nullptr) {
    _outRegs = std::make_shared<decltype(_outRegs)::element_type>();
  }
  // The second assert part is from ReturnExecutor special case, we shrink all
  // results into a single Register column.
  TRI_ASSERT((nrInputRegisters <= nrOutputRegisters) ||
             (nrOutputRegisters == 1 && _registersToKeep->empty() &&
              _registersToClear->empty()));

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  for (RegisterId const inReg : *_inRegs) {
    TRI_ASSERT(inReg < nrInputRegisters);
  }
  for (RegisterId const outReg : *_outRegs) {
    TRI_ASSERT(outReg < nrOutputRegisters);
  }
  for (RegisterId const regToClear : *_registersToClear) {
    // sic: It's possible that a current output register is immediately cleared!
    TRI_ASSERT(regToClear < nrOutputRegisters);
    TRI_ASSERT(_registersToKeep->find(regToClear) == _registersToKeep->end());
  }
  for (RegisterId const regToKeep : *_registersToKeep) {
    TRI_ASSERT(regToKeep < nrInputRegisters);
    TRI_ASSERT(_registersToClear->find(regToKeep) == _registersToClear->end());
  }
#endif
}
