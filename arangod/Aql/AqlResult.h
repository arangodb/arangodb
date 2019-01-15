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

#ifndef ARANGOD_AQL_AQL_RESULT_H
#define ARANGOD_AQL_AQL_RESULT_H 1

#include "Basics/Common.h"
#include "Basics/Result.h"

namespace arangodb {

namespace aql {

class ExecutionEngine;

class ExecutionEngineResult : public Result {
 public:
  ExecutionEngineResult();
  explicit ExecutionEngineResult(int errorNumber);
  ExecutionEngineResult(int errorNumber, std::string const& errorMessage);
  ExecutionEngineResult(int errorNumber, std::string&& errorMessage);

  // This is not explicit on purpose
  // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
  ExecutionEngineResult(Result const& result);
  explicit ExecutionEngineResult(ExecutionEngine*);

  ~ExecutionEngineResult();

  ExecutionEngine* engine() const;

 private:
  ExecutionEngine* _engine;
};

}  // namespace aql
}  // namespace arangodb

#endif
