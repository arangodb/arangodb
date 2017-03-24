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
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"

namespace arangodb {
class ManagedDocumentResult;

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

struct BaseTraverserOptions {

 protected:

  struct LookupInfo {
    // This struct does only take responsibility for the expression
    // NOTE: The expression can be nullptr!
    std::vector<transaction::Methods::IndexHandle> idxHandles;
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

    double estimateCost(size_t& nrItems) const;
    
  };

 private:
  aql::FixedVarExpressionContext* _ctx;

 protected:
  transaction::Methods* _trx;
  std::vector<LookupInfo> _baseLookupInfos;
  aql::Variable const* _tmpVar;
  bool const _isCoordinator;

 public:
  explicit BaseTraverserOptions(transaction::Methods* trx)
      : _ctx(new aql::FixedVarExpressionContext()),
        _trx(trx),
        _tmpVar(nullptr),
        _isCoordinator(arangodb::ServerState::instance()->isCoordinator()) {}

  /// @brief This copy constructor is only working during planning phase.
  ///        After planning this node should not be copied anywhere.
  explicit BaseTraverserOptions(BaseTraverserOptions const&);

  BaseTraverserOptions(arangodb::aql::Query*, arangodb::velocypack::Slice,
                       arangodb::velocypack::Slice);

  virtual ~BaseTraverserOptions();

  // Creates a complete Object containing all EngineInfo
  // in the given builder.
  virtual void buildEngineInfo(arangodb::velocypack::Builder&) const;

  void setVariable(aql::Variable const*);

  void addLookupInfo(aql::Ast* ast, std::string const& collectionName,
                     std::string const& attributeName, aql::AstNode* condition);

  void clearVariableValues();

  void setVariableValue(aql::Variable const*, aql::AqlValue const);

  void serializeVariables(arangodb::velocypack::Builder&) const;

  transaction::Methods* trx() const;

  /// @brief Build a velocypack for cloning in the plan.
  virtual void toVelocyPack(arangodb::velocypack::Builder&) const = 0;

  // Creates a complete Object containing all index information
  // in the given builder.
  virtual void toVelocyPackIndexes(arangodb::velocypack::Builder&) const;

  /// @brief Estimate the total cost for this operation
  virtual double estimateCost(size_t& nrItems) const = 0;

 protected:

  double costForLookupInfoList(std::vector<LookupInfo> const& list,
                               size_t& createItems) const;

  // Requires an open Object in the given builder an
  // will inject index information into it.
  // Does not close the builder.
  void injectVelocyPackIndexes(arangodb::velocypack::Builder&) const;

  // Requires an open Object in the given builder an
  // will inject EngineInfo into it.
  // Does not close the builder.
  void injectEngineInfo(arangodb::velocypack::Builder&) const;

  aql::Expression* getEdgeExpression(size_t cursorId) const;

  bool evaluateExpression(aql::Expression*, arangodb::velocypack::Slice varValue) const;

  void injectLookupInfoInList(std::vector<LookupInfo>&, aql::Ast* ast,
                              std::string const& collectionName,
                              std::string const& attributeName,
                              aql::AstNode* condition);
};

struct TraverserOptions : public BaseTraverserOptions {
  friend class arangodb::aql::TraversalNode;

 public:
  enum UniquenessLevel { NONE, PATH, GLOBAL };

 protected:
  std::unordered_map<size_t, std::vector<LookupInfo>> _depthLookupInfo;
  std::unordered_map<size_t, aql::Expression*> _vertexExpressions;
  aql::Expression* _baseVertexExpression;
  arangodb::traverser::ClusterTraverser* _traverser;

 public:
  uint64_t minDepth;

  uint64_t maxDepth;

  bool useBreadthFirst;

  UniquenessLevel uniqueVertices;

  UniquenessLevel uniqueEdges;

  explicit TraverserOptions(transaction::Methods* trx)
      : BaseTraverserOptions(trx),
        _baseVertexExpression(nullptr),
        _traverser(nullptr),
        minDepth(1),
        maxDepth(1),
        useBreadthFirst(false),
        uniqueVertices(UniquenessLevel::NONE),
        uniqueEdges(UniquenessLevel::PATH) {}

  TraverserOptions(transaction::Methods*, arangodb::velocypack::Slice const&);

  TraverserOptions(arangodb::aql::Query*, arangodb::velocypack::Slice,
                   arangodb::velocypack::Slice);

  /// @brief This copy constructor is only working during planning phase.
  ///        After planning this node should not be copied anywhere.
  TraverserOptions(TraverserOptions const&);

  virtual ~TraverserOptions();

  /// @brief Build a velocypack for cloning in the plan.
  void toVelocyPack(arangodb::velocypack::Builder&) const override;
  
  /// @brief Build a velocypack for indexes
  void toVelocyPackIndexes(arangodb::velocypack::Builder&) const override;

  /// @brief Build a velocypack containing all relevant information
  ///        for DBServer traverser engines.
  void buildEngineInfo(arangodb::velocypack::Builder&) const override;

  bool vertexHasFilter(uint64_t) const;

  bool evaluateEdgeExpression(arangodb::velocypack::Slice,
                              arangodb::velocypack::Slice, uint64_t,
                              size_t) const;

  bool evaluateVertexExpression(arangodb::velocypack::Slice, uint64_t) const;

  EdgeCursor* nextCursor(ManagedDocumentResult*, arangodb::velocypack::Slice, uint64_t) const;

  void linkTraverser(arangodb::traverser::ClusterTraverser*);

  double estimateCost(size_t& nrItems) const override;

 private:

  EdgeCursor* nextCursorLocal(ManagedDocumentResult*,
                              arangodb::velocypack::Slice, uint64_t,
                              std::vector<LookupInfo>&) const;

  EdgeCursor* nextCursorCoordinator(arangodb::velocypack::Slice, uint64_t) const;
};

struct ShortestPathOptions : public BaseTraverserOptions {

 protected:

  double _defaultWeight;
  std::string _weightAttribute;
  std::vector<LookupInfo> _reverseLookupInfos;

 public:

  enum DIRECTION {FORWARD, BACKWARD};

  explicit ShortestPathOptions(transaction::Methods* trx)
    : BaseTraverserOptions(trx),
      _defaultWeight(1),
      _weightAttribute("") {}

  ShortestPathOptions(arangodb::aql::Query*, arangodb::velocypack::Slice info,
                      arangodb::velocypack::Slice collections,
                      arangodb::velocypack::Slice reverseCollections);

  void setWeightAttribute(std::string const& attr) {
    _weightAttribute = attr;
  }

  void setDefaultWeight(double weight) {
    _defaultWeight = weight;
  }

  bool usesWeight() const {
    return !_weightAttribute.empty();
  }

  std::string const weightAttribute() const {
    return _weightAttribute;
  }

  double defaultWeight() const {
    return _defaultWeight;
  }

  /// @brief Build a velocypack for cloning in the plan.
  void toVelocyPack(arangodb::velocypack::Builder&) const override;

  /// @brief Build a velocypack containing all relevant information
  ///        for DBServer traverser engines.
  void buildEngineInfo(arangodb::velocypack::Builder&) const override;

  double estimateCost(size_t& nrItems) const override;

  /// @brief Add a lookup info for reverse direction
  void addReverseLookupInfo(aql::Ast* ast, std::string const& collectionName,
                            std::string const& attributeName,
                            aql::AstNode* condition);

  template<enum DIRECTION>
  EdgeCursor* nextCursor(ManagedDocumentResult*, arangodb::velocypack::Slice) const;

 private:

  template<enum DIRECTION>
  EdgeCursor* nextCursorLocal(ManagedDocumentResult*,
                              arangodb::velocypack::Slice,
                              std::vector<LookupInfo>&) const;

};

}
}
#endif
