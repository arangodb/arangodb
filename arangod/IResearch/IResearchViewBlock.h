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
#include "Indexes/IndexIterator.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"

#include "utils/attributes.hpp"
#include "IResearch/ExpressionFilter.h"
#include "IResearch/IResearchExpressionContext.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchDocument.h"

namespace iresearch {
class score;
} // iresearch

namespace arangodb {
namespace iresearch {

class IResearchViewNode;

///////////////////////////////////////////////////////////////////////////////
/// @class IResearchViewBlockBase
///////////////////////////////////////////////////////////////////////////////
class IResearchViewBlockBase : public aql::ExecutionBlock {
 public:
  IResearchViewBlockBase(
    irs::index_reader const& reader,
    aql::ExecutionEngine&,
    IResearchViewNode const&
  );

  std::pair<aql::ExecutionState, std::unique_ptr<aql::AqlItemBlock>> getSome(size_t atMost) override final;

  // skip between atLeast and atMost returns the number actually skipped . . .
  std::pair<aql::ExecutionState, size_t> skipSome(size_t atMost) override final;

  // here we release our docs from this collection
  virtual std::pair<aql::ExecutionState, Result> initializeCursor(aql::AqlItemBlock* items, size_t pos) override;

 protected:
  class ReadContext {
   private:
    static IndexIterator::DocumentCallback copyDocumentCallback(ReadContext& ctx);

   public:
    explicit ReadContext(aql::RegisterId curRegs)
      : curRegs(curRegs),
        callback(copyDocumentCallback(*this)) {
    }

    aql::RegisterId const curRegs;
    size_t pos{};
    std::unique_ptr<aql::AqlItemBlock> res;
    IndexIterator::DocumentCallback const callback;
  }; // ReadContext

  bool readDocument(
    irs::doc_id_t docId,
    irs::columnstore_reader::values_reader_f const& pkValues,
    IndexIterator::DocumentCallback const& callback
  );

  bool readDocument(
    DocumentPrimaryKey::type const& key,
    IndexIterator::DocumentCallback const& callback
  );

  virtual void reset();

  virtual bool next(
    ReadContext& ctx,
    size_t limit
  ) = 0;

  virtual size_t skip(size_t count) = 0;

  std::vector<DocumentPrimaryKey::type> _keys; // buffer for primary keys
  irs::attribute_view _filterCtx; // filter context
  ViewExpressionContext _ctx;
  irs::index_reader const& _reader;
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
    irs::index_reader const& reader,
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

  // resets _itr and _pkReader
  virtual bool resetIterator();

  irs::columnstore_reader::values_reader_f _pkReader; // current primary key reader
  irs::doc_iterator::ptr _itr;
  size_t _readerOffset;
}; // IResearchViewUnorderedBlock

///////////////////////////////////////////////////////////////////////////////
/// @class IResearchViewBlock
///////////////////////////////////////////////////////////////////////////////
class IResearchViewBlock final : public IResearchViewUnorderedBlock {
 public:
  IResearchViewBlock(
    irs::index_reader const& reader,
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
  virtual bool resetIterator() override;

  irs::score const* _scr;
  irs::bytes_ref _scrVal;
}; // IResearchViewBlock

} // iresearch
} // arangodb

#endif // ARANGOD_IRESEARCH__ENUMERATE_VIEW_BLOCK_H 
