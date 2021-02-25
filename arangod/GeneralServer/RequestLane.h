////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GENERAL_SERVER_LANE_H
#define ARANGOD_GENERAL_SERVER_LANE_H 1

namespace arangodb {
enum class RequestLane {
  // For requests that do not block or wait for something.
  // This ignores blocks that can occur when delivering
  // a file from, for example, an NFS mount.
  CLIENT_FAST,

  // For requests that execute an AQL query or are tightly
  // related like simple queries, but not V8 actions,
  // that do AQL requests, user administrator that
  // internally uses AQL.
  CLIENT_AQL,

  // For requests that are executed within a V8 context,
  // but not for requests that might use a V8 context for
  // user defined functions.
  CLIENT_V8,

  // For requests that might block or wait for something,
  // which are not CLIENT_AQL or CLIENT_V8.
  CLIENT_SLOW,

  // Used for all requests sent by the web UI
  CLIENT_UI,

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

  // For requests from the DBserver to the Coordinator or
  // from the Coordinator to the DBserver. Using AQL
  // these have Medium priority.
  CLUSTER_AQL,

  // For future continuations initiated as part of an AQL request from the
  // DBserver to the Coordinator or from the Coordinator to the DBserver.
  // These have High priority.
  CLUSTER_AQL_CONTINUATION,

  // For requests from the from the Coordinator to the
  // DBserver using V8.
  CLUSTER_V8,

  // For requests from the DBserver to the Coordinator or
  // from the Coordinator to the DBserver for administration
  // or diagnostic purpose. Should not block.
  CLUSTER_ADMIN,

  // For requests used between leader and follower for
  // replication to compare the local states of data.
  SERVER_REPLICATION,

  // For requests used between leader and follower for
  // replication to go the final mile and get back to
  // in-sync mode (wal tailing)
  SERVER_REPLICATION_CATCHUP,

  // For synchronous replication requests on the follower
  SERVER_SYNCHRONOUS_REPLICATION,

  // For periodic or one-off V8-based tasks executed by the
  // Scheduler.
  TASK_V8,

  // Internal tasks with low priority
  INTERNAL_LOW,

  // Default continuation lane for requests (e.g. after returning from a network
  // call). Some requests, such as CLUSTER_AQL, will have a different
  // continuation lane for more fine-grained control.
  CONTINUATION,

  // Not yet used:
  // For requests which go from the agency back to coordinators or
  // DBservers to report about changes in the agency. They are fast
  // and should have high prio. Will never block.
  // AGENCY_CALLBACK`

  // Used by futures that have been delayed using Scheduler::delay.
  DELAYED_FUTURE,

  // undefined request lane, used only in the beginning
  UNDEFINED,
};

enum class RequestPriority {
  MAINTENANCE = 0,
  HIGH = 1,
  MED = 2,
  LOW = 3
};

inline RequestPriority PriorityRequestLane(RequestLane lane) {
  switch (lane) {
    case RequestLane::CLIENT_FAST:
      return RequestPriority::MAINTENANCE;
    case RequestLane::CLIENT_AQL:
      return RequestPriority::LOW;
    case RequestLane::CLIENT_V8:
      return RequestPriority::LOW;
    case RequestLane::CLIENT_SLOW:
      return RequestPriority::LOW;
    case RequestLane::AGENCY_INTERNAL:
      return RequestPriority::HIGH;
    case RequestLane::AGENCY_CLUSTER:
      return RequestPriority::LOW;
    case RequestLane::CLUSTER_INTERNAL:
      return RequestPriority::HIGH;
    case RequestLane::CLUSTER_AQL:
      return RequestPriority::MED;
    case RequestLane::CLUSTER_AQL_CONTINUATION:
      return RequestPriority::HIGH;
    case RequestLane::CLUSTER_V8:
      return RequestPriority::LOW;
    case RequestLane::CLUSTER_ADMIN:
      return RequestPriority::LOW;
    case RequestLane::SERVER_REPLICATION_CATCHUP:
      return RequestPriority::MED;
    case RequestLane::SERVER_REPLICATION:
      return RequestPriority::LOW;
    case RequestLane::TASK_V8:
      return RequestPriority::LOW;
    case RequestLane::INTERNAL_LOW:
      return RequestPriority::LOW;
    case RequestLane::CLIENT_UI:
      return RequestPriority::HIGH;
    case RequestLane::DELAYED_FUTURE:
      return RequestPriority::HIGH;
    case RequestLane::SERVER_SYNCHRONOUS_REPLICATION:
      return RequestPriority::HIGH;
    case RequestLane::CONTINUATION:
      return RequestPriority::MED;
    case RequestLane::UNDEFINED: {
      TRI_ASSERT(false);
    }
  }
  return RequestPriority::LOW;
}

}  // namespace arangodb

#endif
