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

#include "ClusterCommMock.h"

// -----------------------------------------------------------------------------
// --SECTION--                                 GeneralClientConnectionAgencyMock
// -----------------------------------------------------------------------------

ClusterCommMock::Request::Request(
    arangodb::CoordTransactionID trxId,
    std::string const& destination,
    arangodb::rest::RequestType type,
    std::string const& path,
    std::shared_ptr<std::string const> body,
    std::unordered_map<std::string, std::string> const& headerFields,
    std::shared_ptr<arangodb::ClusterCommCallback> callback,
    bool singleRequest
): _body(body),
   _callback(callback),
   _destination(destination),
   _headerFields(headerFields),
   _path(path),
   _singleRequest(singleRequest),
   _trxId(trxId),
   _type(type) {
}

ClusterCommMock::ClusterCommMock(): arangodb::ClusterComm(false) { // false same as in ClusterCommTest.cpp
}

arangodb::OperationID ClusterCommMock::asyncRequest(
   arangodb::CoordTransactionID const coordTransactionID,
   std::string const& destination,
   arangodb::rest::RequestType reqtype,
   std::string const& path,
   std::shared_ptr<std::string const> body,
   std::unordered_map<std::string, std::string> const& headerFields,
   std::shared_ptr<arangodb::ClusterCommCallback> callback,
   arangodb::ClusterCommTimeout timeout,
   bool singleRequest,
   arangodb::ClusterCommTimeout initTimeout
) {
  auto entry = _requests.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(++_operationId), // non-zero value
    std::forward_as_tuple(coordTransactionID, destination, reqtype, path, body, headerFields, callback, singleRequest)
  );

  return entry.first->first;
}

void ClusterCommMock::drop(
   arangodb::CoordTransactionID const coordTransactionID, // 0 == any trxId
   arangodb::OperationID const operationID, // 0 == any opId
   arangodb::ShardID const& shardID // "" = any shardId
) {
  auto itr = _responses.find(operationID);

  TRI_ASSERT(itr != _responses.end());
  TRI_ASSERT(!itr->second.empty());
  itr->second.pop_front();

  if (itr->second.empty()) {
    _responses.erase(itr);
  }
}

/*static*/ std::shared_ptr<ClusterCommMock> ClusterCommMock::setInstance(
  ClusterCommMock& instance
) {
  _theInstance = std::shared_ptr<arangodb::ClusterComm>(
    &instance, [](arangodb::ClusterComm*)->void {}
  );

  return std::shared_ptr<ClusterCommMock>(
    &instance,
    [](ClusterCommMock*)->void {
      arangodb::ClusterComm::_theInstanceInit.store(0);
      _theInstance.reset();
    }
  );
}

arangodb::ClusterCommResult const ClusterCommMock::wait(
   arangodb::CoordTransactionID const coordTransactionID, // 0 == any trxId
   arangodb::OperationID const operationID, // 0 == any opId
   arangodb::ShardID const& shardID, // "" = any shardId
   arangodb::ClusterCommTimeout timeout
) {
  auto itr = _responses.find(operationID);

  if (itr == _responses.end() || itr->second.empty()) {
    return arangodb::ClusterCommResult();
  }

  auto result = std::move(itr->second.front());

  itr->second.pop_front();

  if (itr->second.empty()) {
    _responses.erase(itr);
  }

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------