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
#include "Basics/Result.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace methods {

/// Common code for the db._database(),
struct Databases {
  static TRI_vocbase_t* lookup(std::string const& dbname);
  static std::vector<std::string> list(std::string const& user = "");
  static arangodb::Result info(TRI_vocbase_t* vocbase,
                               arangodb::velocypack::Builder& result);
  static arangodb::Result create(std::string const& dbName,
                                 arangodb::velocypack::Slice const& users,
                                 arangodb::velocypack::Slice const& options);
  static arangodb::Result drop(TRI_vocbase_t* systemVocbase,
                               std::string const&);
};
}
}

#endif
