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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <string>
#include <variant>

#include "Inspection/Types.h"
#include "Pregel/PregelOptions.h"
#include "velocypack/Builder.h"

namespace arangodb::pregel {

struct RestGeneralOptions {
  std::string algorithm;
  VPackBuilder userParameters;
  std::unordered_map<std::string, std::vector<std::string>>
      edgeCollectionRestrictions;
  // A switch between running pregel with or without actors
  // Can be deleted if we finished refactoring to use only actors
  bool useActors;
};
template<typename Inspector>
auto inspect(Inspector& f, RestGeneralOptions& x) {
  return f.object(x).fields(
      f.field("algorithm", x.algorithm),
      f.field("params", x.userParameters)
          .fallback(VPackSlice::emptyObjectSlice()),
      f.field("edgeCollectionRestrictions", x.edgeCollectionRestrictions)
          .fallback(
              std::unordered_map<std::string, std::vector<std::string>>{}),
      f.field("actors", x.useActors).fallback(true));
}

struct RestCollectionSettings {
  RestGeneralOptions options;
  std::vector<std::string> vertexCollections;
  std::vector<std::string> edgeCollections;
};
template<typename Inspector>
auto inspect(Inspector& f, RestCollectionSettings& x) {
  return f.object(x).fields(f.embedFields(x.options),
                            f.field("vertexCollections", x.vertexCollections),
                            f.field("edgeCollections", x.edgeCollections));
}

struct RestGraphSettings {
  RestGeneralOptions options;
  std::string graph;
};
template<typename Inspector>
auto inspect(Inspector& f, RestGraphSettings& x) {
  return f.object(x).fields(f.embedFields(x.options),
                            f.field("graphName", x.graph));
}

struct RestOptions : std::variant<RestGraphSettings, RestCollectionSettings> {
  auto options() -> PregelOptions;
};
template<typename Inspector>
auto inspect(Inspector& f, RestOptions& x) {
  return f.variant(x).unqualified().alternatives(
      inspection::inlineType<RestCollectionSettings>(),
      inspection::inlineType<RestGraphSettings>());
}

}  // namespace arangodb::pregel
