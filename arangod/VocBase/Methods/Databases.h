////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Result.h"
#include "Basics/debugging.h"
#include "VocBase/voc-types.h"
#include "VocBase/VocbaseInfo.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

struct TRI_vocbase_t;

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
struct OperationOptions;
namespace methods {

struct Databases {
  static std::vector<std::string> list(ArangodServer& server,
                                       std::string const& user = "");
  static Result info(TRI_vocbase_t* vocbase, velocypack::Builder& result);
  static Result create(ArangodServer& server, ExecContext const& context,
                       std::string const& dbName, velocypack::Slice users,
                       velocypack::Slice options);
  static Result drop(ExecContext const& context, TRI_vocbase_t* systemVocbase,
                     std::string const& dbName);

 private:
  /// @brief will retry for at most <timeout> seconds
  static Result grantCurrentUser(CreateDatabaseInfo const& info,
                                 int64_t timeout);

  static Result createCoordinator(CreateDatabaseInfo const& info);
  static Result createOther(CreateDatabaseInfo const& info);
};
}  // namespace methods
}  // namespace arangodb
