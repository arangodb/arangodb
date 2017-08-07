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
/// @author Jan Steemann
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_INDEX_BLOCK_H
#define ARANGOD_AQL_INDEX_BLOCK_H 1

#include "Aql/BlockCollector.h"
#include "Aql/DocumentProducingBlock.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/IndexNode.h"
#include "Indexes/IndexIterator.h"
#include "StorageEngine/DocumentIdentifierToken.h"

namespace arangodb {
class ManagedDocumentResult;
struct OperationCursor;

namespace velocypack {
class Slice;
}

namespace aql {

class AqlItemBlock;
struct AstNode;
struct Collection;
class ExecutionEngine;

/// @brief struct to hold the member-indexes in the _condition node
struct NonConstExpression {
  size_t const orMember;
  size_t const andMember;
  size_t const operatorMember;
  Expression* expression;

  NonConstExpression(size_t orM, size_t andM, size_t opM, Expression* exp)
      : orMember(orM), andMember(andM), operatorMember(opM), expression(exp) {}

  ~NonConstExpression() { delete expression; }
};

class IndexBlock final : public ExecutionBlock, public DocumentProducingBlock {
 public:
  IndexBlock(ExecutionEngine* engine, IndexNode const* ep);

  ~IndexBlock();

  /// @brief initialize, here we fetch all docs from the database
  int initialize() override;

  /// @brief initializeCursor, here we release our docs from this collection
  int initializeCursor(AqlItemBlock* items, size_t pos) override;

  AqlItemBlock* getSome(size_t atLeast, size_t atMost) override final;

  // skip between atLeast and atMost, returns the number actually skipped . . .
  // will only return less than atLeast if there aren't atLeast many
  // things to skip overall.
  size_t skipSome(size_t atLeast, size_t atMost) override final;

 private:
  /// @brief adds a SORT to a dynamic IN condition
  arangodb::aql::AstNode* makeUnique(arangodb::aql::AstNode*) const;

  /// @brief create an iterator object
  void createCursor();

  /// @brief Forwards _cursor to the next available index
  void startNextCursor();

  /// @brief Initializes the indexes
  bool initIndexes();

  /// @brief whether or not one of the bounds expressions requires V8
  bool hasV8Expression() const;

  /// @brief execute the bounds expressions
  void executeExpressions();

  /// @brief continue skipping of documents
  bool skipIndex(size_t atMost);

  /// @brief continue fetching of documents
  bool readIndex(size_t atMost, IndexIterator::DocumentCallback const&);

  /// @brief frees the memory for all non-constant expressions
  void cleanupNonConstExpressions();

  /// @brief order a cursor for the index at the specified position
  OperationCursor* orderCursor(size_t currentIndex);

 private:
  /// @brief collection
  Collection const* _collection;

  /// @brief current position in _indexes
  size_t _currentIndex;

  /// @brief _indexes holds all Indexes used in this block
  std::vector<transaction::Methods::IndexHandle> _indexes;

  /// @brief _nonConstExpressions, list of all non const expressions, mapped
  /// by their _condition node path indexes
  std::vector<NonConstExpression*> _nonConstExpressions;

  /// @brief _inVars, a vector containing for each expression above
  /// a vector of Variable*, used to execute the expression
  std::vector<std::vector<Variable const*>> _inVars;

  /// @brief _inRegs, a vector containing for each expression above
  /// a vector of RegisterId, used to execute the expression
  std::vector<std::vector<RegisterId>> _inRegs;

  /// @brief _cursor: holds the current index cursor found using
  /// createCursor (if any) so that it can be read in chunks and not
  /// necessarily all at once.
  arangodb::OperationCursor* _cursor;
  
  /// @brief a vector of cursors for the index block. cursors can be
  /// reused
  std::vector<std::unique_ptr<OperationCursor>> _cursors;

  /// @brief _condition: holds the complete condition this Block can serve for
  AstNode const* _condition;

  /// @brief set of already returned documents. Used to make the result distinct
  std::unordered_set<TRI_voc_rid_t> _alreadyReturned;

  /// @brief whether or not at least one expression uses v8
  bool _hasV8Expression;
  
  /// @brief A managed document result to temporary hold one document
  std::unique_ptr<ManagedDocumentResult> _mmdr;

  /// @brief Flag if all indexes are exhausted to be maintained accross several getSome() calls
  bool _indexesExhausted;

  /// @brief Flag if the current index pointer is the last of the list.
  ///        Used in uniqueness checks.
  bool _isLastIndex;

  /// @brief Counter how many documents have been returned/skipped
  ///        during one call.
  size_t _returned;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
