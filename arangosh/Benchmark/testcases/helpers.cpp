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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "helpers.h"

#include "../BenchFeature.h"

#include "Basics/StringUtils.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

namespace arangodb::arangobench {

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a collection
////////////////////////////////////////////////////////////////////////////////

bool DeleteCollection(arangodb::httpclient::SimpleHttpClient* client,
                      std::string const& name) {
  std::unordered_map<std::string, std::string> headerFields;
  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> result(
      client->request(rest::RequestType::DELETE_REQ, "/_api/collection/" + name,
                      "", 0, headerFields));

  bool failed = true;
  if (result != nullptr) {
    int statusCode = result->getHttpReturnCode();
    if (statusCode == 200 || statusCode == 201 || statusCode == 202 || statusCode == 404) {
      failed = false;
    }
  }

  return !failed;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a collection
////////////////////////////////////////////////////////////////////////////////

bool CreateCollection(arangodb::httpclient::SimpleHttpClient* client,
                      std::string const& name, int const type, BenchFeature const& arangobench) {
  std::unordered_map<std::string, std::string> headerFields;

  std::string payload =
      "{\"name\":\"" + name + "\",\"type\":" + basics::StringUtils::itoa(type) +
      ",\"replicationFactor\":" + basics::StringUtils::itoa(arangobench.replicationFactor()) +
      ",\"numberOfShards\":" + basics::StringUtils::itoa(arangobench.numberOfShards()) +
      ",\"waitForSync\":" + (arangobench.waitForSync() ? "true" : "false") +
      "}";

  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> result(
      client->request(rest::RequestType::POST, "/_api/collection",
                      payload.c_str(), payload.size(), headerFields));

  bool failed = true;

  if (result != nullptr) {
    int statusCode = result->getHttpReturnCode();
    if (statusCode == 200 || statusCode == 201 || statusCode == 202) {
      failed = false;
    } else {
      LOG_TOPIC("567b3", WARN, Logger::FIXME)
          << "error when creating collection: " << result->getHttpReturnMessage()
          << " for payload '" << payload << "': " << result->getBody();
    }
  }

  return !failed;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an index
////////////////////////////////////////////////////////////////////////////////

bool CreateIndex(arangodb::httpclient::SimpleHttpClient* client, std::string const& name,
                 std::string const& type, std::string const& fields) {
  std::unordered_map<std::string, std::string> headerFields;

  std::string payload =
      "{\"type\":\"" + type + "\",\"fields\":" + fields + ",\"unique\":false}";

  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> result(
      client->request(rest::RequestType::POST, "/_api/index?collection=" + name,
                      payload.c_str(), payload.size(), headerFields));

  bool failed = true;

  if (result != nullptr) {
    if (result->getHttpReturnCode() == 200 || result->getHttpReturnCode() == 201) {
      failed = false;
    } else {
      LOG_TOPIC("1dcba", WARN, Logger::FIXME)
          << "error when creating index: " << result->getHttpReturnMessage()
          << " for payload '" << payload << "': " << result->getBody();
    }
  }

  return !failed;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document
////////////////////////////////////////////////////////////////////////////////

bool CreateDocument(arangodb::httpclient::SimpleHttpClient* client,
                    std::string const& collection, std::string const& payload) {
  std::unordered_map<std::string, std::string> headerFields;

  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> result(
      client->request(rest::RequestType::POST, "/_api/document?collection=" + collection,
                      payload.c_str(), payload.size(), headerFields));

  bool failed = true;

  if (result != nullptr) {
    if (result->getHttpReturnCode() == 200 || result->getHttpReturnCode() == 201 ||
        result->getHttpReturnCode() == 202) {
      failed = false;
    }
  }

  return !failed;
}

}  // namespace arangodb::arangobench
