////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_VOC_BASE_API_DATABASE_H
#define ARANGOD_VOC_BASE_API_DATABASE_H 1

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include "Basics/Result.h"
#include "Basics/debugging.h"
#include "VocBase/voc-types.h"
#include "VocBase/VocbaseInfo.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
struct OperationOptions;
namespace methods {

/// Common code for the db._database(),
struct Databases {
  static std::vector<std::string> list(application_features::ApplicationServer& server,
                                       std::string const& user = "");
  static arangodb::Result info(TRI_vocbase_t* vocbase, VPackBuilder& result);
  static arangodb::Result create(application_features::ApplicationServer& server,
                                 ExecContext const& context, std::string const& dbName,
                                 VPackSlice const& users, VPackSlice const& options);
  static arangodb::Result drop(ExecContext const& context, TRI_vocbase_t* systemVocbase,
                               std::string const& dbName);

 private:
  /// @brief will retry for at most <timeout> seconds
  static arangodb::Result grantCurrentUser(CreateDatabaseInfo const& info, int64_t timeout);

  static arangodb::Result createCoordinator(CreateDatabaseInfo const& info);
  static arangodb::Result createOther(CreateDatabaseInfo const& info);
};
}  // namespace methods
}  // namespace arangodb

#endif
