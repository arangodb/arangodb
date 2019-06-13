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
   std::shared_ptr<arangodb::ClusterCommCallback> const& callback,
   arangodb::ClusterCommTimeout timeout,
   bool singleRequest,
   arangodb::ClusterCommTimeout initTimeout
) {
  // execute before insertion to avoid consuming an '_operationId'
  if (arangodb::rest::RequestType::PUT == reqtype
      && std::string::npos != path.find("/_api/aql/shutdown/")) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED); // terminate query 'shutdown' infinite loops with exception
  }

  if (_responses.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "request not expected since result not provided");
  }

  auto operationID = _responses.front().operationID; // set expected responce ID

  _requests.emplace_back(coordTransactionID, destination, reqtype, path, body, headerFields, callback, singleRequest);

  if (!callback) {
    return operationID;
  }

  auto result = wait(coordTransactionID, 0, "", timeout); // OperationID == 0 same as ClusterComm::performRequests(...)

  (*callback)(&result);

  return operationID;
}

void ClusterCommMock::drop(
   arangodb::CoordTransactionID const coordTransactionID, // 0 == any trxId
   arangodb::OperationID const operationID, // 0 == any opId
   arangodb::ShardID const& shardID // "" = any shardId
) {
  _responses.pop_front();
}

/*static*/ std::shared_ptr<ClusterCommMock> ClusterCommMock::setInstance(
  ClusterCommMock& instance
) {
  _theInstance = std::shared_ptr<arangodb::ClusterComm>(
    &instance, [](arangodb::ClusterComm*)->void {}
  );
  arangodb::ClusterComm::_theInstanceInit.store(2);

  return std::shared_ptr<ClusterCommMock>(
    &instance,
    [](ClusterCommMock*)->void {
      arangodb::ClusterComm::_theInstanceInit.store(0);
      _theInstance.reset();
    }
  );
}

std::unique_ptr<arangodb::ClusterCommResult> ClusterCommMock::syncRequest(
  arangodb::CoordTransactionID const coordTransactionID,
  std::string const& destination,
  arangodb::rest::RequestType reqtype,
  std::string const& path,
  std::string const& body,
  std::unordered_map<std::string, std::string> const& headerFields,
  arangodb::ClusterCommTimeout timeout
) {
  asyncRequest(
    coordTransactionID,
    destination,
    reqtype,
    path,
    std::shared_ptr<std::string const>(&body, [](std::string const*)->void {}),
    headerFields,
    nullptr,
    timeout,
    true,
    timeout
  );

  auto result = std::make_unique<arangodb::ClusterCommResult>(
    wait(coordTransactionID, 0, "", timeout) // OperationID == 0 same as ClusterComm::performRequests(...)
  );

  return result;
}

arangodb::ClusterCommResult const ClusterCommMock::wait(
   arangodb::CoordTransactionID const coordTransactionID, // 0 == any trxId
   arangodb::OperationID const operationID, // 0 == any opId
   arangodb::ShardID const& shardID, // "" = any shardId
   arangodb::ClusterCommTimeout timeout
) {
  if (_responses.empty()) {
    return arangodb::ClusterCommResult();
  }

  auto result = std::move(_responses.front());

  _responses.pop_front();

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
