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

#include "Metrics/IRegistry.h"
#include "Metrics/Builder.h"
#include "Metrics/Metric.h"

namespace arangodb::metrics {


// A registry that populates metrics but does not register them with any
// actual metrics endpoint. Returned shared_ptr is the only owner.  
struct FakeRegistry : public IRegistry {
 protected:
  std::shared_ptr<metrics::Metric> doAdd(metrics::Builder& builder) override {
    return builder.build();
  }
};

}  // namespace arangodb::metrics