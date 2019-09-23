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
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <v8.h>

#include "Auth/AuthUser.h"
#include "Auth/DatabaseResource.h"
#include "Auth/User.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

struct TRI_vocbase_t;
struct TRI_v8_global_t;

namespace arangodb {
class TestHelper {
 public:
  void mockDatabase();
  void unmockDatabase();

  void setupV8();
  v8::Isolate* v8Isolate();
  v8::Local<v8::Context> v8Context();
  TRI_v8_global_t* v8Globals();

  std::unique_ptr<arangodb::ExecContext> createExecContext(auth::AuthUser const&,
                                                           auth::DatabaseResource const&);

  void createUser(std::string const& username, std::function<void(auth::User*)> callback);

  TRI_vocbase_t* createDatabase(std::string const& dbName);

  std::shared_ptr<arangodb::LogicalCollection> createCollection(TRI_vocbase_t*,
                                                                auth::CollectionResource const&);

  std::shared_ptr<arangodb::LogicalView> createView(TRI_vocbase_t*,
                                                    auth::CollectionResource const&);

  void callFunction(v8::Handle<v8::Value>, std::vector<v8::Local<v8::Value>>&);

  void callFunctionThrow(v8::Handle<v8::Value>,
                         std::vector<v8::Local<v8::Value>>&, int errorCode);
};
}  // namespace arangodb
