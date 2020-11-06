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

struct EdgeDocumentToken;

class RefactoredSingleServerEdgeCursor {
 public:
  struct LookupInfo {
    LookupInfo();
    ~LookupInfo();

    LookupInfo(LookupInfo const&);
    LookupInfo& operator=(LookupInfo const&) = delete;

    LookupInfo(arangodb::aql::QueryContext&, arangodb::velocypack::Slice const&,
               arangodb::velocypack::Slice const&);

    void rearmVertex(VertexType vertex);

    aql::AstNode const* indexCondition() const;

    std::vector<transaction::Methods::IndexHandle> const& indexHandles() const;

    /// @brief Build a velocypack containing all relevant information
    ///        for DBServer traverser engines.
    void buildEngineInfo(arangodb::velocypack::Builder&) const;

    double estimateCost(size_t& nrItems) const;

    void addLookupInfo(aql::ExecutionPlan* plan, std::string const& collectionName,
                       std::string const& attributeName,
                       aql::AstNode* condition, bool onlyEdgeIndexes = false);

   protected:
    void injectLookupInfoInList(std::vector<LookupInfo>&, aql::ExecutionPlan* plan,
                                std::string const& collectionName,
                                std::string const& attributeName,
                                aql::AstNode* condition, bool onlyEdgeIndexes = false);

   private:
    // This struct does only take responsibility for the expression
    // NOTE: The expression can be nullptr!
    std::vector<transaction::Methods::IndexHandle> _idxHandles;
    std::unique_ptr<aql::Expression> _expression;
    aql::AstNode* _indexCondition;
    // Flag if we have to update _from / _to in the index search condition
    bool _conditionNeedUpdate;
    // Position of _from / _to in the index search condition
    size_t _conditionMemberToUpdate;
  };

  enum Direction { FORWARD, BACKWARD };

 public:
  RefactoredSingleServerEdgeCursor(arangodb::transaction::Methods* trx,
                                   arangodb::aql::QueryContext* queryContext);
  ~RefactoredSingleServerEdgeCursor();

  using Callback =
      std::function<void(EdgeDocumentToken&&, arangodb::velocypack::Slice, size_t)>;

 private:
  aql::Variable const* _tmpVar;
  std::vector<std::vector<std::unique_ptr<IndexIterator>>> _cursors;
  size_t _currentCursor;
  size_t _currentSubCursor;
  std::vector<LocalDocumentId> _cache;
  size_t _cachePos;
  std::vector<size_t> const* _internalCursorMapping;
  std::vector<LookupInfo> _lookupInfo;

 protected:
  arangodb::transaction::Methods* _trx;
  arangodb::aql::QueryContext* _queryContext;

 public:
  bool next(Callback const& callback);

  void readAll(Callback const& callback);

  void rearm(VertexType vertex, uint64_t depth);

 private:
  // returns false if cursor can not be further advanced
  bool advanceCursor(IndexIterator*& cursor,
                     std::vector<std::unique_ptr<IndexIterator>>*& cursorSet);

  void getDocAndRunCallback(IndexIterator*, Callback const& callback);

  void buildLookupInfo(VertexType vertex);

  void addCursor(LookupInfo& info, VertexType vertex);

  [[nodiscard]] transaction::Methods* trx() const;  // TODO check nodiscard
};
}  // namespace graph
}  // namespace arangodb

#endif
