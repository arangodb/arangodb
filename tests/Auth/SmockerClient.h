////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Network/ConnectionPool.h"

#include <fuerte/types.h>

#include <memory>
#include <string>

namespace arangodb::test {

class SmockerClient {
 public:
  SmockerClient(std::string containerName, std::string mockUrl,
                std::string adminUrl);

  void start();
  void stop();
  void resetMocks();
  void addMock(std::string const& path, int status,
               std::string const& responseBody);

  auto mockUrl() const -> std::string const& { return mockUrl_; }

 private:
  auto sendToAdmin(fuerte::RestVerb verb, std::string const& path,
                   std::string const& body = {})
      -> std::unique_ptr<fuerte::Response>;

  std::string containerName_;
  std::string mockUrl_;
  std::string adminUrl_;
  std::unique_ptr<network::ConnectionPool> adminPool_;
};

}  // namespace arangodb::test
