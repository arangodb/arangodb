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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_EXECUTION_BLOCK_H
#define ARANGOD_AQL_EXECUTION_BLOCK_H 1

#include "AqlItemBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Variable.h"

#include <deque>

#if 0

#define DEBUG_BEGIN_BLOCK() try {  //
#define DEBUG_END_BLOCK()                                                     \
  }                                                                           \
  catch (arangodb::basics::Exception const& ex) {                             \
    LOG(WARN) << "arango exception caught in " << __FILE__ << ":" << __LINE__ \
              << ":" << ex.what();                                            \
    throw;                                                                    \
  }                                                                           \
  catch (std::exception const& ex) {                                          \
    LOG(WARN) << "std exception caught in " << __FILE__ << ":" << __LINE__    \
              << ": " << ex.what();                                           \
    throw;                                                                    \
  }                                                                           \
  catch (...) {                                                               \
    LOG(WARN) << "exception caught in " << __FILE__ << ":" << __LINE__;       \
    throw;                                                                    \
  }  //

#else

#define DEBUG_BEGIN_BLOCK()  //
#define DEBUG_END_BLOCK()    //

#endif

namespace arangodb {
class Transaction;

namespace aql {

class ExecutionEngine;

class ExecutionBlock {
 public:
  ExecutionBlock(ExecutionEngine*, ExecutionNode const*);

  virtual ~ExecutionBlock();

 public:
  /// @brief batch size value
  static constexpr inline size_t DefaultBatchSize() { return 1000; }

  /// @brief returns the register id for a variable id
  /// will return ExecutionNode::MaxRegisterId for an unknown variable
  RegisterId getRegister(VariableId id) const;
  RegisterId getRegister(Variable const* variable) const;

  /// @brief determine the number of rows in a vector of blocks
  size_t countBlocksRows(std::vector<AqlItemBlock*> const&) const;

  /// @brief whether or not the query was killed
  bool isKilled() const;

  /// @brief throw an exception if query was killed
  void throwIfKilled();

  /// @brief add a dependency
  void addDependency(ExecutionBlock* ep) { 
    TRI_ASSERT(ep != nullptr);
    _dependencies.emplace_back(ep); 
  }

  /// @brief get all dependencies
  std::vector<ExecutionBlock*> getDependencies() const { return _dependencies; }

  /// @brief remove a dependency, returns true if the pointer was found and
  /// removed, please note that this does not delete ep!
  bool removeDependency(ExecutionBlock* ep);

  /// @brief access the pos-th dependency
  ExecutionBlock* operator[](size_t pos) {
    if (pos >= _dependencies.size()) {
      return nullptr;
    }

    return _dependencies.at(pos);
  }

  /// @brief Methods for execution
  /// Lifecycle is:
  ///    CONSTRUCTOR
  ///    then the ExecutionEngine automatically calls
  ///      initialize() once, including subqueries
  ///    possibly repeat many times:
  ///      initializeCursor(...)   (optionally with bind parameters)
  ///      // use cursor functionality
  ///    then the ExecutionEngine automatically calls
  ///      shutdown()
  ///    DESTRUCTOR
  /// @brief initialize
  virtual int initialize();

  /// @brief initializeCursor, could be called multiple times
  virtual int initializeCursor(AqlItemBlock* items, size_t pos);

  /// @brief shutdown, will be called exactly once for the whole query
  virtual int shutdown(int);

  /// @brief getOne, gets one more item
  virtual AqlItemBlock* getOne() { return getSome(1, 1); }

  /// @brief getSome, gets some more items, semantic is as follows: not
  /// more than atMost items may be delivered. The method tries to
  /// return a block of at least atLeast items, however, it may return
  /// less (for example if there are not enough items to come). However,
  /// if it returns an actual block, it must contain at least one item.
  virtual AqlItemBlock* getSome(size_t atLeast, size_t atMost);

 protected:
  /// @brief request an AqlItemBlock from the memory manager
  AqlItemBlock* requestBlock(size_t, RegisterId);

  /// @brief return an AqlItemBlock to the memory manager
  void returnBlock(AqlItemBlock*&);

  /// @brief copy register data from one block (src) into another (dst)
  /// register values are cloned
  void inheritRegisters(AqlItemBlock const* src, AqlItemBlock* dst, size_t row);

  void inheritRegisters(AqlItemBlock const* src, AqlItemBlock* dst, size_t,
                        size_t);

  /// @brief the following is internal to pull one more block and append it to
  /// our _buffer deque. Returns true if a new block was appended and false if
  /// the dependent node is exhausted.
  bool getBlock(size_t atLeast, size_t atMost);

  /// @brief getSomeWithoutRegisterClearout, same as above, however, this
  /// is the actual worker which does not clear out registers at the end
  /// the idea is that somebody who wants to call the generic functionality
  /// in a derived class but wants to modify the results before the register
  /// cleanup can use this method, internal use only
  AqlItemBlock* getSomeWithoutRegisterClearout(size_t atLeast, size_t atMost);

  /// @brief clearRegisters, clears out registers holding values that are no
  /// longer needed by later nodes
  void clearRegisters(AqlItemBlock* result);

 public:
  /// @brief getSome, skips some more items, semantic is as follows: not
  /// more than atMost items may be skipped. The method tries to
  /// skip a block of at least atLeast items, however, it may skip
  /// less (for example if there are not enough items to come). The number of
  /// elements skipped is returned.
  virtual size_t skipSome(size_t atLeast, size_t atMost);

  // skip exactly <number> outputs, returns <true> if _done after
  // skipping, and <false> otherwise . . .
  bool skip(size_t number, size_t& numActuallySkipped);

  virtual bool hasMore();

  virtual int64_t count() const { return _dependencies[0]->count(); }

  virtual int64_t remaining();

  ExecutionNode const* getPlanNode() const { return _exeNode; }

 protected:
  /// @brief generic method to get or skip some
  virtual int getOrSkipSome(size_t atLeast, size_t atMost, bool skipping,
                            AqlItemBlock*& result, size_t& skipped);

  /// @brief the execution engine
  ExecutionEngine* _engine;

  /// @brief the transaction for this query
  arangodb::Transaction* _trx;

  /// @brief our corresponding ExecutionNode node
  ExecutionNode const* _exeNode;

  /// @brief our dependent nodes
  std::vector<ExecutionBlock*> _dependencies;

  /// @brief this is our buffer for the items, it is a deque of AqlItemBlocks.
  /// We keep the following invariant between this and the other two variables
  /// _pos and _done: If _buffer.size() != 0, then 0 <= _pos <
  /// _buffer[0]->size()
  /// and _buffer[0][_pos] is the next item to be handed on. If _done is true,
  /// then no more documents will ever be returned. _done will be set to
  /// true if and only if we have no more data ourselves (i.e.
  /// _buffer.size()==0)
  /// and we have unsuccessfully tried to get another block from our dependency.
  std::deque<AqlItemBlock*> _buffer;

  /// @brief current working position in the first entry of _buffer
  size_t _pos;

  /// @brief if this is set, we are done, this is reset to false by execute()
  bool _done;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
