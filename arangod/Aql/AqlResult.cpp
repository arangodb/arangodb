////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionEngine.h"
#include "AqlResult.h"

using namespace arangodb;
using namespace arangodb::aql;

ExecutionEngineResult::ExecutionEngineResult() : _result(), _engine(nullptr) {}
ExecutionEngineResult::ExecutionEngineResult(int num)
    : _result(num), _engine(nullptr) {}
ExecutionEngineResult::ExecutionEngineResult(int num, std::string const& msg)
    : _result(num, msg), _engine(nullptr) {}
ExecutionEngineResult::ExecutionEngineResult(int num, std::string&& msg)
    : _result(num, msg), _engine(nullptr) {}
ExecutionEngineResult::ExecutionEngineResult(ExecutionEngine* engine)
    : _result(), _engine(engine) {}

// No responsibilty for the pointer
ExecutionEngineResult::~ExecutionEngineResult() {}

ExecutionEngine* ExecutionEngineResult::engine() const { return _engine; }

ExecutionEngineResult::ExecutionEngineResult(const Result& result)
    : _result(result), _engine(nullptr) {}

bool ExecutionEngineResult::ok() const { return _result.ok(); }

bool ExecutionEngineResult::fail() const { return _result.fail(); }

int ExecutionEngineResult::errorNumber() const { return _result.errorNumber(); }

std::string ExecutionEngineResult::errorMessage() const {
  return _result.errorMessage();
}
