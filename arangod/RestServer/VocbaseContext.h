////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#include "Rest/GeneralRequest.h"
#include "Utils/ExecContext.h"

struct TRI_vocbase_t;

namespace arangodb {

/// @brief just also stores the context
class VocbaseContext : public arangodb::ExecContext {
 public:
  ~VocbaseContext();

  static VocbaseContext* create(GeneralRequest& req, TRI_vocbase_t& vocbase);
  TEST_VIRTUAL TRI_vocbase_t& vocbase() const { return _vocbase; }

  /// @brief upgrade to internal superuser
  void forceSuperuser();

  /// @brief upgrade to internal read-only user
  void forceReadOnly();

 private:
  TRI_vocbase_t& _vocbase;

  VocbaseContext(GeneralRequest& req, TRI_vocbase_t& vocbase, ExecContext::Type type,
                 auth::Level systemLevel, auth::Level dbLevel, bool isAdminUser);
  VocbaseContext(VocbaseContext const&) = delete;
  VocbaseContext& operator=(VocbaseContext const&) = delete;
};

}  // namespace arangodb

#endif
