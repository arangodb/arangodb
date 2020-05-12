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
#include "RegisterInfos.h"
#include "Logger/LogMacros.h"

#include "Basics/debugging.h"

using namespace arangodb::aql;

RegisterInfos::RegisterInfos(
    // cppcheck-suppress passedByValue
    RegIdSet readableInputRegisters,
    // cppcheck-suppress passedByValue
    RegIdSet writeableOutputRegisters,
    RegisterCount nrInputRegisters, RegisterCount nrOutputRegisters,
    // cppcheck-suppress passedByValue
    RegIdSet registersToClear,
    // cppcheck-suppress passedByValue
    RegIdSetStack registersToKeep)
    : _inRegs(std::move(readableInputRegisters)),
      _outRegs(std::move(writeableOutputRegisters)),
      _numInRegs(nrInputRegisters),
      _numOutRegs(nrOutputRegisters),
      _registersToKeep(std::move(registersToKeep)),
      _registersToClear(std::move(registersToClear)) {
  // The second assert part is from ReturnExecutor special case, we shrink all
  // results into a single Register column.
  TRI_ASSERT((nrInputRegisters <= nrOutputRegisters) ||
             (nrOutputRegisters == 1 && _registersToKeep.back().empty() &&
              _registersToClear.empty()));

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  for (RegisterId const inReg : _inRegs) {
    TRI_ASSERT(inReg < nrInputRegisters);
  }
  for (RegisterId const outReg : _outRegs) {
    TRI_ASSERT(outReg < nrOutputRegisters);
  }
  for (RegisterId const regToClear : _registersToClear) {
    // sic: It's possible that a current output register is immediately cleared!
    TRI_ASSERT(regToClear < nrOutputRegisters);
    TRI_ASSERT(_registersToKeep.back().find(regToClear) == _registersToKeep.back().end());
  }
  TRI_ASSERT(!_registersToKeep.empty());
  for (RegisterId const regToKeep : _registersToKeep.back()) {
    TRI_ASSERT(regToKeep < nrInputRegisters);
    TRI_ASSERT(_registersToClear.find(regToKeep) == _registersToClear.end());
  }
#endif
}

RegIdSet const& RegisterInfos::getInputRegisters() const {
  return _inRegs;
}

RegIdSet const& RegisterInfos::getOutputRegisters() const {
  return _outRegs;
}

RegisterCount RegisterInfos::numberOfInputRegisters() const { return _numInRegs; }

RegisterCount RegisterInfos::numberOfOutputRegisters() const {
  return _numOutRegs;
}

RegIdSetStack const& RegisterInfos::registersToKeep() const {
  return _registersToKeep;
}

RegIdSet const& RegisterInfos::registersToClear() const {
  return _registersToClear;
}
