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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Utils.h"

#include "Agency/AgencyFeature.h"
#include "Agency/Agent.h"
#include "Basics/Common.h"
#include "Basics/NumberUtils.h"
#include "Cluster/ClusterInfo.h"
#include "Logger/Logger.h"
#include "Network/Methods.h"
#include "VocBase/ticks.h"

#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace network {

int resolveDestination(DestinationId const& dest, std::string& endpoint) {
  using namespace arangodb;

  if (dest.find("tcp://") == 0 || dest.find("ssl://") == 0) {
    endpoint = dest;
    return TRI_ERROR_NO_ERROR;  // all good
  }

  // Now look up the actual endpoint:
  auto* ci = ClusterInfo::instance();
  if (!ci) {
    return TRI_ERROR_SHUTTING_DOWN;
  }

  // This sets result.shardId, result.serverId and result.endpoint,
  // depending on what dest is. Note that if a shardID is given, the
  // responsible server is looked up, if a serverID is given, the endpoint
  // is looked up, both can fail and immediately lead to a CL_COMM_ERROR
  // state.
  ServerID serverID;
  if (dest.compare(0, 6, "shard:", 6) == 0) {
    ShardID shardID = dest.substr(6);
    {
      std::shared_ptr<std::vector<ServerID>> resp = ci->getResponsibleServer(shardID);
      if (!resp->empty()) {
        serverID = (*resp)[0];
      } else {
        LOG_TOPIC("60ee8", ERR, Logger::CLUSTER)
            << "cannot find responsible server for shard '" << shardID << "'";
        return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;
      }
    }
    LOG_TOPIC("64670", DEBUG, Logger::CLUSTER) << "Responsible server: " << serverID;
  } else if (dest.compare(0, 7, "server:", 7) == 0) {
    serverID = dest.substr(7);
  } else {
    std::string errorMessage = "did not understand destination '" + dest + "'";
    LOG_TOPIC("77a84", ERR, Logger::COMMUNICATION)
        << "did not understand destination '" << dest << "'";
    return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;
  }

  endpoint = ci->getServerEndpoint(serverID);
  if (endpoint.empty()) {
    if (serverID.find(',') != std::string::npos) {
      TRI_ASSERT(false);
    }
    std::string errorMessage =
        "did not find endpoint of server '" + serverID + "'";
    LOG_TOPIC("f29ef", ERR, Logger::COMMUNICATION)
        << "did not find endpoint of server '" << serverID << "'";
    return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;
  }
  return TRI_ERROR_NO_ERROR;
}

/// @brief extract the error code form the body
int errorCodeFromBody(arangodb::velocypack::Slice body) {
  if (body.isObject()) {
    VPackSlice num = body.get(StaticStrings::ErrorNum);
    if (num.isNumber()) {
      // we found an error number, so let's use it!
      return num.getNumericValue<int>();
    }
  }
  return TRI_ERROR_ILLEGAL_NUMBER;
}
  
Result resultFromBody(std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>> const& body,
                      int defaultError) {
  // read the error number from the response and use it if present
  if (body && !body->empty()) {
    return resultFromBody(VPackSlice(body->data()), defaultError);
  }
  return Result(defaultError);
}
  
Result resultFromBody(std::shared_ptr<arangodb::velocypack::Builder> const& body,
                      int defaultError) {
  
  // read the error number from the response and use it if present
  if (body) {
    return resultFromBody(body->slice(), defaultError);
  }

  return Result(defaultError);
}
  
Result resultFromBody(arangodb::velocypack::Slice slice,
                      int defaultError) {
  // read the error number from the response and use it if present
  if (slice.isObject()) {
    VPackSlice num = slice.get(StaticStrings::ErrorNum);
    VPackSlice msg = slice.get(StaticStrings::ErrorMessage);
    if (num.isNumber()) {
      if (msg.isString()) {
        // found an error number and an error message, so let's use it!
        return Result(num.getNumericValue<int>(), msg.copyString());
      }
      // we found an error number, so let's use it!
      return Result(num.getNumericValue<int>());
    }
  }
  return Result(defaultError);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Extract all error baby-style error codes and store them in a map
////////////////////////////////////////////////////////////////////////////////

void errorCodesFromHeaders(network::Headers headers,
                           std::unordered_map<int, size_t>& errorCounter,
                           bool includeNotFound) {
  auto const& codes = headers.find(StaticStrings::ErrorCodes);
  if (codes != headers.end()) {
    auto parsedCodes = VPackParser::fromJson(codes->second);
    VPackSlice codesSlice = parsedCodes->slice();
    if (!codesSlice.isObject()) {
      return;
    }
    
    for (auto const& code : VPackObjectIterator(codesSlice)) {
      VPackValueLength codeLength;
      char const* codeString = code.key.getString(codeLength);
      int codeNr = NumberUtils::atoi_zero<int>(codeString, codeString + codeLength);
      if (includeNotFound || codeNr != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        errorCounter[codeNr] += code.value.getNumericValue<size_t>();
      }
    }
  }
}
  
int fuerteToArangoErrorCode(network::Response const& res) {
  return fuerteToArangoErrorCode(res.error);
}
  
int fuerteToArangoErrorCode(fuerte::Error err) {

  // This function creates an error code from a ClusterCommResult,
  // but only if it is a communication error. If the communication
  // was successful and there was an HTTP error code, this function
  // returns TRI_ERROR_NO_ERROR.
  // If TRI_ERROR_NO_ERROR is returned, then the result was CL_COMM_RECEIVED
  // and .answer can safely be inspected.
  
  
//  LOG_TOPIC_IF("abcde", ERR, Logger::CLUSTER, res.error != fuerte::Error::NoError) << fuerte::to_string(res.error);
  
  switch (err) {
    case fuerte::Error::NoError:
      return TRI_ERROR_NO_ERROR;

    case fuerte::Error::CouldNotConnect:
      return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;

    case fuerte::Error::CloseRequested:
    case fuerte::Error::ConnectionClosed:
      return TRI_ERROR_CLUSTER_CONNECTION_LOST;

    case fuerte::Error::Timeout:  // No reply, we give up:
      return TRI_ERROR_CLUSTER_TIMEOUT;

    case fuerte::Error::QueueCapacityExceeded:  // there is no result
    case fuerte::Error::ReadError:
    case fuerte::Error::WriteError:
    case fuerte::Error::Canceled:
    case fuerte::Error::ProtocolError:
      return TRI_ERROR_CLUSTER_CONNECTION_LOST;
      
    case fuerte::Error::VstUnauthorized:
      return TRI_ERROR_FORBIDDEN;
  }

  return TRI_ERROR_INTERNAL;
}

/// @brief Create Cluster Communication result for insert
OperationResult clusterResultInsert(arangodb::fuerte::StatusCode code,
                                    std::shared_ptr<VPackBuffer<uint8_t>> body,
                                    OperationOptions const& options,
                                    std::unordered_map<int, size_t> const& errorCounter) {
  switch (code) {
    case fuerte::StatusAccepted:
      return OperationResult(Result(), std::move(body), options, errorCounter);
    case fuerte::StatusCreated: {
      OperationOptions copy = options;
      copy.waitForSync = true;  // wait for sync is abused herea
      // operationResult should get a return code.
      return OperationResult(Result(), std::move(body), copy, errorCounter);
    }
    case fuerte::StatusPreconditionFailed:
      return network::opResultFromBody(body, TRI_ERROR_ARANGO_CONFLICT);
    case fuerte::StatusBadRequest:
      return network::opResultFromBody(body, TRI_ERROR_INTERNAL);
    case fuerte::StatusNotFound:
      return network::opResultFromBody(body, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    case fuerte::StatusConflict:
      return network::opResultFromBody(body, TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
    default:
      return network::opResultFromBody(body, TRI_ERROR_INTERNAL);
  }
}

/// @brief Create Cluster Communication result for document
OperationResult clusterResultDocument(arangodb::fuerte::StatusCode code,
                                      std::shared_ptr<VPackBuffer<uint8_t>> body,
                                      OperationOptions const& options,
                                      std::unordered_map<int, size_t> const& errorCounter) {
  switch (code) {
    case fuerte::StatusOK:
      return OperationResult(Result(), std::move(body), options, errorCounter);
    case fuerte::StatusPreconditionFailed:
      return OperationResult(Result(TRI_ERROR_ARANGO_CONFLICT), std::move(body),
                             options, errorCounter);
    case fuerte::StatusNotFound:
      return network::opResultFromBody(body, TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
    default:
      return network::opResultFromBody(body, TRI_ERROR_INTERNAL);
  }
}
  
/// @brief Create Cluster Communication result for modify
OperationResult clusterResultModify(arangodb::fuerte::StatusCode code,
                                    std::shared_ptr<VPackBuffer<uint8_t>> body,
                                    OperationOptions const& options,
                                    std::unordered_map<int, size_t> const& errorCounter) {
  switch (code) {
    case fuerte::StatusAccepted:
    case fuerte::StatusCreated: {
      OperationOptions options;
      options.waitForSync = (code == fuerte::StatusCreated);
      return OperationResult(Result(), std::move(body), options, errorCounter);
    }
    case fuerte::StatusConflict:
      return OperationResult(network::resultFromBody(body, TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED),
                             body, options, errorCounter);
    case fuerte::StatusPreconditionFailed:
      return OperationResult(network::resultFromBody(body, TRI_ERROR_ARANGO_CONFLICT),
                             body, options, errorCounter);
    case fuerte::StatusNotFound:
      return network::opResultFromBody(body, TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
    default: {
      return network::opResultFromBody(body, TRI_ERROR_INTERNAL);
    }
  }
}
  
/// @brief Create Cluster Communication result for delete
OperationResult clusterResultDelete(arangodb::fuerte::StatusCode code,
                                    std::shared_ptr<VPackBuffer<uint8_t>> body,
                                    OperationOptions const& options,
                                    std::unordered_map<int, size_t> const& errorCounter) {
  switch (code) {
    case fuerte::StatusOK:
    case fuerte::StatusAccepted:
    case fuerte::StatusCreated: {
      OperationOptions options;
      options.waitForSync = (code != fuerte::StatusAccepted);
      return OperationResult(Result(), std::move(body), options, errorCounter);
    }
    case fuerte::StatusPreconditionFailed:
      return OperationResult(network::resultFromBody(body, TRI_ERROR_ARANGO_CONFLICT),
                             body, options, errorCounter);
    case fuerte::StatusNotFound:
      return network::opResultFromBody(body, TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
    default: {
      return network::opResultFromBody(body, TRI_ERROR_INTERNAL);
    }
  }
}
}  // namespace network
}  // namespace arangodb
