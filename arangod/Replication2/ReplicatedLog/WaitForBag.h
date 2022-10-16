////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <exception>
#include <vector>
#include <yaclib/async/future.hpp>
#include <yaclib/async/promise.hpp>

namespace arangodb {

struct WaitForBag {
  WaitForBag() = default;

  auto addWaitFor() -> yaclib::Future<>;

  void resolveAll();

  void resolveAll(std::exception_ptr const&);

  [[nodiscard]] auto empty() const noexcept -> bool;

 private:
  std::vector<yaclib::Promise<>> _waitForBag;
};

}  // namespace arangodb
