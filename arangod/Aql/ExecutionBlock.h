////////////////////////////////////////////////////////////////////////////////
/// @brief Infrastructure for ExecutionBlocks (the execution engine)
///
/// @file arangod/Aql/ExecutionBlock.h
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_EXECUTION_BLOCK_H
#define ARANGODB_AQL_EXECUTION_BLOCK_H 1

#include <Basics/JsonHelper.h>
#include <ShapedJson/shaped-json.h>

#include "Aql/Types.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionNode.h"
#include "Utils/AqlTransaction.h"
#include "Utils/transactions.h"
#include "Utils/V8TransactionContext.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                   AggregatorGroup
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief details about the current group
////////////////////////////////////////////////////////////////////////////////

    struct AggregatorGroup {
      std::vector<AqlValue> groupValues;
      std::vector<TRI_document_collection_t const*> collections;

      std::vector<AqlItemBlock*> groupBlocks;

      size_t firstRow;
      size_t lastRow;
      bool rowsAreValid;

      AggregatorGroup ()
        : firstRow(0),
          lastRow(0),
          rowsAreValid(false) {
      }

      ~AggregatorGroup () {
        reset();
      }

      void initialize (size_t capacity);
      void reset ();

      void setFirstRow (size_t value) {
        firstRow = value;
        rowsAreValid = true;
      }

      void setLastRow (size_t value) {
        lastRow = value;
        rowsAreValid = true;
      }

      void addValues (AqlItemBlock const* src, RegisterId groupRegister);
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                    ExecutionBlock
// -----------------------------------------------------------------------------

    class ExecutionBlock {

    public:

////////////////////////////////////////////////////////////////////////////////
/// @brief functionality to walk an execution block recursively
////////////////////////////////////////////////////////////////////////////////

        class WalkerWorker {
          public:
            WalkerWorker () {};
            virtual ~WalkerWorker () {};
            virtual void before (ExecutionBlock* eb) {};
            virtual void after (ExecutionBlock* eb) {};
            virtual bool enterSubquery (ExecutionBlock* super,
                                        ExecutionBlock* sub) {
              return true;  // indicates that we enter the subquery
            };
            virtual void leaveSubquery (ExecutionBlock* super,
                                        ExecutionBlock* sub) {};
            bool done (ExecutionBlock* eb) {
              if (_done.find(eb) == _done.end()) {
                _done.insert(eb);
                return false;
              }
              else {
                return true;
              }
            }
            void reset () {
              _done.clear();
            }
          private:
            std::unordered_set<ExecutionBlock*> _done;
        };

        void walk (WalkerWorker* worker);

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        ExecutionBlock (AQL_TRANSACTION_V8* trx,
                        ExecutionNode const* ep)
          : _trx(trx), _exeNode(ep), _done(false), _depth(0),
            _varOverview(nullptr) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        virtual ~ExecutionBlock ();

////////////////////////////////////////////////////////////////////////////////
/// @brief add a dependency
////////////////////////////////////////////////////////////////////////////////

        void addDependency (ExecutionBlock* ep) {
          _dependencies.push_back(ep);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get all dependencies
////////////////////////////////////////////////////////////////////////////////

        std::vector<ExecutionBlock*> getDependencies () {
          return _dependencies;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a dependency, returns true if the pointer was found and
/// removed, please note that this does not delete ep!
////////////////////////////////////////////////////////////////////////////////

        bool removeDependency (ExecutionBlock* ep);

////////////////////////////////////////////////////////////////////////////////
/// @brief access the pos-th dependency
////////////////////////////////////////////////////////////////////////////////

        ExecutionBlock* operator[] (size_t pos) {
          if (pos >= _dependencies.size()) {
            return nullptr;
          }

          return _dependencies.at(pos);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief static analysis, walker class and information collector
////////////////////////////////////////////////////////////////////////////////

        struct VarInfo {
          unsigned int const depth;
          RegisterId const registerId;

          VarInfo () = delete;
          VarInfo (int depth, int registerId)
            : depth(depth), registerId(registerId) {
          }
        };

        struct VarOverview : public WalkerWorker {
          // The following are collected for global usage in the ExecutionBlock:

          // map VariableIds to their depth and registerId:
          std::unordered_map<VariableId, VarInfo> varInfo;

          // number of variables in the frame of the current depth:
          std::vector<RegisterId>                 nrRegsHere;

          // number of variables in this and all outer frames together,
          // the entry with index i here is always the sum of all values
          // in nrRegsHere from index 0 to i (inclusively) and the two
          // have the same length:
          std::vector<RegisterId>                 nrRegs;

          // We collect the subquery blocks to deal with them at the end:
          std::vector<ExecutionBlock*>            subQueryBlocks;

          // Local for the walk:
          unsigned int depth;
          unsigned int totalNrRegs;

          // This is used to tell all Blocks and share a pointer to ourselves
          shared_ptr<VarOverview>* me;

          VarOverview ()
            : depth(0), totalNrRegs(0), me(nullptr) {
            nrRegsHere.push_back(0);
            nrRegs.push_back(0);
          };

          void setSharedPtr (shared_ptr<VarOverview>* shared) {
            me = shared;
          }

          // Copy constructor used for a subquery:
          VarOverview (VarOverview const& v, unsigned int newdepth);
          ~VarOverview () {};

          virtual bool enterSubquery (ExecutionBlock* super,
                                      ExecutionBlock* sub) {
            return false;  // do not walk into subquery
          }

          virtual void after (ExecutionBlock *eb);

        };

////////////////////////////////////////////////////////////////////////////////
/// @brief Methods for execution
/// Lifecycle is:
///    CONSTRUCTOR
///    then the ExecutionEngine automatically calls
///      staticAnalysis() once, including subqueries
///    then the ExecutionEngine automatically calls 
///      initialize() once, including subqueries
///    possibly repeat many times:
///      initCursor(...)   (optionally with bind parameters)
///      // use cursor functionality
///    then the ExecutionEngine automatically calls
///      shutdown()
///    DESTRUCTOR
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief static analysis
////////////////////////////////////////////////////////////////////////////////

        void staticAnalysis (ExecutionBlock* super = nullptr);

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize
////////////////////////////////////////////////////////////////////////////////

        virtual int initialize ();

////////////////////////////////////////////////////////////////////////////////
/// @brief initCursor, could be called multiple times
////////////////////////////////////////////////////////////////////////////////

        virtual int initCursor (AqlItemBlock* items, size_t pos);

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown, will be called exactly once for the whole query
////////////////////////////////////////////////////////////////////////////////

        virtual int shutdown ();

////////////////////////////////////////////////////////////////////////////////
/// @brief copy register data from one block (src) into another (dst)
/// register values are cloned
////////////////////////////////////////////////////////////////////////////////

      protected:

        void inheritRegisters (AqlItemBlock const* src,
                               AqlItemBlock* dst,
                               size_t row);
////////////////////////////////////////////////////////////////////////////////
/// @brief the following is internal to pull one more block and append it to
/// our _buffer deque. Returns true if a new block was appended and false if
/// the dependent node is exhausted.
////////////////////////////////////////////////////////////////////////////////

        bool getBlock (size_t atLeast, size_t atMost);

////////////////////////////////////////////////////////////////////////////////
/// @brief getOne, gets one more item
////////////////////////////////////////////////////////////////////////////////

      public:

        virtual AqlItemBlock* getOne () {
          return getSome(1, 1);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome, gets some more items, semantic is as follows: not
/// more than atMost items may be delivered. The method tries to
/// return a block of at least atLeast items, however, it may return
/// less (for example if there are not enough items to come). However,
/// if it returns an actual block, it must contain at least one item.
////////////////////////////////////////////////////////////////////////////////

        virtual AqlItemBlock* getSome (size_t atLeast, size_t atMost);

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome, skips some more items, semantic is as follows: not
/// more than atMost items may be skipped. The method tries to
/// skip a block of at least atLeast items, however, it may skip
/// less (for example if there are not enough items to come). The number of
/// elements skipped is returned.
////////////////////////////////////////////////////////////////////////////////

        virtual size_t skipSome (size_t atLeast, size_t atMost);

        // skip exactly <number> outputs, returns <true> if _done after
        // skipping, and <false> otherwise . . .
        bool skip (size_t number);

        virtual bool hasMore ();

        virtual int64_t count () {
          return _dependencies[0]->count();
        }

        virtual int64_t remaining ();

        ExecutionNode const* getPlanNode () {
          return _exeNode;
        }

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief generic method to get or skip some
////////////////////////////////////////////////////////////////////////////////

        virtual int getOrSkipSome (size_t atLeast,
                                   size_t atMost,
                                   bool skipping,
                                   AqlItemBlock*& result,
                                   size_t& skipped);

////////////////////////////////////////////////////////////////////////////////
/// @brief the transaction for this query
////////////////////////////////////////////////////////////////////////////////

        AQL_TRANSACTION_V8* _trx;

////////////////////////////////////////////////////////////////////////////////
/// @brief our corresponding ExecutionNode node
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode const* _exeNode;

////////////////////////////////////////////////////////////////////////////////
/// @brief our dependent nodes
////////////////////////////////////////////////////////////////////////////////

        std::vector<ExecutionBlock*> _dependencies;

////////////////////////////////////////////////////////////////////////////////
/// @brief this is our buffer for the items, it is a deque of AqlItemBlocks.
/// We keep the following invariant between this and the other two variables
/// _pos and _done: If _buffer.size() != 0, then 0 <= _pos < _buffer[0]->size()
/// and _buffer[0][_pos] is the next item to be handed on. If _done is true,
/// then no more documents will ever be returned. _done will be set to
/// true if and only if we have no more data ourselves (i.e. _buffer.size()==0)
/// and we have unsuccessfully tried to get another block from our dependency.
////////////////////////////////////////////////////////////////////////////////

        std::deque<AqlItemBlock*> _buffer;

////////////////////////////////////////////////////////////////////////////////
/// @brief current working position in the first entry of _buffer
////////////////////////////////////////////////////////////////////////////////

        size_t _pos;

////////////////////////////////////////////////////////////////////////////////
/// @brief if this is set, we are done, this is reset to false by execute()
////////////////////////////////////////////////////////////////////////////////

        bool _done;

////////////////////////////////////////////////////////////////////////////////
/// @brief depth of frames (number of FOR statements here or above)
////////////////////////////////////////////////////////////////////////////////

        int _depth;  // will be filled in by staticAnalysis

////////////////////////////////////////////////////////////////////////////////
/// @brief info about variables, filled in by staticAnalysis
////////////////////////////////////////////////////////////////////////////////

        std::shared_ptr<VarOverview> _varOverview;

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief batch size value
////////////////////////////////////////////////////////////////////////////////

        static size_t const DefaultBatchSize;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                    SingletonBlock
// -----------------------------------------------------------------------------

    class SingletonBlock : public ExecutionBlock {

      public:

        SingletonBlock (AQL_TRANSACTION_V8* trx, SingletonNode const* ep)
          : ExecutionBlock(trx, ep), _inputRegisterValues(nullptr) {
        }

        ~SingletonBlock () {
          if (_inputRegisterValues != nullptr) {
            delete _inputRegisterValues;
            _inputRegisterValues = nullptr;
          }
        }

        int initialize () {
          _inputRegisterValues = nullptr;   // just in case
          return ExecutionBlock::initialize();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief initCursor, store a copy of the register values coming from above
////////////////////////////////////////////////////////////////////////////////

        int initCursor (AqlItemBlock* items, size_t pos);

        int shutdown ();

        bool hasMore () {
          return ! _done;
        }

        int64_t count () {
          return 1;
        }

        int64_t remaining () {
          return _done ? 0 : 1;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the bind data coming from outside
////////////////////////////////////////////////////////////////////////////////

      private:

        int getOrSkipSome (size_t atLeast,
                           size_t atMost,
                           bool skipping,
                           AqlItemBlock*& result,
                           size_t& skipped);

////////////////////////////////////////////////////////////////////////////////
/// @brief _inputRegisterValues
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* _inputRegisterValues;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                          EnumerateCollectionBlock
// -----------------------------------------------------------------------------

    class EnumerateCollectionBlock : public ExecutionBlock {

      public:

        EnumerateCollectionBlock (AQL_TRANSACTION_V8* trx,
                                  EnumerateCollectionNode const* ep);

        ~EnumerateCollectionBlock () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize fetching of documents
////////////////////////////////////////////////////////////////////////////////

        void initDocuments () {
          _internalSkip = 0;
          if (! moreDocuments()) {
            _done = true;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief continue fetching of documents
////////////////////////////////////////////////////////////////////////////////

        bool moreDocuments ();

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize, here we fetch all docs from the database
////////////////////////////////////////////////////////////////////////////////

        int initialize ();

////////////////////////////////////////////////////////////////////////////////
/// @brief initCursor, here we release our docs from this collection
////////////////////////////////////////////////////////////////////////////////

        int initCursor (AqlItemBlock* items, size_t pos);

        AqlItemBlock* getSome (size_t atLeast, size_t atMost);

////////////////////////////////////////////////////////////////////////////////
// skip between atLeast and atMost, returns the number actually skipped . . .
// will only return less than atLeast if there aren't atLeast many
// things to skip overall.
////////////////////////////////////////////////////////////////////////////////

        size_t skipSome (size_t atLeast, size_t atMost);

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief collection
////////////////////////////////////////////////////////////////////////////////

        Collection* _collection;

////////////////////////////////////////////////////////////////////////////////
/// @brief total number of documents in the collection
////////////////////////////////////////////////////////////////////////////////

        uint32_t _totalCount;

////////////////////////////////////////////////////////////////////////////////
/// @brief internal skip value
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_size_t _internalSkip;

////////////////////////////////////////////////////////////////////////////////
/// @brief document buffer
////////////////////////////////////////////////////////////////////////////////

        std::vector<TRI_doc_mptr_copy_t> _documents;

////////////////////////////////////////////////////////////////////////////////
/// @brief current position in _allDocs
////////////////////////////////////////////////////////////////////////////////

        size_t _posInAllDocs;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                EnumerateListBlock
// -----------------------------------------------------------------------------

    class EnumerateListBlock : public ExecutionBlock {

      public:

        EnumerateListBlock (AQL_TRANSACTION_V8* trx,
                                  EnumerateListNode const* ep)
          : ExecutionBlock(trx, ep) {

        }

        ~EnumerateListBlock () {
        }

        int initialize ();

////////////////////////////////////////////////////////////////////////////////
/// @brief initCursor, here we release our docs from this collection
////////////////////////////////////////////////////////////////////////////////

        int initCursor (AqlItemBlock* items, size_t pos);

        AqlItemBlock* getSome (size_t atLeast, size_t atMost);

////////////////////////////////////////////////////////////////////////////////
// skip between atLeast and atMost returns the number actually skipped . . .
// will only return less than atLeast if there aren't atLeast many
// things to skip overall.
////////////////////////////////////////////////////////////////////////////////

        size_t skipSome (size_t atLeast, size_t atMost);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AqlValue from the inVariable using the current _index
////////////////////////////////////////////////////////////////////////////////

      private:

        AqlValue getAqlValue (AqlValue inVarReg);

////////////////////////////////////////////////////////////////////////////////
/// @brief current position in the _inVariable
////////////////////////////////////////////////////////////////////////////////

        size_t _index;

////////////////////////////////////////////////////////////////////////////////
/// @brief current block in DOCVEC
////////////////////////////////////////////////////////////////////////////////

        size_t _thisblock;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of elements in DOCVEC before the current block
////////////////////////////////////////////////////////////////////////////////

        size_t _seen;

////////////////////////////////////////////////////////////////////////////////
/// @brief total number of elements in DOCVEC
////////////////////////////////////////////////////////////////////////////////

        size_t _DOCVECsize;

////////////////////////////////////////////////////////////////////////////////
/// @brief document collection from DOCVEC
////////////////////////////////////////////////////////////////////////////////

        TRI_document_collection_t const* collection;

////////////////////////////////////////////////////////////////////////////////
/// @brief the register index containing the inVariable of the EnumerateListNode
////////////////////////////////////////////////////////////////////////////////

        RegisterId _inVarRegId;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                  CalculationBlock
// -----------------------------------------------------------------------------

    class CalculationBlock : public ExecutionBlock {

      public:

        CalculationBlock (AQL_TRANSACTION_V8* trx,
                          CalculationNode const* en)
          : ExecutionBlock(trx, en),
            _expression(en->expression()),
            _outReg(0) {

        }

        ~CalculationBlock () {
        }

        int initialize ();

////////////////////////////////////////////////////////////////////////////////
/// @brief doEvaluation, private helper to do the work
////////////////////////////////////////////////////////////////////////////////

      private:

        void doEvaluation (AqlItemBlock* result);

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

        virtual AqlItemBlock* getSome (size_t atLeast,
                                       size_t atMost);

////////////////////////////////////////////////////////////////////////////////
/// @brief we hold a pointer to the expression in the plan
////////////////////////////////////////////////////////////////////////////////

      private:
        Expression* _expression;

////////////////////////////////////////////////////////////////////////////////
/// @brief info about input variables
////////////////////////////////////////////////////////////////////////////////

        std::vector<Variable*> _inVars;

////////////////////////////////////////////////////////////////////////////////
/// @brief info about input registers
////////////////////////////////////////////////////////////////////////////////

        std::vector<RegisterId> _inRegs;

////////////////////////////////////////////////////////////////////////////////
/// @brief output register
////////////////////////////////////////////////////////////////////////////////

        RegisterId _outReg;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the expression is a simple variable reference
////////////////////////////////////////////////////////////////////////////////

        bool _isReference;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                     SubqueryBlock
// -----------------------------------------------------------------------------

    class SubqueryBlock : public ExecutionBlock {

      public:

        SubqueryBlock (AQL_TRANSACTION_V8* trx,
                       SubqueryNode const* en,
                       ExecutionBlock* subquery)
          : ExecutionBlock(trx, en), _outReg(0),
           _subquery(subquery) {
        }

        ~SubqueryBlock () {
        }

        int initialize ();

        virtual AqlItemBlock* getSome (size_t atLeast,
                                       size_t atMost);

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for the pointer to the subquery
////////////////////////////////////////////////////////////////////////////////

        ExecutionBlock* getSubquery() {
          return _subquery;
        }

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief output register
////////////////////////////////////////////////////////////////////////////////

        RegisterId _outReg;

////////////////////////////////////////////////////////////////////////////////
/// @brief we need to have an executionblock and where to write the result
////////////////////////////////////////////////////////////////////////////////

        ExecutionBlock* _subquery;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                       FilterBlock
// -----------------------------------------------------------------------------

    class FilterBlock : public ExecutionBlock {

      public:

        FilterBlock (AQL_TRANSACTION_V8* trx, FilterNode const* ep)
          : ExecutionBlock(trx, ep) {
        }

        ~FilterBlock () {
        }

        int initialize ();

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to actually decide
////////////////////////////////////////////////////////////////////////////////

      private:

        bool takeItem (AqlItemBlock* items, size_t index) {
          AqlValue v = items->getValue(index, _inReg);
          return v.isTrue();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to get another block
////////////////////////////////////////////////////////////////////////////////

        bool getBlock (size_t atLeast, size_t atMost);

        int getOrSkipSome (size_t atLeast,
                           size_t atMost,
                           bool skipping,
                           AqlItemBlock*& result,
                           size_t& skipped);

        bool hasMore ();

        int64_t count () {
          return -1;   // refuse to work
        }

        int64_t remaining () {
          return -1;   // refuse to work
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief input register
////////////////////////////////////////////////////////////////////////////////

      private:

        RegisterId _inReg;

////////////////////////////////////////////////////////////////////////////////
/// @brief vector of indices of those documents in the current block
/// that are chosen
////////////////////////////////////////////////////////////////////////////////

        std::vector<size_t> _chosen;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                    AggregateBlock
// -----------------------------------------------------------------------------

    class AggregateBlock : public ExecutionBlock  {

      public:

        AggregateBlock (AQL_TRANSACTION_V8* trx,
                        ExecutionNode const* ep)
          : ExecutionBlock(trx, ep),
            _groupRegister(0),
            _variableNames() {
        }

        virtual ~AggregateBlock () {};

        int initialize ();

      private:

        int getOrSkipSome (size_t atLeast,
                           size_t atMost,
                           bool skipping,
                           AqlItemBlock*& result,
                           size_t& skipped);

////////////////////////////////////////////////////////////////////////////////
/// @brief writes the current group data into the result
////////////////////////////////////////////////////////////////////////////////

        void emitGroup (AqlItemBlock const* cur,
                        AqlItemBlock* res,
                        size_t row);

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief pairs, consisting of out register and in register
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::pair<RegisterId, RegisterId>> _aggregateRegisters;

////////////////////////////////////////////////////////////////////////////////
/// @brief details about the current group
////////////////////////////////////////////////////////////////////////////////

        AggregatorGroup _currentGroup;

////////////////////////////////////////////////////////////////////////////////
/// @brief the optional register that contains the values for each group
/// if no values should be returned, then this has a value of 0
////////////////////////////////////////////////////////////////////////////////

        RegisterId _groupRegister;

////////////////////////////////////////////////////////////////////////////////
/// @brief list of variables names for the registers
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::string> _variableNames;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                         SortBlock
// -----------------------------------------------------------------------------

    class SortBlock : public ExecutionBlock  {

      public:

        SortBlock (AQL_TRANSACTION_V8* trx,
                   ExecutionNode const* ep)
          : ExecutionBlock(trx, ep),
            _stable(false) {
        }

        virtual ~SortBlock () {};

        int initialize ();

        virtual int initCursor (AqlItemBlock* items, size_t pos);

////////////////////////////////////////////////////////////////////////////////
/// @brief dosorting
////////////////////////////////////////////////////////////////////////////////

      private:

        void doSorting ();

////////////////////////////////////////////////////////////////////////////////
/// @brief OurLessThan
////////////////////////////////////////////////////////////////////////////////

        class OurLessThan {
          public:
            OurLessThan (AQL_TRANSACTION_V8* trx,
                         std::deque<AqlItemBlock*>& buffer,
                         std::vector<std::pair<RegisterId, bool>>& sortRegisters,
                         std::vector<TRI_document_collection_t const*>& colls)
              : _trx(trx),
                _buffer(buffer),
                _sortRegisters(sortRegisters),
                _colls(colls) {
            }

            bool operator() (std::pair<size_t, size_t> const& a,
                             std::pair<size_t, size_t> const& b);

          private:
            AQL_TRANSACTION_V8* _trx;
            std::deque<AqlItemBlock*>& _buffer;
            std::vector<std::pair<RegisterId, bool>>& _sortRegisters;
            std::vector<TRI_document_collection_t const*>& _colls;
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief pairs, consisting of variable and sort direction
/// (true = ascending | false = descending)
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::pair<RegisterId, bool>> _sortRegisters;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the sort should be stable
////////////////////////////////////////////////////////////////////////////////

        bool _stable;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                       LimitBlock
// -----------------------------------------------------------------------------

    class LimitBlock : public ExecutionBlock {

      public:

        LimitBlock (AQL_TRANSACTION_V8* trx, LimitNode const* ep)
          : ExecutionBlock(trx, ep), _offset(ep->_offset), _limit(ep->_limit),
            _state(0) {  // start in the beginning
        }

        ~LimitBlock () {
        }

        int initialize ();

        int initCursor (AqlItemBlock* items, size_t pos);

        virtual int getOrSkipSome (size_t atLeast,
                                   size_t atMost,
                                   bool skipping,
                                   AqlItemBlock*& result,
                                   size_t& skipped);

////////////////////////////////////////////////////////////////////////////////
/// @brief _offset
////////////////////////////////////////////////////////////////////////////////

        size_t _offset;

////////////////////////////////////////////////////////////////////////////////
/// @brief _limit
////////////////////////////////////////////////////////////////////////////////

        size_t _limit;

////////////////////////////////////////////////////////////////////////////////
/// @brief _count, number of items already handed on
////////////////////////////////////////////////////////////////////////////////

        size_t _count;

////////////////////////////////////////////////////////////////////////////////
/// @brief _state, 0 is beginning, 1 is after offset, 2 is done
////////////////////////////////////////////////////////////////////////////////

        int _state;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                       ReturnBlock
// -----------------------------------------------------------------------------

    class ReturnBlock : public ExecutionBlock {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        ReturnBlock (AQL_TRANSACTION_V8* trx, ReturnNode const* ep)
          : ExecutionBlock(trx, ep) {

        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~ReturnBlock () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

        virtual AqlItemBlock* getSome (size_t atLeast,
                                       size_t atMost);

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                       RemoveBlock
// -----------------------------------------------------------------------------

    class RemoveBlock : public ExecutionBlock {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        RemoveBlock (AQL_TRANSACTION_V8* trx, RemoveNode const* ep)
          : ExecutionBlock(trx, ep) {

        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~RemoveBlock () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

        virtual AqlItemBlock* getSome (size_t atLeast,
                                       size_t atMost);

    };

  }  // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
