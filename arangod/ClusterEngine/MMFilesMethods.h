////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_CLUSTER_ENGINE_MMFILES_METHODS_H
#define ARANGOD_CLUSTER_ENGINE_MMFILES_METHODS_H 1

#include <string>

namespace arangodb {
namespace mmfiles {
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief rotate the active journals for the collection on all DBServers
  ////////////////////////////////////////////////////////////////////////////////
  
  int rotateActiveJournalOnAllDBServers(std::string const& dbname,
                                        std::string const& collname);
}}

#endif
