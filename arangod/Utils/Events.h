////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_UTILS_EVENTS_H
#define ARANGOD_UTILS_EVENTS_H 1

#include "Basics/Common.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Rest/CommonDefines.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {
class GeneralRequest;

namespace events {
void UnknownAuthenticationMethod(GeneralRequest const&);
void CredentialsMissing(GeneralRequest const&);
void CredentialsBad(GeneralRequest const&, rest::AuthenticationMethod);
void PasswordChangeRequired(GeneralRequest const&);
void Authenticated(GeneralRequest const&, rest::AuthenticationMethod);
void NotAuthorized(GeneralRequest const&);
void CreateCollection(std::string const& db, std::string const& name, int result);
void DropCollection(std::string const& db, std::string const& name, int result);
void TruncateCollection(std::string const& db, std::string const& name, int result);
void CreateDatabase(std::string const& name, int result);
void DropDatabase(std::string const& name, int result);
void CreateIndex(std::string const& db, std::string const& col, VPackSlice const&);
void DropIndex(std::string const& db, std::string const& col,
               std::string const& idx, int result);
void CreateView(std::string const& db, std::string const& name, int result);
void DropView(std::string const& db, std::string const& name, int result);
}  // namespace events
}  // namespace arangodb

#endif
