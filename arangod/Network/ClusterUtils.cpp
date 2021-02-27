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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "ClusterUtils.h"

#include "Network/ConnectionPool.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"

#include "Logger/LogMacros.h"

#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace network {

/// @brief Create Cluster Communication result for insert
OperationResult clusterResultInsert(arangodb::fuerte::StatusCode code,
                                    std::shared_ptr<VPackBuffer<uint8_t>> body,
                                    OperationOptions options,
                                    std::unordered_map<ErrorCode, size_t> const& errorCounter) {
  switch (code) {
    case fuerte::StatusAccepted:
      return OperationResult(Result(), std::move(body), std::move(options), errorCounter);
    case fuerte::StatusCreated: {
      options.waitForSync = true;  // wait for sync is abused herea
      // operationResult should get a return code.
      return OperationResult(Result(), std::move(body), std::move(options), errorCounter);
    }
    case fuerte::StatusPreconditionFailed:
      return network::opResultFromBody(std::move(body), TRI_ERROR_ARANGO_CONFLICT,
                                       std::move(options));
    case fuerte::StatusBadRequest:
      return network::opResultFromBody(std::move(body), TRI_ERROR_INTERNAL,
                                       std::move(options));
    case fuerte::StatusNotFound:
      return network::opResultFromBody(std::move(body), TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                       std::move(options));
    case fuerte::StatusConflict:
      return network::opResultFromBody(std::move(body), TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED,
                                       std::move(options));
    default:
      return network::opResultFromBody(std::move(body), TRI_ERROR_INTERNAL,
                                       std::move(options));
  }
}

/// @brief Create Cluster Communication result for document
OperationResult clusterResultDocument(arangodb::fuerte::StatusCode code,
                                      std::shared_ptr<VPackBuffer<uint8_t>> body,
                                      OperationOptions options,
                                      std::unordered_map<ErrorCode, size_t> const& errorCounter) {
  switch (code) {
    case fuerte::StatusOK:
      return OperationResult(Result(), std::move(body), std::move(options), errorCounter);
    case fuerte::StatusConflict:
    case fuerte::StatusPreconditionFailed:
      return OperationResult(Result(TRI_ERROR_ARANGO_CONFLICT), std::move(body),
                             std::move(options), errorCounter);
    case fuerte::StatusNotFound:
      return network::opResultFromBody(std::move(body), TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
                                       std::move(options));
    default:
      return network::opResultFromBody(std::move(body), TRI_ERROR_INTERNAL,
                                       std::move(options));
  }
}

/// @brief Create Cluster Communication result for modify
OperationResult clusterResultModify(arangodb::fuerte::StatusCode code,
                                    std::shared_ptr<VPackBuffer<uint8_t>> body,
                                    OperationOptions options,
                                    std::unordered_map<ErrorCode, size_t> const& errorCounter) {
  switch (code) {
    case fuerte::StatusAccepted:
    case fuerte::StatusCreated: {
      options.waitForSync = (code == fuerte::StatusCreated);
      return OperationResult(Result(), std::move(body), std::move(options), errorCounter);
    }
    case fuerte::StatusConflict:
      return OperationResult(network::resultFromBody(body, TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED),
                             body, std::move(options), errorCounter);
    case fuerte::StatusPreconditionFailed:
      return OperationResult(network::resultFromBody(body, TRI_ERROR_ARANGO_CONFLICT),
                             body, std::move(options), errorCounter);
    case fuerte::StatusNotFound:
      return network::opResultFromBody(std::move(body), TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
                                       std::move(options));
    default: {
      return network::opResultFromBody(std::move(body), TRI_ERROR_INTERNAL,
                                       std::move(options));
    }
  }
}

/// @brief Create Cluster Communication result for delete
OperationResult clusterResultDelete(arangodb::fuerte::StatusCode code,
                                    std::shared_ptr<VPackBuffer<uint8_t>> body,
                                    OperationOptions options,
                                    std::unordered_map<ErrorCode, size_t> const& errorCounter) {
  switch (code) {
    case fuerte::StatusOK:
    case fuerte::StatusAccepted:
    case fuerte::StatusCreated: {
      options.waitForSync = (code != fuerte::StatusAccepted);
      return OperationResult(Result(), std::move(body), std::move(options), errorCounter);
    }
    case fuerte::StatusConflict:
    case fuerte::StatusPreconditionFailed:
      return OperationResult(network::resultFromBody(body, TRI_ERROR_ARANGO_CONFLICT),
                             body, std::move(options), errorCounter);
    case fuerte::StatusNotFound:
      return network::opResultFromBody(std::move(body), TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
                                       std::move(options));
    default: {
      return network::opResultFromBody(std::move(body), TRI_ERROR_INTERNAL,
                                       std::move(options));
    }
  }
}

}  // namespace network
}  // namespace arangodb
                   
