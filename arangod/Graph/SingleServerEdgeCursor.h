////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include <memory>
#include <vector>

#include "Basics/Common.h"
#include "BaseOptions.h"
#include "Graph/EdgeCursor.h"
#include "Indexes/IndexIterator.h"

#include <velocypack/StringRef.h>

namespace arangodb {

class LocalDocumentId;
class LogicalCollection;

namespace aql {
struct Variable;
}

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
  BaseOptions const* _opts;
  transaction::Methods* _trx;
  aql::Variable const* _tmpVar;
  // TODO: make this a flat vector
  std::vector<std::vector<std::unique_ptr<IndexIterator>>> _cursors;
  size_t _currentCursor;
  size_t _currentSubCursor;
  std::vector<LocalDocumentId> _cache;
  size_t _cachePos;
  std::vector<size_t> const* _internalCursorMapping;
  std::vector<BaseOptions::LookupInfo> const& _lookupInfo;

 public:
  explicit SingleServerEdgeCursor(BaseOptions* options, 
                                  aql::Variable const* tmpVar,
                                  std::vector<size_t> const* mapping,
                                  std::vector<BaseOptions::LookupInfo> const& lookupInfo);

  ~SingleServerEdgeCursor();

  bool next(EdgeCursor::Callback const& callback) override;

  void readAll(EdgeCursor::Callback const& callback) override;

  /// @brief number of HTTP requests performed. always 0 in single server
  size_t httpRequests() const override { return 0; }
  
  void rearm(arangodb::velocypack::StringRef vertex, uint64_t depth) override;
  
 private:
  // returns false if cursor can not be further advanced
  bool advanceCursor(IndexIterator*& cursor, std::vector<std::unique_ptr<IndexIterator>>*& cursorSet);

  void getDocAndRunCallback(IndexIterator*, EdgeCursor::Callback const& callback);

  void buildLookupInfo(arangodb::velocypack::StringRef vertex); 

  void addCursor(BaseOptions::LookupInfo const& info, arangodb::velocypack::StringRef vertex);
};

}  // namespace graph
}  // namespace arangodb

#endif
