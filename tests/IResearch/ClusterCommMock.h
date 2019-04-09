////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IRESEARCH__IRESEARCH_CLUSTER_COMM_MOCK_H
#define ARANGODB_IRESEARCH__IRESEARCH_CLUSTER_COMM_MOCK_H 1

#include "Cluster/ClusterComm.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief a ClusterComm implementation for tracking requests and returning
///        preset responses
////////////////////////////////////////////////////////////////////////////////
class ClusterCommMock: public arangodb::ClusterComm {
 public:
  struct Request {
    std::shared_ptr<std::string const> _body;
    std::shared_ptr<arangodb::ClusterCommCallback> _callback;
    std::string _destination;
    std::unordered_map<std::string, std::string> _headerFields;
    std::string const _path;
    bool _singleRequest;
    arangodb::CoordTransactionID _trxId;
    arangodb::rest::RequestType _type;

    Request(
        arangodb::CoordTransactionID const trxId,
        std::string const& destination,
        arangodb::rest::RequestType type,
        std::string const& path,
        std::shared_ptr<std::string const> body,
        std::unordered_map<std::string, std::string> const& headerFields,
        std::shared_ptr<arangodb::ClusterCommCallback> callback,
        bool singleRequest
    );
  };

  std::vector<Request> _requests;
  std::deque<arangodb::ClusterCommResult> _responses;

  ClusterCommMock();

  arangodb::OperationID asyncRequest(
     arangodb::CoordTransactionID const coordTransactionID,
     std::string const& destination,
     arangodb::rest::RequestType reqtype,
     std::string const& path,
     std::shared_ptr<std::string const> body,
     std::unordered_map<std::string, std::string> const& headerFields,
     std::shared_ptr<arangodb::ClusterCommCallback> const& callback,
     arangodb::ClusterCommTimeout timeout,
     bool singleRequest,
     arangodb::ClusterCommTimeout initTimeout
  ) override;

  void drop(
     arangodb::CoordTransactionID const coordTransactionID, // 0 == any trxId
     arangodb::OperationID const operationID, // 0 == any opId
     arangodb::ShardID const& shardID // "" = any shardId
  ) override;

  static std::shared_ptr<ClusterCommMock> setInstance(
    ClusterCommMock& instance
  );

  std::unique_ptr<arangodb::ClusterCommResult> syncRequest(
    arangodb::CoordTransactionID const coordTransactionID,
    std::string const& destination,
    arangodb::rest::RequestType reqtype,
    std::string const& path,
    std::string const& body,
    std::unordered_map<std::string, std::string> const& headerFields,
    arangodb::ClusterCommTimeout timeout
  ) override;

  arangodb::ClusterCommResult const wait(
     arangodb::CoordTransactionID const coordTransactionID, // 0 == any trxId
     arangodb::OperationID const operationID, // 0 == any opId
     arangodb::ShardID const& shardID, // "" = any shardId
     arangodb::ClusterCommTimeout timeout
  ) override;
};

#endif
