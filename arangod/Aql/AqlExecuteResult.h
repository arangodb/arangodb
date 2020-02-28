////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_AQLEXECUTERESULT_H
#define ARANGOD_AQL_AQLEXECUTERESULT_H

#include "Aql/ExecutionState.h"
#include "Aql/SharedAqlItemBlockPtr.h"

#include <utility>

namespace arangodb::velocypack {
class Builder;
struct Options;
}  // namespace arangodb::velocypack

namespace arangodb::aql {

class AqlExecuteResult {
 public:
  AqlExecuteResult(ExecutionState state, std::size_t skipped, SharedAqlItemBlockPtr&& block)
      : _state(state), _skipped(skipped), _block(std::move(block)) {}

  void toVelocyPack(velocypack::Builder&, velocypack::Options const*);

  [[nodiscard]] auto state() const noexcept -> ExecutionState;
  [[nodiscard]] auto skipped() const noexcept -> std::size_t;
  [[nodiscard]] auto block() const noexcept -> SharedAqlItemBlockPtr const&;

 private:
  ExecutionState _state = ExecutionState::HASMORE;
  std::size_t _skipped = 0;
  SharedAqlItemBlockPtr _block = nullptr;
};

}  // namespace arangodb::aql

#endif  // ARANGOD_AQL_AQLEXECUTERESULT_H
