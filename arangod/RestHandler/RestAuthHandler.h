////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_HANDLER_REST_AUTH_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_AUTH_HANDLER_H 1

#include "Basics/Common.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

#include <chrono>

namespace arangodb {
class RestAuthHandler : public RestVocbaseBaseHandler {
 public:
  RestAuthHandler(GeneralRequest*, GeneralResponse*);

  std::string generateJwt(std::string const&, std::string const&);

 public:
  char const* name() const override final { return "RestAuthHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_SLOW; }
  RestStatus execute() override;

#ifdef USE_ENTERPRISE
  void shutdownExecute(bool isFinalized) noexcept override;
#endif

 private:
  RestStatus badRequest();

 private:
  std::string _username;
  bool _isValid = false;
  std::chrono::seconds _validFor;
};
}  // namespace arangodb

#endif
