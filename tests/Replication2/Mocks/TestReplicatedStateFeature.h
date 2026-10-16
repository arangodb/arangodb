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
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "Mocks/FakeRegistry.h"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"
#include "Replication2/ReplicatedState/ReplicatedStateMetrics.h"

namespace arangodb::replication2::tests {

// Suitable for tests; populate with mock metrics objects with FakeRegistry
// so they don't need a MetricsFeature.
struct TestReplicatedStateFeature : replicated_state::ReplicatedStateFeature {
 protected:
  auto createMetricsObject(std::string_view impl)
      -> std::shared_ptr<replicated_state::ReplicatedStateMetrics> override {
    return std::make_shared<replicated_state::ReplicatedStateMetrics>(
        _fakeRegistry, impl);
  }

 private:
  metrics::FakeRegistry _fakeRegistry;
};

}  // namespace arangodb::replication2::tests