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
    std::shared_ptr<std::unordered_set<RegisterId>> inputRegisters,
    std::shared_ptr<std::unordered_set<RegisterId>> outputRegisters,
    RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    std::unordered_set<RegisterId> registersToClear)
    : _inRegs(std::move(inputRegisters)),
      _outRegs(std::move(outputRegisters)),
      _numInRegs(nrInputRegisters),
      _numOutRegs(nrOutputRegisters),
      _registersToKeep(nullptr),
      _registersToClear(std::move(registersToClear)) {
  // We allow these to be passed as nullptr for ease of use, but do NOT allow
  // the members to be null for simplicity and safety.
  if (_inRegs == nullptr) {
    _inRegs = std::make_shared<decltype(_inRegs)::element_type>();
  }
  if (_outRegs == nullptr) {
    _outRegs = std::make_shared<decltype(_outRegs)::element_type>();
  }
  for (RegisterId const inReg : *_inRegs) {
    TRI_ASSERT(inReg < nrInputRegisters);
  }
  for (RegisterId const outReg : *_outRegs) {
    TRI_ASSERT(outReg < nrOutputRegisters);
  }
  TRI_ASSERT(nrInputRegisters <= nrOutputRegisters);
  {
    auto registersToKeep = std::make_shared<std::unordered_set<RegisterId>>();
    for (RegisterId i = 0; i < nrInputRegisters; i++) {
      if (_registersToClear.find(i) == _registersToClear.end()) {
        registersToKeep->emplace(i);
      }
    }
    _registersToKeep = std::move(registersToKeep);
  }
  TRI_ASSERT(_registersToClear.size() + _registersToKeep->size() ==
             nrInputRegisters);
}
