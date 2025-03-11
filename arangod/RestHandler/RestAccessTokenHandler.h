////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RestHandler/RestVocbaseBaseHandler.h"

namespace arangodb {
namespace auth {
class UserManager;
}

class RestAccessTokenHandler : public RestVocbaseBaseHandler {
 public:
  RestAccessTokenHandler(ArangodServer&, GeneralRequest*, GeneralResponse*);

 public:
  char const* name() const override final { return "RestAccessTokenHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_SLOW; }
  RestStatus execute() override;

 private:
  RestStatus createAccessToken(auth::UserManager*, std::string const& user);
  RestStatus deleteAccessToken(auth::UserManager*, std::string const& user);
  RestStatus showAccessTokens(auth::UserManager*, std::string const& user);
};
}  // namespace arangodb
