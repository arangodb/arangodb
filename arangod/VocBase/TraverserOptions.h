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

#ifndef ARANGOD_VOC_BASE_TRAVERSER_OPTIONS_H
#define ARANGOD_VOC_BASE_TRAVERSER_OPTIONS_H 1

#include "Basics/Common.h"
#include "Aql/FixedVarExpressionContext.h"
#include "Utils/Transaction.h"

namespace arangodb {

namespace velocypack {
class Builder;
class Slice;
}

namespace aql {
struct AstNode;
class Expression;
class Query;
class TraversalNode;
}

namespace traverser {

class ClusterTraverser;

/// @brief Abstract class used in the traversals
/// to abstract away access to indexes / DBServers.
/// Returns edges as VelocyPack.

class EdgeCursor {
 public:
  EdgeCursor() {}
  virtual ~EdgeCursor() {}

  virtual bool next(std::vector<arangodb::velocypack::Slice>&, size_t&) = 0;
  virtual bool readAll(std::unordered_set<arangodb::velocypack::Slice>&,
                       size_t&) = 0;
};


struct TraverserOptions {
  friend class arangodb::aql::TraversalNode;

 public:
  enum UniquenessLevel { NONE, PATH, GLOBAL };

 protected:

  struct LookupInfo {
    // This struct does only take responsibility for the expression
    // NOTE: The expression can be nullptr!
    std::vector<arangodb::Transaction::IndexHandle> idxHandles;
    aql::Expression* expression;
    aql::AstNode* indexCondition;
    // Flag if we have to update _from / _to in the index search condition
    bool conditionNeedUpdate;
    // Position of _from / _to in the index search condition
    size_t conditionMemberToUpdate;

    LookupInfo();
    ~LookupInfo();

    LookupInfo(LookupInfo const&);

    LookupInfo(arangodb::aql::Query*, arangodb::velocypack::Slice const&,
               arangodb::velocypack::Slice const&);

    /// @brief Build a velocypack containing all relevant information
    ///        for DBServer traverser engines.
    void buildEngineInfo(arangodb::velocypack::Builder&) const;
  };

 protected:
  arangodb::Transaction* _trx;
  std::vector<LookupInfo> _baseLookupInfos;
  std::unordered_map<size_t, std::vector<LookupInfo>> _depthLookupInfo;
  std::unordered_map<size_t, aql::Expression*> _vertexExpressions;
  aql::Expression* _baseVertexExpression;
  aql::Variable const* _tmpVar;
  aql::FixedVarExpressionContext* _ctx;
  arangodb::traverser::ClusterTraverser* _traverser;

 public:
  uint64_t minDepth;

  uint64_t maxDepth;

  bool useBreadthFirst;

  UniquenessLevel uniqueVertices;

  UniquenessLevel uniqueEdges;

  explicit TraverserOptions(arangodb::Transaction* trx)
      : _trx(trx),
        _baseVertexExpression(nullptr),
        _tmpVar(nullptr),
        _ctx(new aql::FixedVarExpressionContext()),
        _traverser(nullptr),
        minDepth(1),
        maxDepth(1),
        useBreadthFirst(false),
        uniqueVertices(UniquenessLevel::NONE),
        uniqueEdges(UniquenessLevel::PATH) {}

  TraverserOptions(arangodb::Transaction*, arangodb::velocypack::Slice const&);

  TraverserOptions(arangodb::aql::Query*, arangodb::velocypack::Slice,
                   arangodb::velocypack::Slice);

  /// @brief This copy constructor is only working during planning phase.
  ///        After planning this node should not be copied anywhere.
  TraverserOptions(TraverserOptions const&);

  virtual ~TraverserOptions();

  /// @brief Build a velocypack for cloning in the plan.
  void toVelocyPack(arangodb::velocypack::Builder&) const;

  /// @brief Build a velocypack containing all relevant information
  ///        for DBServer traverser engines.
  void buildEngineInfo(arangodb::velocypack::Builder&) const;

  bool vertexHasFilter(size_t) const;

  bool evaluateEdgeExpression(arangodb::velocypack::Slice,
                              arangodb::velocypack::Slice, size_t,
                              size_t) const;

  bool evaluateVertexExpression(arangodb::velocypack::Slice, size_t) const;

  EdgeCursor* nextCursor(arangodb::velocypack::Slice, size_t) const;

  void clearVariableValues();

  void setVariableValue(aql::Variable const*, aql::AqlValue const);

  void linkTraverser(arangodb::traverser::ClusterTraverser*);

 private:
  EdgeCursor* nextCursorLocal(arangodb::velocypack::Slice, size_t,
                              std::vector<LookupInfo>&) const;

  EdgeCursor* nextCursorCoordinator(arangodb::velocypack::Slice, size_t) const;
};

}
}
#endif
