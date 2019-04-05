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
#include <velocypack/StringRef.h>

namespace arangodb {

class LocalDocumentId;
struct OperationCursor;
class LogicalCollection;

namespace transaction {
class Methods;
}

namespace velocypack {
class Slice;
}

namespace graph {
struct BaseOptions;
struct SingleServerEdgeDocumentToken;

class SingleServerEdgeCursor final : public EdgeCursor {
 private:
  BaseOptions* _opts;
  transaction::Methods* _trx;
  std::vector<std::vector<OperationCursor*>> _cursors;
  size_t _currentCursor;
  size_t _currentSubCursor;
  std::vector<LocalDocumentId> _cache;
  size_t _cachePos;
  std::vector<size_t> const* _internalCursorMapping;

 public:
  SingleServerEdgeCursor(BaseOptions* options, size_t,
                         std::vector<size_t> const* mapping = nullptr);

  ~SingleServerEdgeCursor();

  bool next(EdgeCursor::Callback const& callback) override;

  void readAll(EdgeCursor::Callback const& callback) override;

  std::vector<std::vector<OperationCursor*>>& getCursors() { return _cursors; }
  
  /// @brief number of HTTP requests performed. always 0 in single server
  size_t httpRequests() const override { return 0; }

 private:
  // returns false if cursor can not be further advanced
  bool advanceCursor(OperationCursor*& cursor, std::vector<OperationCursor*>& cursorSet);

  void getDocAndRunCallback(OperationCursor*, EdgeCursor::Callback const& callback);
};
}  // namespace graph
}  // namespace arangodb

#endif
