////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Roman Rabinovich
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>

struct TRI_vocbase_t;

namespace arangodb::pregel3 {

class Utils {
 public:
  Utils() = delete;

  // constants

  // numbers
  static constexpr uint32_t standardBatchSize = 10000;

  // strings
  static constexpr auto vertexCollNames = "vertexCollNames";
  static constexpr auto edgeCollNames = "edgeCollNames";
  static constexpr auto wrongGraphSpecErrMsg =
      "The graph specification is either the graph name or an object "
      "containing a list of vertex collection names and a list of edge "
      "collection names.";
  static constexpr auto wrongRequestBody =
      "Could not parse the body of the request.";
  static constexpr auto queryId = "queryId";
  static constexpr auto graphSpec = "graphSpec";
  static constexpr auto graphName = "graphName";
  static constexpr auto vertexCollectionNames = "vertexCollectionNames";
  static constexpr auto edgeCollectionNames = "edgeCollectionNames";
  static constexpr auto state = "state";
};
}  // namespace arangodb::pregel3
