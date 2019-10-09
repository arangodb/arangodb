////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_REST_HANDLER_REST_USERS_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_USERS_HANDLER_H 1

#include "RestHandler/RestBaseHandler.h"

namespace arangodb {
namespace auth {
class UserManager;
}

class RestUsersHandler : public arangodb::RestBaseHandler {
 public:
  RestUsersHandler(application_features::ApplicationServer&, GeneralRequest*,
                   GeneralResponse*);

 public:
  virtual char const* name() const override { return "RestUsersHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_SLOW; }
  RestStatus execute() override;

 private:
  bool isAdminUser() const;
  bool canAccessUser(std::string const& user) const;

  /// helper to generate a compliant response for individual user requests
  void generateUserResult(rest::ResponseCode code,
                          velocypack::Builder const& doc);

  void generateDatabaseResult(auth::UserManager*, std::string const& user, bool full);

  RestStatus getRequest(auth::UserManager*);
  RestStatus postRequest(auth::UserManager*);
  RestStatus putRequest(auth::UserManager*);
  RestStatus patchRequest(auth::UserManager*);
  RestStatus deleteRequest(auth::UserManager*);
};
}  // namespace arangodb

#endif
