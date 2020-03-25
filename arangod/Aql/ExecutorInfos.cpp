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
#include "Basics/debugging.h"

#include <algorithm>

using namespace arangodb::aql;

ExecutorInfos::ExecutorInfos(
    // cppcheck-suppress passedByValue
    std::shared_ptr<std::unordered_set<RegisterId>> writeableOutputRegisters,
    RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    // cppcheck-suppress passedByValue
    std::vector<RegisterId> registersToClear,
    // cppcheck-suppress passedByValue
    std::vector<RegisterId> registersToKeep)
    : _outRegs(std::move(writeableOutputRegisters)),
      _numInRegs(nrInputRegisters),
      _numOutRegs(nrOutputRegisters),
      _registersToKeep(std::move(registersToKeep)),
      _registersToClear(std::move(registersToClear)) {

  // we assume that the input of these vectors is sorted and unique.
  // we are using it in the same way as we would be using an unordered_set, but
  // without paying the memory allocation price for it and also being able
  // to iterate over the registers in order (so we can benefit from cache line
  // effects when peeking at the registers' values in the AqlItemBlock later).
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(std::is_sorted(_registersToKeep.begin(), _registersToKeep.end()));
  TRI_ASSERT(std::is_sorted(_registersToClear.begin(), _registersToClear.end()));
  TRI_ASSERT(std::unique(_registersToKeep.begin(), _registersToKeep.end()) == _registersToKeep.end());
  TRI_ASSERT(std::unique(_registersToClear.begin(), _registersToClear.end()) == _registersToClear.end());
#endif

  // We allow these to be passed as nullptr for ease of use, but do NOT allow
  // the members to be null for simplicity and safety.
  if (_outRegs == nullptr) {
    _outRegs = std::make_shared<decltype(_outRegs)::element_type>();
  }
  // The second assert part is from ReturnExecutor special case, we shrink all
  // results into a single Register column.
  TRI_ASSERT((nrInputRegisters <= nrOutputRegisters) ||
             (nrOutputRegisters == 1 && _registersToKeep.empty() &&
              _registersToClear.empty()));

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  for (RegisterId const outReg : *_outRegs) {
    TRI_ASSERT(outReg < nrOutputRegisters);
  }
  for (auto const& regToClear : _registersToClear) {
    // sic: It's possible that a current output register is immediately cleared!
    TRI_ASSERT(regToClear < nrOutputRegisters);
    TRI_ASSERT(std::find(_registersToKeep.begin(), _registersToKeep.end(), regToClear) == _registersToKeep.end());
  }
  for (auto const& regToKeep : _registersToKeep) {
    TRI_ASSERT(regToKeep < nrInputRegisters);
    TRI_ASSERT(std::find(_registersToClear.begin(), _registersToClear.end(), regToKeep) == _registersToClear.end());
  }
#endif
}

std::shared_ptr<std::unordered_set<RegisterId> const> ExecutorInfos::getOutputRegisters() const {
  return _outRegs;
}

RegisterId ExecutorInfos::numberOfInputRegisters() const { return _numInRegs; }

RegisterId ExecutorInfos::numberOfOutputRegisters() const {
  return _numOutRegs;
}

std::vector<RegisterId> const& ExecutorInfos::registersToKeep() const {
  return _registersToKeep;
}

std::vector<RegisterId> const& ExecutorInfos::registersToClear() const {
  return _registersToClear;
}

std::vector<RegisterId> ExecutorInfos::makeRegisterVector(RegisterId size) {
  std::vector<RegisterId> result;
  result.reserve(size);
  for (RegisterId i = 0; i < size; i++) {
    result.push_back(i);
  }
  return result;
}

std::shared_ptr<std::unordered_set<RegisterId>> arangodb::aql::make_shared_unordered_set(
    const std::initializer_list<RegisterId>& list) {
  return std::make_shared<std::unordered_set<RegisterId>>(list);
}

std::shared_ptr<std::unordered_set<RegisterId>> arangodb::aql::make_shared_unordered_set(RegisterId size) {
  auto set = make_shared_unordered_set();
  for (RegisterId i = 0; i < size; i++) {
    set->insert(i);
  }
  return set;
}
