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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__ENUMERATE_VIEW_BLOCK_H
#define ARANGOD_IRESEARCH__ENUMERATE_VIEW_BLOCK_H 1

#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExpressionContext.h"
#include "Views/ViewIterator.h"
#include "VocBase/LogicalView.h"

namespace arangodb {

namespace aql {
class AqlItemBlock;
class ExecutionEngine;
} // aql

namespace iresearch {

class IResearchViewNode;

///////////////////////////////////////////////////////////////////////////////
/// @class ViewExpressionContext
///////////////////////////////////////////////////////////////////////////////
class ViewExpressionContext final : public aql::ExpressionContext {
 public:
  explicit ViewExpressionContext(aql::ExecutionBlock* block)
    : _block(block) {
    TRI_ASSERT(_block);
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
  aql::ExecutionBlock* _block;
  size_t _pos{};
}; // ViewExpressionContext

///////////////////////////////////////////////////////////////////////////////
/// @class EnumerateViewBlock
///////////////////////////////////////////////////////////////////////////////
class IResearchViewBlock final : public aql::ExecutionBlock {
 public:
  IResearchViewBlock(aql::ExecutionEngine*, IResearchViewNode const*);

  // here we release our docs from this collection
  int initializeCursor(aql::AqlItemBlock* items, size_t pos) override;

  aql::AqlItemBlock* getSome(size_t atLeast, size_t atMost) override final;

  // skip between atLeast and atMost returns the number actually skipped . . .
  // will only return less than atLeast if there aren't atLeast many
  // things to skip overall.
  size_t skipSome(size_t atLeast, size_t atMost) override final;

 private:
  void refreshIterator();

  ViewExpressionContext _ctx;
  std::unique_ptr<ViewIterator> _iter;
  ManagedDocumentResult _mmdr;
  bool _hasMore;
  bool _volatileState; // we have to recreate cached iterator when `reset` requested
}; // EnumerateViewBlock

} // iresearch
} // arangodb

#endif // ARANGOD_IRESEARCH__ENUMERATE_VIEW_BLOCK_H 
