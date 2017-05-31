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

#ifndef ARANGOD_GRAPH_SINGLE_SERVER_EDGE_CURSOR_H
#define ARANGOD_GRAPH_SINGLE_SERVER_EDGE_CURSOR_H 1

#include "Basics/Common.h"

#include "Graph/EdgeCursor.h"

namespace arangodb {

struct DocumentIdentifierToken;
class ManagedDocumentResult;
struct OperationCursor;
class StringRef;

namespace transaction {
class Methods;
}

namespace velocypack {
class Slice;
}

namespace graph {
struct BaseOptions;

class SingleServerEdgeCursor : public EdgeCursor {
 private:
  BaseOptions* _opts;
  transaction::Methods* _trx;
  ManagedDocumentResult* _mmdr;
  std::vector<std::vector<OperationCursor*>> _cursors;
  size_t _currentCursor;
  size_t _currentSubCursor;
  std::vector<DocumentIdentifierToken> _cache;
  size_t _cachePos;
  std::vector<size_t> const* _internalCursorMapping;

 public:
  SingleServerEdgeCursor(ManagedDocumentResult* mmdr, BaseOptions* options,
                         size_t, std::vector<size_t> const* mapping = nullptr);

  ~SingleServerEdgeCursor();

  bool next(std::function<void(std::unique_ptr<EdgeDocumentToken>&&,
                               arangodb::velocypack::Slice, size_t)>
                callback) override;

  void readAll(
      std::function<void(std::unique_ptr<EdgeDocumentToken>&&,
                         arangodb::velocypack::Slice, size_t)>) override;

  std::vector<std::vector<OperationCursor*>>& getCursors() { return _cursors; }
};

}  // namespace graph
}  // namespace arangodb

#endif
