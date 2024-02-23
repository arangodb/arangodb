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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "Basics/Common.h"
#include "BaseOptions.h"
#include "Graph/EdgeCursor.h"
#include "Indexes/IndexIterator.h"

namespace arangodb {
class LocalDocumentId;
class LogicalCollection;
struct ResourceMonitor;

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

class SingleServerEdgeCursor final : public EdgeCursor {
 private:
  struct CursorInfo {
    std::unique_ptr<IndexIterator> cursor;
    uint16_t coveringIndexPosition;

    CursorInfo(std::unique_ptr<IndexIterator> cursor,
               uint16_t coveringIndexPosition) noexcept
        : cursor(std::move(cursor)),
          coveringIndexPosition(coveringIndexPosition) {}

    CursorInfo(CursorInfo const&) = delete;
    CursorInfo& operator=(CursorInfo const&) = delete;
    CursorInfo(CursorInfo&&) noexcept = default;
    CursorInfo& operator=(CursorInfo&&) noexcept = default;
  };

  BaseOptions const* _opts;
  ResourceMonitor& _monitor;
  transaction::Methods* _trx;
  aql::Variable const* _tmpVar;
  std::vector<std::vector<CursorInfo>> _cursors;
  size_t _currentCursor;
  size_t _currentSubCursor;
  std::vector<LocalDocumentId> _cache;
  size_t _cachePos;
  std::vector<size_t> const* _internalCursorMapping;
  std::vector<BaseOptions::LookupInfo> const& _lookupInfo;

 public:
  explicit SingleServerEdgeCursor(
      BaseOptions* options, aql::Variable const* tmpVar,
      std::vector<size_t> const* mapping,
      std::vector<BaseOptions::LookupInfo> const& lookupInfo);

  ~SingleServerEdgeCursor();

  bool next(EdgeCursor::Callback const& callback) override;

  void readAll(EdgeCursor::Callback const& callback) override;

  /// @brief number of HTTP requests performed. always 0 in single server
  std::uint64_t httpRequests() const override { return 0; }

  void rearm(std::string_view vertex, uint64_t depth) override;

 private:
  // returns false if cursor can not be further advanced
  bool advanceCursor(IndexIterator*& cursor,
                     std::vector<CursorInfo>*& cursorSet);

  void getDocAndRunCallback(IndexIterator*,
                            EdgeCursor::Callback const& callback);

  void buildLookupInfo(std::string_view vertex);

  void addCursor(BaseOptions::LookupInfo const& info, std::string_view vertex);
};

}  // namespace graph
}  // namespace arangodb
