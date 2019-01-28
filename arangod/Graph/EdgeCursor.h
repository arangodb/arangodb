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

#ifndef ARANGOD_GRAPH_EDGECURSOR_H
#define ARANGOD_GRAPH_EDGECURSOR_H 1

#include "Basics/Common.h"

namespace arangodb {

class StringRef;

namespace velocypack {
class Slice;
}

namespace graph {

struct EdgeDocumentToken;

/// @brief Abstract class used in the traversals
/// to abstract away access to indexes / DBServers.
/// Returns edges as VelocyPack.

class EdgeCursor {
 public:
  EdgeCursor() {}
  virtual ~EdgeCursor() {}

  virtual bool next(std::function<void(EdgeDocumentToken&&, arangodb::velocypack::Slice, size_t)> callback) = 0;

  virtual void readAll(std::function<void(EdgeDocumentToken&&, arangodb::velocypack::Slice, size_t)>) = 0;
};

}  // namespace graph
}  // namespace arangodb

#endif
