////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_UTILS_EVENTS_H
#define ARANGOD_UTILS_EVENTS_H 1

#include "Basics/Common.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Rest/CommonDefines.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {
class GeneralRequest;
class GeneralResponse;
struct OperationResult;

namespace events {
void UnknownAuthenticationMethod(GeneralRequest const&);
void CredentialsMissing(GeneralRequest const&);
void LoggedIn(GeneralRequest const&, std::string const& username);
void CredentialsBad(GeneralRequest const&, std::string const& username);
void CredentialsBad(GeneralRequest const&, rest::AuthenticationMethod);
void Authenticated(GeneralRequest const&, rest::AuthenticationMethod);
void NotAuthorized(GeneralRequest const&);
void CreateCollection(std::string const& db, std::string const& name, int result);
void DropCollection(std::string const& db, std::string const& name, int result);
void PropertyUpdateCollection(std::string const& db, std::string const& collectionName,
                              OperationResult const&);
void TruncateCollection(std::string const& db, std::string const& name,
                        OperationResult const& result);
void CreateDatabase(std::string const& name, Result const& result, ExecContext const& context);
void DropDatabase(std::string const& name, Result const& result, ExecContext const& context);
void CreateIndex(std::string const& db, std::string const& col, VPackSlice const&, int result);
void DropIndex(std::string const& db, std::string const& col,
               std::string const& idx, int result);
void CreateView(std::string const& db, std::string const& name, int result);
void DropView(std::string const& db, std::string const& name, int result);
void CreateDocument(std::string const& db, std::string const& collection,
                    VPackSlice const& document, OperationOptions const& options, int code);
void DeleteDocument(std::string const& db, std::string const& collection,
                    VPackSlice const& document, OperationOptions const& options, int code);
void ReadDocument(std::string const& db, std::string const& collection,
                  VPackSlice const& document, OperationOptions const& options, int code);
void ReplaceDocument(std::string const& db, std::string const& collection,
                     VPackSlice const& document, OperationOptions const& options, int code);
void ModifyDocument(std::string const& db, std::string const& collection,
                    VPackSlice const& document, OperationOptions const& options, int code);
void IllegalDocumentOperation(GeneralRequest const&, int result);
void QueryDocument(std::string const& db, std::string const&, std::string const&, int code);
void QueryDocument(std::string const& db, VPackSlice const&, int code);
void QueryDocument(GeneralRequest const&, GeneralResponse const*, VPackSlice const&);
void CreateHotbackup(std::string const& id, int result);
void RestoreHotbackup(std::string const& id, int result);
void DeleteHotbackup(std::string const& id, int result);
}  // namespace events
}  // namespace arangodb

#endif
