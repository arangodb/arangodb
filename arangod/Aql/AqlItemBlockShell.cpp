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

#include "AqlItemBlockShell.h"

using namespace arangodb::aql;

AqlItemBlockShell::AqlItemBlockShell(
    AqlItemBlockManager& manager, std::unique_ptr<AqlItemBlock> block_,
    std::shared_ptr<const std::unordered_set<RegisterId>> inputRegisters_,
    std::shared_ptr<const std::unordered_set<RegisterId>> outputRegisters_,
    std::shared_ptr<const std::unordered_set<RegisterId>> registersToKeep_,
    AqlItemBlockShell::AqlItemBlockId aqlItemBlockId_)
    : _block(block_.release(), AqlItemBlockDeleter{manager}),
      _inputRegisters(std::move(inputRegisters_)),
      _outputRegisters(std::move(outputRegisters_)),
      _registersToKeep(std::move(registersToKeep_)),
      _aqlItemBlockId(aqlItemBlockId_) {
  if (_inputRegisters == nullptr) {
    _inputRegisters =
        std::make_shared<decltype(_inputRegisters)::element_type>();
  }
  if (_outputRegisters == nullptr) {
    _outputRegisters =
        std::make_shared<decltype(_outputRegisters)::element_type>();
  }
  // An AqlItemBlockShell instance is assumed to be responsible for *exactly*
  // one AqlItemBlock. _block may never be null!
  TRI_ASSERT(_block != nullptr);
}
