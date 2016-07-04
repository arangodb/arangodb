////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////


#ifndef ARANGOD_SINGLE_SERVER_TRAVERSAL_PATH_H
#define ARANGOD_SINGLE_SERVER_TRAVERSAL_PATH_H 1

#include "VocBase/SingleServerTraverser.h"
#include "Aql/AqlValue.h"

namespace arangodb {
namespace traverser {

class SingleServerTraversalPath final : public TraversalPath {
 public:
  SingleServerTraversalPath(
      arangodb::basics::EnumeratedPath<std::string, std::string> const& path,
      SingleServerTraverser* traverser)
      : _traverser(traverser), _path(path) {}

  ~SingleServerTraversalPath() {}

  void pathToVelocyPack(arangodb::Transaction*,
                        arangodb::velocypack::Builder&) override;

  void lastEdgeToVelocyPack(arangodb::Transaction*,
                            arangodb::velocypack::Builder&) override;

  aql::AqlValue lastVertexToAqlValue(arangodb::Transaction*) override;

 private:

  SingleServerTraverser* _traverser;

  arangodb::basics::EnumeratedPath<std::string, std::string> const _path;
};

} // namespace traverser
} // namespace arangodb

#endif
