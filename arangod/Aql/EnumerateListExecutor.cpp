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

#include "EnumerateListExecutor.h"
#include <lib/Logger/LogMacros.h>

#include "Basics/Common.h"

#include "Aql/InputAqlItemRow.h"
#include "Aql/AqlValue.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/Exceptions.h"

using namespace arangodb;
using namespace arangodb::aql;

EnumerateListExecutorInfos::EnumerateListExecutorInfos(
    RegisterId inputRegister, RegisterId outputRegister,
    RegisterId nrOutputRegisters, RegisterId nrInputRegisters,
    std::unordered_set<RegisterId> const registersToClear,
    transaction::Methods* trx)
    : ExecutorInfos(inputRegister, outputRegister, nrOutputRegisters,
                    nrInputRegisters, registersToClear),
      _trx(trx) {
  TRI_ASSERT(trx != nullptr);
}

EnumerateListExecutorInfos::~EnumerateListExecutorInfos() {
}

transaction::Methods* EnumerateListExecutorInfos::trx() const {
  return _trx;
}

EnumerateListExecutor::EnumerateListExecutor(Fetcher& fetcher,
                                             EnumerateListExecutorInfos& infos)
    : _fetcher(fetcher), _infos(infos){};
EnumerateListExecutor::~EnumerateListExecutor() = default;

ExecutionState EnumerateListExecutor::produceRow(OutputAqlItemRow &output) {
  ExecutionState state;
  InputAqlItemRow const* input = nullptr;

  while (true) {
    std::tie(state, input) = _fetcher.fetchRow();

    if (state == ExecutionState::WAITING) {
      return state;
    }

    if (input == nullptr) {
      TRI_ASSERT(state == ExecutionState::DONE);
      return state;
    }

    AqlValue const& value = input->getValue(_infos.getInput());

    if (!value.isArray()) {
      throwArrayExpectedException(value);
    }

    size_t sizeInValue;
    if (value.isDocvec()) {
      sizeInValue = value.docvecSize();
    } else {
      sizeInValue = value.length();
    }

    if (sizeInValue == 0) {
      return state;
    } else {
      for (size_t j = 0; j < sizeInValue; j++) {
        bool mustDestroy = false;  // TODO:  needed?
        AqlValue innerValue = getAqlValue(innerValue, j, mustDestroy);
        AqlValueGuard guard(innerValue, mustDestroy);

        output.copyRow(*input);
        output.setValue(_infos.getInput(), *input, innerValue);
      }
    }

    return state;

    /*
    if (state == ExecutionState::DONE) {
      return state;
    }
    TRI_ASSERT(state == ExecutionState::HASMORE);
    */
  }
}

/// @brief create an AqlValue from the inVariable using the current _index
AqlValue EnumerateListExecutor::getAqlValue(AqlValue const& inVarReg,
                                            size_t const& pos,
                                            bool& mustDestroy) {
  TRI_IF_FAILURE("EnumerateListBlock::getAqlValue") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return inVarReg.at(_infos.trx(), pos, mustDestroy, true);
}

void EnumerateListExecutor::throwArrayExpectedException(AqlValue const& value) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_QUERY_ARRAY_EXPECTED,
      std::string("collection or ") +
          TRI_errno_string(TRI_ERROR_QUERY_ARRAY_EXPECTED) +
          std::string(
              " as operand to FOR loop; you provided a value of type '") +
          value.getTypeString() + std::string("'"));
}
