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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_SERVER_VOCBASE_CONTEXT_H
#define ARANGOD_REST_SERVER_VOCBASE_CONTEXT_H 1

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/Common.h"
#include "Utils/ExecContext.h"

#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "Rest/RequestContext.h"
#include "GeneralServer/AuthenticationFeature.h"

#include "Rest/GeneralRequest.h"
#include "Rest/GeneralResponse.h"

struct TRI_vocbase_t;

namespace arangodb {
class VocbaseContext final : public arangodb::RequestContext {
 public:
  static double ServerSessionTtl;

 public:
  VocbaseContext(VocbaseContext const&) = delete;
  VocbaseContext& operator=(VocbaseContext const&) = delete;

  VocbaseContext(GeneralRequest*, TRI_vocbase_t*);
  ~VocbaseContext();

 public:
  TRI_vocbase_t* vocbase() const { return _vocbase; }

 public:
  rest::ResponseCode authenticate() override final;

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks the authentication (basic)
  //////////////////////////////////////////////////////////////////////////////

  rest::ResponseCode basicAuthentication(char const*);
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks the authentication (jwt)
  //////////////////////////////////////////////////////////////////////////////

  rest::ResponseCode jwtAuthentication(std::string const&);

 private: 
  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks the authentication header and sets user if successful
  //////////////////////////////////////////////////////////////////////////////

  rest::ResponseCode authenticateRequest();

 private:
  TRI_vocbase_t* _vocbase;
  AuthenticationFeature* _authentication;
  std::shared_ptr<ExecContext> _execContext;

};
}

#endif
