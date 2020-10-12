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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////
#include "RegisterInfos.h"
#include "Logger/LogMacros.h"

#include "Basics/debugging.h"

using namespace arangodb::aql;

auto setStackToFlatSetStack(RegIdSetStack const& setStack) -> RegIdFlatSetStack {
  auto flatSetStack = RegIdFlatSetStack{};
  flatSetStack.reserve(setStack.size());

  std::transform(setStack.begin(), setStack.end(),
                 std::back_inserter(flatSetStack), [](auto const& stackEntry) {
                   return RegIdFlatSet{stackEntry.begin(), stackEntry.end()};
                 });
  return flatSetStack;
}

RegisterInfos::RegisterInfos(
    // cppcheck-suppress passedByValue
    RegIdSet readableInputRegisters,
    // cppcheck-suppress passedByValue
    RegIdSet writeableOutputRegisters, RegisterCount nrInputRegisters,
    RegisterCount nrOutputRegisters,
    // cppcheck-suppress passedByValue
    RegIdSet const& registersToClear,
    // cppcheck-suppress passedByValue
    RegIdSetStack const& registersToKeep)
    : RegisterInfos(std::move(readableInputRegisters),
                    std::move(writeableOutputRegisters), nrInputRegisters, nrOutputRegisters,
                    RegIdFlatSet{registersToClear.begin(), registersToClear.end()},
                    setStackToFlatSetStack(registersToKeep)) {}

RegisterInfos::RegisterInfos(RegIdSet readableInputRegisters, RegIdSet writeableOutputRegisters,
                             RegisterCount nrInputRegisters, RegisterCount nrOutputRegisters,
                             RegIdFlatSet registersToClear, RegIdFlatSetStack registersToKeep)
    : _inRegs(std::move(readableInputRegisters)),
      _outRegs(std::move(writeableOutputRegisters)),
      _numInRegs(nrInputRegisters),
      _numOutRegs(nrOutputRegisters),
      _registersToKeep(std::move(registersToKeep)),
      _registersToClear(std::move(registersToClear)) {

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
    TRI_ASSERT(_registersToKeep.back().find(regToClear) ==
               _registersToKeep.back().end());
  }
  TRI_ASSERT(!_registersToKeep.empty());
  for (RegisterId const regToKeep : _registersToKeep.back()) {
    TRI_ASSERT(regToKeep < nrInputRegisters);
    TRI_ASSERT(regToKeep < nrOutputRegisters);
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

RegIdFlatSetStack const& RegisterInfos::registersToKeep() const {
  return _registersToKeep;
}

RegIdFlatSet const& RegisterInfos::registersToClear() const {
  return _registersToClear;
}
