////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <memory>

namespace arangodb::containers {

struct Metrics {
  virtual ~Metrics() = default;
  virtual auto increment_total_nodes() -> void {}
  virtual auto increment_registered_nodes() -> void {}
  virtual auto decrement_registered_nodes() -> void {}
  virtual auto increment_ready_for_deletion_nodes() -> void {}
  virtual auto decrement_ready_for_deletion_nodes() -> void {}
  virtual auto increment_total_lists() -> void {}
  virtual auto increment_existing_lists() -> void {}
  virtual auto decrement_existing_lists() -> void {}
};

template<typename T>
concept UpdatesMetrics = requires(T t, std::shared_ptr<Metrics> m) {
  t.set_metrics(m);
};

}  // namespace arangodb::containers
