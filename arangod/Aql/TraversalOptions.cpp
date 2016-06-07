////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "Aql/TraversalOptions.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;
using Json = arangodb::basics::Json;
using JsonHelper = arangodb::basics::JsonHelper;

TraversalOptions::TraversalOptions(Json const& json) {
  Json obj = json.get("traversalFlags");
  useBreathFirst = JsonHelper::getBooleanValue(obj.json(), "bfs", false);
  std::string tmp = JsonHelper::getStringValue(obj.json(), "uniqueVertices", "");
  if (tmp == "path") {
    uniqueVertices =
        arangodb::traverser::TraverserOptions::UniquenessLevel::PATH;
  } else if (tmp == "global") {
    uniqueVertices =
        arangodb::traverser::TraverserOptions::UniquenessLevel::GLOBAL;
  } else {
    uniqueVertices =
        arangodb::traverser::TraverserOptions::UniquenessLevel::NONE;
  }

  tmp = JsonHelper::getStringValue(obj.json(), "uniqueEdges", "");
  if (tmp == "none") {
    uniqueEdges =
        arangodb::traverser::TraverserOptions::UniquenessLevel::NONE;
  } else if (tmp == "global") {
    uniqueEdges =
        arangodb::traverser::TraverserOptions::UniquenessLevel::GLOBAL;
  } else {
    uniqueEdges =
        arangodb::traverser::TraverserOptions::UniquenessLevel::PATH;
  }
}

void TraversalOptions::toJson(arangodb::basics::Json& json,
                              TRI_memory_zone_t* zone) const {
  Json flags;

  flags = Json(Json::Object, 3);
  flags("bfs", Json(useBreathFirst));

  switch (uniqueVertices) {
    case arangodb::traverser::TraverserOptions::UniquenessLevel::NONE:
      flags("uniqueVertices", Json("none"));
      break;
    case arangodb::traverser::TraverserOptions::UniquenessLevel::PATH:
      flags("uniqueVertices", Json("path"));
      break;
    case arangodb::traverser::TraverserOptions::UniquenessLevel::GLOBAL:
      flags("uniqueVertices", Json("global"));
      break;
  }

  switch (uniqueEdges) {
    case arangodb::traverser::TraverserOptions::UniquenessLevel::NONE:
      flags("uniqueEdges", Json("none"));
      break;
    case arangodb::traverser::TraverserOptions::UniquenessLevel::PATH:
      flags("uniqueEdges", Json("path"));
      break;
    case arangodb::traverser::TraverserOptions::UniquenessLevel::GLOBAL:
      flags("uniqueEdges", Json("global"));
      break;
  }

  json("traversalFlags", flags);
}

void TraversalOptions::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder guard(&builder);

  builder.add("bfs", VPackValue(useBreathFirst));

  switch (uniqueVertices) {
    case arangodb::traverser::TraverserOptions::UniquenessLevel::NONE:
      builder.add("uniqueVertices", VPackValue("none"));
      break;
    case arangodb::traverser::TraverserOptions::UniquenessLevel::PATH:
      builder.add("uniqueVertices", VPackValue("path"));
      break;
    case arangodb::traverser::TraverserOptions::UniquenessLevel::GLOBAL:
      builder.add("uniqueVertices", VPackValue("global"));
      break;
  }

  switch (uniqueEdges) {
    case arangodb::traverser::TraverserOptions::UniquenessLevel::NONE:
      builder.add("uniqueEdges", VPackValue("none"));
      break;
    case arangodb::traverser::TraverserOptions::UniquenessLevel::PATH:
      builder.add("uniqueEdges", VPackValue("path"));
      break;
    case arangodb::traverser::TraverserOptions::UniquenessLevel::GLOBAL:
      builder.add("uniqueEdges", VPackValue("global"));
      break;
  }
}
