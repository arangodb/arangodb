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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GENERAL_SERVER_LANE_H
#define ARANGOD_GENERAL_SERVER_LANE_H 1

#include "Basics/Common.h"

namespace arangodb {
class GeneralRequest;

enum class RequestLane {
  // For requests that do not block or wait for something.
  // This ignores blocks that can occur when delivering
  // a file from, for example, an NFS mount.
  CLIENT_FAST,

  // For requests that execute an AQL query or tightly
  // related like simple queries, but not V8 actions,
  // that do AQL requests, user administrator that
  // internally uses AQL.
  CLIENT_AQL,

  // For requests that are executed within an V8 context,
  // but not for requests that might use a V8 context for
  // user defined functions.
  CLIENT_V8,

  // For requests that might block or wait for something,
  // which are not CLIENT_AQL or CLIENT_V8.
  CLIENT_SLOW,

  // For requests between agents. These are basically the
  // requests used to implement RAFT.
  AGENCY_INTERNAL,

  // For requests from the DBserver or Coordinator accessing
  // the agency.
  AGENCY_CLUSTER,

  // For requests from the DBserver to the Coordinator or
  // from the Coordinator to the DBserver. But not using
  // V8 or having high priority.
  CLUSTER_INTERNAL,

  // For requests from the from the Coordinator to the
  // DBserver using V8.
  CLUSTER_V8,

  // For requests from the DBserver to the Coordinator or
  // from the Coordinator to the DBserver for administration
  // or diagnostic purpose. Should not block.
  CLUSTER_ADMIN,

  // For requests used between leader and follower for
  // replication.
  SERVER_REPLICATION,

  // For periodic or one-off V8-based tasks executed by the
  // Scheduler.
  TASK_V8

  // Not yet used:
  // For requests which go from the agency back to coordinators or
  // DBservers to report about changes in the agency. They are fast
  // and should have high prio. Will never block.
  // AGENCY_CALLBACK`
};

enum class RequestPriority { HIGH, MED, LOW };

}

#endif
