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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_CLUSTER_REST_COMPACT_HANDLER_H
#define ARANGOD_CLUSTER_CLUSTER_REST_COMPACT_HANDLER_H 1

#include "RestHandler/RestCollectionHandler.h"

namespace arangodb {

class ClusterRestCompactHandler : public arangodb::RestBaseHandler {
 public:
  ClusterRestCompactHandler(GeneralRequest*, GeneralResponse*);

 public:
  RestStatus execute() override;
  char const* name() const override { return "ClusterRestCompactHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_SLOW; }
};

}  // namespace arangodb

#endif
