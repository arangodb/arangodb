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

namespace aql {
class TraversalStats;
}

namespace velocypack {
class Builder;
class HashedStringRef;
}  // namespace velocypack

namespace graph {
struct IndexAccessor;

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
                                   arangodb::aql::Variable const* tmpVar,
                                   std::vector<IndexAccessor> const& indexConditions);
  ~RefactoredSingleServerEdgeCursor();

  using Callback =
      std::function<void(EdgeDocumentToken&&, arangodb::velocypack::Slice, size_t)>;

 private:
  aql::Variable const* _tmpVar;
  size_t _currentCursor;
  std::vector<LookupInfo> _lookupInfo;

  arangodb::transaction::Methods* _trx;

 public:
  void readAll(aql::TraversalStats& stats, Callback const& callback);

  void rearm(VertexType vertex, uint64_t depth);

 private:
  [[nodiscard]] transaction::Methods* trx() const;
};
}  // namespace graph
}  // namespace arangodb

#endif
