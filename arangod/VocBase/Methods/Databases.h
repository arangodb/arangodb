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

#ifndef ARANGOD_VOC_BASE_API_DATABASE_H
#define ARANGOD_VOC_BASE_API_DATABASE_H 1

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include "Basics/Result.h"
#include "VocBase/voc-types.h"


struct TRI_vocbase_t;

namespace arangodb {
namespace methods {


class CreateDatabaseInfo {
 public:
  CreateDatabaseInfo() = default;
  Result load(std::string const& name,
              VPackSlice const& options,
              VPackSlice const& users);

  Result buildSlice(VPackBuilder& builder) const;

  uint64_t getId() const { return _id; };
  std::string getName() const { return _name; };
  VPackSlice const& getUsers() const { return _userSlice; }

 private:
  Result sanitizeUsers(VPackSlice const& users, VPackBuilder& sanitizedUsers);
  Result sanitizeOptions(VPackSlice const& options, VPackBuilder& sanitizedOptions);

 private:
  uint64_t _id;
  std::string _name;
  VPackBuilder _options;
  VPackBuilder _users;
  VPackSlice _userSlice;
};

/// Common code for the db._database(),
struct Databases {
  static TRI_vocbase_t* lookup(std::string const& dbname);
  static TRI_vocbase_t* lookup(TRI_voc_tick_t);
  static std::vector<std::string> list(std::string const& user = "");
  static arangodb::Result info(TRI_vocbase_t* vocbase, VPackBuilder& result);
  static arangodb::Result create(std::string const& dbName,
                                 VPackSlice const& users,
                                 VPackSlice const& options);
  static arangodb::Result drop(TRI_vocbase_t* systemVocbase, std::string const& dbName);

 private:
  /// @brief will retry for at most <timeout> seconds
  static arangodb::Result grantCurrentUser(CreateDatabaseInfo const& info, double timeout);

  static arangodb::Result createCoordinator(CreateDatabaseInfo const& info);
  static arangodb::Result createOther(CreateDatabaseInfo const& info);
};
}  // namespace methods
}  // namespace arangodb

#endif
