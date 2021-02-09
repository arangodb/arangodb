////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_MOCKS_PREPARED_RESPONSE_CONNECTION_POOL
#define ARANGODB_MOCKS_PREPARED_RESPONSE_CONNECTION_POOL 1

#include "Network/ConnectionPool.h"
#include "Rest/CommonDefines.h"
#include "Rest/GeneralResponse.h"

#include <vector>

struct GeneralRequestMock;

namespace arangodb {

class AgencyCache;

namespace fuerte {
inline namespace v1 {
class Connection;
class ConnectionBuilder;
}  // namespace v1
}  // namespace fuerte

namespace tests {

class PreparedRequestResponse {
 public:
  PreparedRequestResponse(TRI_vocbase_t& vocbase);

  void setRequestType(arangodb::rest::RequestType type);
  void addSuffix(std::string suffix);
  void addRestSuffix(std::string suffix);
  void addBody(VPackSlice slice);

  std::unique_ptr<GeneralRequestMock> generateRequest() const;

  void rememberResponse(std::unique_ptr<GeneralResponse> response);

  bool operator==(fuerte::Request const& other) const;

  std::unique_ptr<fuerte::Response> generateResponse() const;

 private:
  TRI_vocbase_t& _vocbase;
  std::string _dbName;
  arangodb::rest::RequestType _type;
  std::vector<std::string> _suffixes;
  std::vector<std::string> _fullSuffixes;
  std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>> _payload;
  std::unique_ptr<GeneralResponse> _response;
};

class PreparedResponseConnectionPool final : public arangodb::network::ConnectionPool {
 public:
  explicit PreparedResponseConnectionPool(arangodb::AgencyCache& agencyCache,
                                          arangodb::network::ConnectionPool::Config const& config);

  std::shared_ptr<arangodb::fuerte::Connection> createConnection(arangodb::fuerte::ConnectionBuilder&) override;

  void addPreparedResponses(std::pair<std::string, std::string> const& endpoint,
                            std::vector<PreparedRequestResponse> responses);

 private:
  arangodb::AgencyCache& _cache;
  std::unordered_map<std::string, std::shared_ptr<std::vector<PreparedRequestResponse>>> _responses;
};

}  // namespace tests
}  // namespace arangodb

#endif
