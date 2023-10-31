////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace arangodb {

namespace velocypack {
class Slice;
}  // namespace velocypack

struct OperationResult;

namespace pregel {
struct ExecutionNumber;
}
}  // namespace arangodb

namespace arangodb::pregel::statuswriter {

struct StatusWriterInterface {
  virtual ~StatusWriterInterface() = default;

  // CRUD interface definition
  [[nodiscard]] virtual auto createResult(velocypack::Slice data)
      -> OperationResult = 0;
  [[nodiscard]] virtual auto readResult() -> OperationResult = 0;
  [[nodiscard]] virtual auto readAllResults() -> OperationResult = 0;
  [[nodiscard]] virtual auto readAllNonExpiredResults() -> OperationResult = 0;
  [[nodiscard]] virtual auto updateResult(velocypack::Slice data)
      -> OperationResult = 0;
  [[nodiscard]] virtual auto deleteResult() -> OperationResult = 0;
  [[nodiscard]] virtual auto deleteAllResults() -> OperationResult = 0;
};

}  // namespace arangodb::pregel::statuswriter
