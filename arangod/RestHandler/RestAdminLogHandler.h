////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RestHandler/RestBaseHandler.h"

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief admin log request handler
////////////////////////////////////////////////////////////////////////////////

class RestAdminLogHandler : public RestBaseHandler {
 public:
  explicit RestAdminLogHandler(ArangodServer&, GeneralRequest*,
                               GeneralResponse*);

 public:
  char const* name() const override final { return "RestAdminLogHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_FAST; }
  auto executeAsync() -> futures::Future<futures::Unit> override;

 protected:
  // we just use the database from the URL to log it.
  [[nodiscard]] auto makeSharedLogContextValue() const
      -> std::shared_ptr<LogContext::Values> override {
    return LogContext::makeValue()
        .with<structuredParams::UrlName>(_request->fullUrl())
        .with<structuredParams::UserName>(_request->user())
        .with<structuredParams::DatabaseName>(_request->databaseName())
        .share();
  }

 private:
  arangodb::Result verifyPermitted();
  void clearLogs();
  auto reportLogs(bool newFormat) -> async<void>;
  auto handleLogLevel() -> async<void>;
  auto handleLogWrite() -> async<void>;
  void handleLogStructuredParams();
};
}  // namespace arangodb
