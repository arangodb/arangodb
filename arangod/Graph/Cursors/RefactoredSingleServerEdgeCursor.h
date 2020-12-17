////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GRAPH_CURSORS_REFACTOREDSINGLESERVEREDGECURSOR_H
#define ARANGOD_GRAPH_CURSORS_REFACTOREDSINGLESERVEREDGECURSOR_H 1

#include "Aql/Expression.h"
#include "Aql/QueryContext.h"
#include "Indexes/IndexIterator.h"
#include "Transaction/Methods.h"

#include "Graph/Providers/TypeAliases.h"

#include <vector>

namespace arangodb {

class LocalDocumentId;

namespace velocypack {
class Builder;
class HashedStringRef;
}  // namespace velocypack

namespace graph {
struct IndexAccessor;

/*

// brauche ich als standalone input
transaction::Methods* _trx; // Liefert das Query (geteilt)



// Packet:
Variable, liste<IndexAccessor>

Variable* _variable; // "" (geteilt)
struct IndexAccessor const { //definition
  IndexHande const& _index; // Muss das query vorher ausrechnen, der Provider muss es bekommen (pro collection)
  AstNode* _searchCondition; // "" (pro index) _from == X  || _to == X
}
// keine ahnung von Rearm, baut den ersten Cursor?

// Action.
=> IndexCursor := _trx->indexScanForCondition()
=> rearm
*/
struct EdgeDocumentToken;

class RefactoredSingleServerEdgeCursor {
 public:
  struct LookupInfo {
    LookupInfo(transaction::Methods::IndexHandle idx, aql::AstNode* condition,
               std::optional<size_t> memberToUpdate);
    ~LookupInfo();

    LookupInfo(LookupInfo const&) = delete;
    LookupInfo(LookupInfo&&) noexcept;
    LookupInfo& operator=(LookupInfo const&) = delete;

    void rearmVertex(VertexType vertex, transaction::Methods* trx,
                     arangodb::aql::Variable const* tmpVar);

    IndexIterator& cursor();

   private:
    // This struct does only take responsibility for the expression
    // NOTE: The expression can be nullptr!
    transaction::Methods::IndexHandle _idxHandle;
    std::unique_ptr<aql::Expression> _expression;
    aql::AstNode* _indexCondition;

    std::unique_ptr<IndexIterator> _cursor;

    // Position of _from / _to in the index search condition
    std::optional<size_t> _conditionMemberToUpdate;
  };

  enum Direction { FORWARD, BACKWARD };

 public:
  RefactoredSingleServerEdgeCursor(arangodb::transaction::Methods* trx,
                                   arangodb::aql::QueryContext* queryContext,
                                   arangodb::aql::Variable const* tmpVar,
                                   std::vector<IndexAccessor> const& indexConditions);
  ~RefactoredSingleServerEdgeCursor();

  using Callback =
      std::function<void(EdgeDocumentToken&&, arangodb::velocypack::Slice, size_t)>;

 private:
  aql::Variable const* _tmpVar;
  size_t _currentCursor;
  std::vector<LocalDocumentId> _cache;
  size_t _cachePos;
  std::vector<size_t> const* _internalCursorMapping;
  std::vector<LookupInfo> _lookupInfo;

 protected:
  arangodb::transaction::Methods* _trx;
  arangodb::aql::QueryContext* _queryContext;

 public:
#if 0
  bool next(Callback const& callback);
#endif
  void readAll(Callback const& callback);

  void rearm(VertexType vertex, uint64_t depth);

 private:
#if 0
  // returns false if cursor can not be further advanced
  bool advanceCursor(IndexIterator& cursor);
#endif
  void getDocAndRunCallback(IndexIterator*, Callback const& callback);

  [[nodiscard]] transaction::Methods* trx() const;
};
}  // namespace graph
}  // namespace arangodb

#endif
