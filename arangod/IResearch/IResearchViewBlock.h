////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__IRESEARCH_VIEW_BLOCK_H
#define ARANGOD_IRESEARCH__IRESEARCH_VIEW_BLOCK_H 1

#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/QueryExpressionContext.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"

#include "utils/attributes.hpp"
#include "IResearch/ExpressionFilter.h"
#include "IResearch/IResearchView.h"

NS_BEGIN(iresearch)
class score;
NS_END // iresearch

NS_BEGIN(arangodb)
NS_BEGIN(aql)
class AqlItemBlock;
class ExecutionEngine;
NS_END // aql

NS_BEGIN(iresearch)

class IResearchViewNode;

///////////////////////////////////////////////////////////////////////////////
/// @class ViewExpressionContext
///////////////////////////////////////////////////////////////////////////////
class ViewExpressionContext final : public aql::QueryExpressionContext {
 public:
  explicit ViewExpressionContext(arangodb::aql::Query* query, IResearchViewNode const& node)
    : QueryExpressionContext(query),
      _node(&node) {
    TRI_ASSERT(_node);
  }

  virtual size_t numRegisters() const override;

  virtual aql::AqlValue const& getRegisterValue(size_t i) const override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual aql::Variable const* getVariable(size_t i) const override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual aql::AqlValue getVariableValue(
    aql::Variable const* variable,
    bool doCopy,
    bool& mustDestroy
  ) const override;

  aql::AqlItemBlock const* _data{};
  IResearchViewNode const* _node;
  size_t _pos{};
}; // ViewExpressionContext

///////////////////////////////////////////////////////////////////////////////
/// @class IResearchViewBlockBase
///////////////////////////////////////////////////////////////////////////////
class IResearchViewBlockBase : public aql::ExecutionBlock {
 public:
  IResearchViewBlockBase(
    arangodb::iresearch::PrimaryKeyIndexReader const& reader,
    aql::ExecutionEngine&,
    IResearchViewNode const&
  );

  std::pair<aql::ExecutionState, std::unique_ptr<aql::AqlItemBlock>> getSome(size_t atMost) override final;

  // skip between atLeast and atMost returns the number actually skipped . . .
  std::pair<aql::ExecutionState, size_t> skipSome(size_t atMost) override final;

  // here we release our docs from this collection
  virtual std::pair<aql::ExecutionState, Result> initializeCursor(aql::AqlItemBlock* items, size_t pos) override;

 protected:
  struct ReadContext {
    explicit ReadContext(aql::RegisterId curRegs)
      : curRegs(curRegs) {
    }

    std::unique_ptr<aql::AqlItemBlock> res;
    size_t pos{};
    const aql::RegisterId curRegs;
  }; // ReadContext

  bool readDocument(
    size_t segmentId,
    irs::doc_id_t docId,
    IndexIterator::DocumentCallback const& callback
  );

  virtual void reset();

  virtual bool next(
    ReadContext& ctx,
    size_t limit
  ) = 0;

  virtual size_t skip(size_t count) = 0;

  irs::attribute_view _filterCtx; // filter context
  ViewExpressionContext _ctx;
  iresearch::PrimaryKeyIndexReader const& _reader;
  irs::filter::prepared::ptr _filter;
  irs::order::prepared _order;
  iresearch::ExpressionExecutionContext _execCtx; // expression execution context
  size_t _inflight; // The number of documents inflight if we hit a WAITING state.
  bool _hasMore;
  bool _volatileSort;
  bool _volatileFilter;
}; // IResearchViewBlockBase

///////////////////////////////////////////////////////////////////////////////
/// @class IResearchViewUnorderedBlock
///////////////////////////////////////////////////////////////////////////////
class IResearchViewUnorderedBlock : public IResearchViewBlockBase {
 public:
  IResearchViewUnorderedBlock(
    PrimaryKeyIndexReader const& reader,
    aql::ExecutionEngine& engine,
    IResearchViewNode const& node
  );

 protected:
  virtual void reset() override final {
    IResearchViewBlockBase::reset();

    // reset iterator state
    _itr.reset();
    _readerOffset = 0;
  }

  virtual bool next(
    ReadContext& ctx,
    size_t limit
  ) override;

  virtual size_t skip(size_t count) override;

  irs::doc_iterator::ptr _itr;
  size_t _readerOffset;
}; // IResearchViewUnorderedBlock

///////////////////////////////////////////////////////////////////////////////
/// @class IResearchViewBlock
///////////////////////////////////////////////////////////////////////////////
class IResearchViewBlock final : public IResearchViewUnorderedBlock {
 public:
  IResearchViewBlock(
    PrimaryKeyIndexReader const& reader,
    aql::ExecutionEngine& engine,
    IResearchViewNode const& node
  );

 protected:
  virtual bool next(
    ReadContext& ctx,
    size_t limit
  ) override;

  virtual size_t skip(size_t count) override;

 private:
  void resetIterator();

  irs::score const* _scr;
  irs::bytes_ref _scrVal;
}; // IResearchViewBlock

///////////////////////////////////////////////////////////////////////////////
/// @class IResearchViewOrderedBlock
///////////////////////////////////////////////////////////////////////////////
class IResearchViewOrderedBlock final : public IResearchViewBlockBase {
 public:
  IResearchViewOrderedBlock(
    PrimaryKeyIndexReader const& reader,
    aql::ExecutionEngine& engine,
    IResearchViewNode const& node
  );

 protected:
  virtual void reset() override {
    IResearchViewBlockBase::reset();
    _skip = 0;
  }

  virtual bool next(
    ReadContext& ctx,
    size_t limit
  ) override;

  virtual size_t skip(size_t count) override;

 private:
  size_t _skip{};
}; // IResearchViewOrderedBlock

NS_END // iresearch
NS_END // arangodb

#endif // ARANGOD_IRESEARCH__ENUMERATE_VIEW_BLOCK_H 
