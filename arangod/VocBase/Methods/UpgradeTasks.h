////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include <velocypack/Slice.h>

struct TRI_vocbase_t;

namespace arangodb::methods {

/// Code to create and initialize databases
/// Replaces upgrade-database.js for good
struct UpgradeTasks {
  static Result createSystemCollectionsAndIndices(TRI_vocbase_t& vocbase,
                                                  velocypack::Slice slice);
  static Result createStatisticsCollectionsAndIndices(TRI_vocbase_t& vocbase,
                                                      velocypack::Slice slice);
  static Result addDefaultUserOther(TRI_vocbase_t& vocbase,
                                    velocypack::Slice slice);
  static Result renameReplicationApplierStateFiles(TRI_vocbase_t& vocbase,
                                                   velocypack::Slice slice);
  static Result dropLegacyAnalyzersCollection(TRI_vocbase_t& vocbase,
                                              velocypack::Slice slice);
  static Result dropPregelQueriesCollection(TRI_vocbase_t& vocbase,
                                            velocypack::Slice slice);
};

}  // namespace arangodb::methods
