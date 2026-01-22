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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#include <unordered_set>
#include "Graph/Cursors/DBServerEdgeCursor.h"
#include "Graph/Cursors/EdgeCursor.h"
#include "Graph/EdgeDocumentToken.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "gtest/gtest.h"

using namespace arangodb;
using namespace arangodb::graph;

namespace {
struct MockIndexCursor {
  void all(EdgeCursor::Callback const& callback) {
    for (auto const& id : _ids[_currentVertex]) {
      callback(EdgeDocumentToken{DataSourceId{1}, id}, velocypack::Slice{}, 0);
    }
  }
  bool next(EdgeCursor::Callback const& callback) {
    return nextBatch(callback, 1) == 1;
  }
  uint64_t nextBatch(EdgeCursor::Callback const& callback, uint64_t batchSize) {
    uint64_t count = 0;
    while (count < batchSize) {
      if (_nextPosition >= _ids[_currentVertex].size()) {
        return count;
      }
      callback(EdgeDocumentToken{DataSourceId{1},
                                 _ids[_currentVertex][_nextPosition]},
               velocypack::Slice{}, 0);
      _nextPosition++;
      count++;
    }
    return count;
  }

  void rearm(std::string_view vertex) {
    _currentVertex = vertex;
    _nextPosition = 0;
  }
  bool hasMore() const {
    auto currentIds = _ids.find(_currentVertex);
    if (currentIds == _ids.end()) {
      return false;
    }
    return _nextPosition < currentIds->second.size();
  }

  std::unordered_map<std::string, std::vector<LocalDocumentId>> _ids;
  size_t _nextPosition = 0;
  std::string _currentVertex = "";
};
}  // namespace

TEST(DBServerEdgeCursorTest, empty_cursor_does_not_do_anything) {
  auto cursor = DBServerEdgeCursor<MockIndexCursor>({});
  EXPECT_FALSE(cursor.hasMore());

  cursor.rearm("v/0", 0);
  EXPECT_FALSE(cursor.hasMore());

  size_t counter = 0;
  cursor.readAll(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        counter++;
        return;
      });
  EXPECT_EQ(counter, 0);
  EXPECT_FALSE(cursor.hasMore());

  cursor.rearm("v/0", 0);
  EXPECT_FALSE(cursor.hasMore());

  counter = 0;
  EXPECT_FALSE(cursor.next(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        counter++;
        return;
      }));
  EXPECT_EQ(counter, 0);
  EXPECT_FALSE(cursor.hasMore());
}

TEST(DBServerEdgeCursorTest, reads_all_edges_of_rearmed_vertex) {
  auto cursor = DBServerEdgeCursor<MockIndexCursor>({MockIndexCursor{
      {{"v/1", {LocalDocumentId{13}}},
       {"v/0",
        {LocalDocumentId{1}, LocalDocumentId{5}, LocalDocumentId{3}}}}}});
  EXPECT_FALSE(cursor.hasMore());

  cursor.rearm("v/0", 0);
  EXPECT_TRUE(cursor.hasMore());

  std::unordered_multiset<LocalDocumentId> documents;
  cursor.readAll(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        return;
      });
  EXPECT_EQ(documents,
            (std::unordered_multiset<LocalDocumentId>{
                LocalDocumentId{1}, LocalDocumentId{5}, LocalDocumentId{3}}));
  EXPECT_FALSE(cursor.hasMore());

  cursor.rearm("v/1", 0);
  EXPECT_TRUE(cursor.hasMore());

  documents = {};
  cursor.readAll(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        return;
      });
  EXPECT_EQ(documents,
            (std::unordered_multiset<LocalDocumentId>{LocalDocumentId{13}}));
  EXPECT_FALSE(cursor.hasMore());
}

TEST(DBServerEdgeCursorTest, reads_next_edges_of_rearmed_vertex) {
  auto cursor = DBServerEdgeCursor<MockIndexCursor>({MockIndexCursor{
      {{"v/1", {LocalDocumentId{13}}},
       {"v/0",
        {LocalDocumentId{1}, LocalDocumentId{5}, LocalDocumentId{3}}}}}});
  EXPECT_FALSE(cursor.hasMore());

  cursor.rearm("v/0", 0);
  EXPECT_TRUE(cursor.hasMore());

  std::unordered_multiset<LocalDocumentId> documents;
  size_t counter = 0;
  EXPECT_TRUE(cursor.next(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        counter++;
        return;
      }));
  EXPECT_EQ(counter, 1);
  EXPECT_TRUE(cursor.hasMore());

  EXPECT_TRUE(cursor.next(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        counter++;
        return;
      }));
  EXPECT_EQ(counter, 2);
  EXPECT_TRUE(cursor.hasMore());

  EXPECT_TRUE(cursor.next(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        counter++;
        return;
      }));
  EXPECT_EQ(counter, 3);
  EXPECT_FALSE(cursor.hasMore());

  EXPECT_FALSE(cursor.next(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        counter++;
        return;
      }));
  EXPECT_EQ(counter, 3);
  EXPECT_EQ(documents,
            (std::unordered_multiset<LocalDocumentId>{
                LocalDocumentId{1}, LocalDocumentId{5}, LocalDocumentId{3}}));
  EXPECT_FALSE(cursor.hasMore());

  cursor.rearm("v/1", 0);
  EXPECT_TRUE(cursor.hasMore());

  documents = {};
  counter = 0;
  EXPECT_TRUE(cursor.next(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        counter++;
        return;
      }));
  EXPECT_EQ(counter, 1);
  EXPECT_FALSE(cursor.hasMore());

  EXPECT_FALSE(cursor.next(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        counter++;
        return;
      }));
  EXPECT_EQ(counter, 1);
  EXPECT_EQ(documents,
            (std::unordered_multiset<LocalDocumentId>{LocalDocumentId{13}}));
  EXPECT_FALSE(cursor.hasMore());
}

TEST(DBServerEdgeCursorTest, reads_next_batch_of_edges_of_rearmed_vertex) {
  auto cursor = DBServerEdgeCursor<MockIndexCursor>({MockIndexCursor{
      {{"v/1", {LocalDocumentId{13}}},
       {"v/0",
        {LocalDocumentId{1}, LocalDocumentId{5}, LocalDocumentId{3}}}}}});

  cursor.rearm("v/0", 0); /* rearm to vertex 0 - depth is ignored */

  std::unordered_multiset<LocalDocumentId> documents;
  size_t counter = 0;
  EXPECT_TRUE(cursor.nextBatch(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        counter++;
        return;
      },
      2));
  EXPECT_EQ(counter, 2);

  EXPECT_TRUE(cursor.nextBatch(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        counter++;
        return;
      },
      0));  // batch size 0
  EXPECT_EQ(counter, 2);

  EXPECT_FALSE(cursor.nextBatch(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        counter++;
        return;
      },
      2));
  EXPECT_EQ(counter, 3);
  EXPECT_EQ(documents,
            (std::unordered_multiset<LocalDocumentId>{
                LocalDocumentId{1}, LocalDocumentId{5}, LocalDocumentId{3}}));

  cursor.rearm("v/1", 0); /* rearm to vertex 1 - depth is ignored */

  documents = {};
  counter = 0;
  EXPECT_FALSE(cursor.nextBatch(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        counter++;
        return;
      },
      2));
  EXPECT_EQ(counter, 1);
  EXPECT_EQ(documents,
            (std::unordered_multiset<LocalDocumentId>{LocalDocumentId{13}}));
}

TEST(DBServerEdgeCursorTest, combines_all_edges_of_all_internal_cursors) {
  auto cursor = DBServerEdgeCursor<MockIndexCursor>(
      {MockIndexCursor{{{"v/1", {LocalDocumentId{13}}},
                        {"v/0", {LocalDocumentId{1}, LocalDocumentId{5}}}}},
       MockIndexCursor{{{"v/0", {LocalDocumentId{1}}},
                        {"v/5", {LocalDocumentId{1}, LocalDocumentId{9}}}}},
       MockIndexCursor{{{"v/1", {LocalDocumentId{15}}},
                        {"v/0", {LocalDocumentId{2}}},
                        {
                            "v/7",
                            {LocalDocumentId{4}, LocalDocumentId{5}},
                        }}}});

  cursor.rearm("v/0", 0);

  std::unordered_multiset<LocalDocumentId> documents;
  cursor.readAll(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        return;
      });
  EXPECT_EQ(documents, (std::unordered_multiset<LocalDocumentId>{
                           LocalDocumentId{1}, LocalDocumentId{5},
                           LocalDocumentId{1}, LocalDocumentId{2}}));

  cursor.rearm("v/1", 0);

  documents = {};
  cursor.readAll(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        return;
      });
  EXPECT_EQ(documents, (std::unordered_multiset<LocalDocumentId>{
                           LocalDocumentId{13}, LocalDocumentId{15}}));

  cursor.rearm("v/9", 0);

  documents = {};
  cursor.readAll(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        return;
      });
  EXPECT_EQ(documents, (std::unordered_multiset<LocalDocumentId>{}));
}

TEST(DBServerEdgeCursorTest, combines_next_edges_of_all_internal_cursors) {
  auto cursor = DBServerEdgeCursor<MockIndexCursor>(
      {MockIndexCursor{{{"v/1", {LocalDocumentId{13}}},
                        {"v/0", {LocalDocumentId{1}, LocalDocumentId{5}}}}},
       MockIndexCursor{{{"v/0", {LocalDocumentId{1}}},
                        {"v/5", {LocalDocumentId{1}, LocalDocumentId{9}}}}},
       MockIndexCursor{{{"v/1", {LocalDocumentId{15}}},
                        {"v/0", {LocalDocumentId{2}}},
                        {
                            "v/7",
                            {LocalDocumentId{4}, LocalDocumentId{5}},
                        }}}});

  cursor.rearm("v/0", 0);

  std::unordered_multiset<LocalDocumentId> documents;
  size_t counter = 0;
  EXPECT_TRUE(cursor.next(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        counter++;
        return;
      }));
  EXPECT_EQ(counter, 1);

  EXPECT_TRUE(cursor.next(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        counter++;
        return;
      }));
  EXPECT_EQ(counter, 2);

  EXPECT_TRUE(cursor.next(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        counter++;
        return;
      }));
  EXPECT_EQ(counter, 3);

  EXPECT_TRUE(cursor.next(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        counter++;
        return;
      }));
  EXPECT_EQ(counter, 4);

  EXPECT_FALSE(cursor.next(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        counter++;
        return;
      }));
  EXPECT_EQ(counter, 4);
  EXPECT_EQ(documents, (std::unordered_multiset<LocalDocumentId>{
                           LocalDocumentId{1}, LocalDocumentId{5},
                           LocalDocumentId{1}, LocalDocumentId{2}}));

  cursor.rearm("v/1", 0);

  documents = {};
  counter = 0;
  EXPECT_TRUE(cursor.next(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        counter++;
        return;
      }));
  EXPECT_EQ(counter, 1);

  EXPECT_TRUE(cursor.next(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        counter++;
        return;
      }));
  EXPECT_EQ(counter, 2);

  EXPECT_FALSE(cursor.next(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        counter++;
        return;
      }));
  EXPECT_EQ(counter, 2);
  EXPECT_EQ(documents, (std::unordered_multiset<LocalDocumentId>{
                           LocalDocumentId{13}, LocalDocumentId{15}}));

  cursor.rearm("v/9", 0);

  documents = {};
  counter = 0;
  EXPECT_FALSE(cursor.next(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        counter++;
        return;
      }));
  EXPECT_EQ(counter, 0);
  EXPECT_EQ(documents, (std::unordered_multiset<LocalDocumentId>{}));
}

TEST(DBServerEdgeCursorTest,
     combines_next_batch_of_edges_of_all_internal_cursors) {
  auto cursor = DBServerEdgeCursor<MockIndexCursor>(
      {MockIndexCursor{{{"v/1", {LocalDocumentId{13}}},
                        {"v/0", {LocalDocumentId{1}, LocalDocumentId{5}}}}},
       MockIndexCursor{{{"v/0", {LocalDocumentId{1}}},
                        {"v/5", {LocalDocumentId{1}, LocalDocumentId{9}}}}},
       {MockIndexCursor{{
           {"v/1", {LocalDocumentId{15}}},
           {"v/0", {LocalDocumentId{2}}},
           {"v/7", {LocalDocumentId{4}, LocalDocumentId{5}}},
       }}}});

  cursor.rearm("v/0", 0); /* rearm to vertex 0 - depth is ignored */

  std::unordered_multiset<LocalDocumentId> documents;
  size_t counter = 0;
  EXPECT_TRUE(cursor.nextBatch(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        counter++;
        return;
      },
      2));
  EXPECT_EQ(counter, 2);

  EXPECT_TRUE(cursor.nextBatch(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        counter++;
        return;
      },
      1));
  EXPECT_EQ(counter, 3);

  EXPECT_FALSE(cursor.nextBatch(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        counter++;
        return;
      },
      3));
  EXPECT_EQ(counter, 4);
  EXPECT_EQ(documents, (std::unordered_multiset<LocalDocumentId>{
                           LocalDocumentId{1}, LocalDocumentId{5},
                           LocalDocumentId{1}, LocalDocumentId{2}}));

  cursor.rearm("v/1", 0); /* rearm to vertex 1 - depth is ignored */

  documents = {};
  counter = 0;
  EXPECT_TRUE(cursor.nextBatch(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        counter++;
        return;
      },
      2));
  EXPECT_EQ(counter, 2);

  EXPECT_FALSE(cursor.nextBatch(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        counter++;
        return;
      },
      2));
  EXPECT_EQ(counter, 2);
  EXPECT_EQ(documents, (std::unordered_multiset<LocalDocumentId>{
                           LocalDocumentId{13}, LocalDocumentId{15}}));

  cursor.rearm("v/9", 0); /* rearm to vertex 9 - depth is ignored */

  documents = {};
  counter = 0;
  EXPECT_FALSE(cursor.nextBatch(
      [&](EdgeDocumentToken&& eid, velocypack::Slice edge, size_t cursorId) {
        documents.emplace(eid.localDocumentId());
        counter++;
        return;
      },
      2));
  EXPECT_EQ(counter, 0);
  EXPECT_EQ(documents, (std::unordered_multiset<LocalDocumentId>{}));
}
