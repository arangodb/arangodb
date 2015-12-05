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

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionNode.h"

namespace triagens {
  namespace arango {
    class AqlTransaction;
  }

  namespace aql {
// -----------------------------------------------------------------------------
// --SECTION--                                                          NodePath
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief struct to hold the member-indexes in the _condition node
////////////////////////////////////////////////////////////////////////////////

    struct NonConstExpression {
      size_t const orMember;
      size_t const andMember;
      size_t const operatorMember;
      Expression* expression;

      NonConstExpression (size_t orM,
                          size_t andM,
                          size_t opM,
                          Expression* exp)
      : orMember(orM),
        andMember(andM),
        operatorMember(opM),
        expression(exp) {
      }

      ~NonConstExpression () {
        delete expression;
      }
    };


    class ExecutionEngine;

// -----------------------------------------------------------------------------
// --SECTION--                                                    ExecutionBlock
// -----------------------------------------------------------------------------

    class ExecutionBlock {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        ExecutionBlock (ExecutionEngine*,
                        ExecutionNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        virtual ~ExecutionBlock ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------
      
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the number of rows in a vector of blocks
////////////////////////////////////////////////////////////////////////////////

        size_t countBlocksRows (std::vector<AqlItemBlock*> const&) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the query was killed
////////////////////////////////////////////////////////////////////////////////

        bool isKilled () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief throw an exception if query was killed
////////////////////////////////////////////////////////////////////////////////

        void throwIfKilled ();

////////////////////////////////////////////////////////////////////////////////
/// @brief add a dependency
////////////////////////////////////////////////////////////////////////////////

        void addDependency (ExecutionBlock* ep) {
          _dependencies.emplace_back(ep);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get all dependencies
////////////////////////////////////////////////////////////////////////////////

        std::vector<ExecutionBlock*> getDependencies () const {
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
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize
////////////////////////////////////////////////////////////////////////////////

        virtual int initialize ();

////////////////////////////////////////////////////////////////////////////////
/// @brief initializeCursor, could be called multiple times
////////////////////////////////////////////////////////////////////////////////

        virtual int initializeCursor (AqlItemBlock* items, size_t pos);

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown, will be called exactly once for the whole query
////////////////////////////////////////////////////////////////////////////////

        virtual int shutdown (int);

////////////////////////////////////////////////////////////////////////////////
/// @brief getOne, gets one more item
////////////////////////////////////////////////////////////////////////////////

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

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief request an AqlItemBlock from the memory manager
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* requestBlock (size_t, 
                                    RegisterId);

////////////////////////////////////////////////////////////////////////////////
/// @brief return an AqlItemBlock to the memory manager
////////////////////////////////////////////////////////////////////////////////

        void returnBlock (AqlItemBlock*&);

////////////////////////////////////////////////////////////////////////////////
/// @brief resolve a collection name and return cid and document key
/// this is used for parsing _from, _to and _id values
////////////////////////////////////////////////////////////////////////////////
  
        int resolve (char const*, 
                     TRI_voc_cid_t&, 
                     std::string&) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief copy register data from one block (src) into another (dst)
/// register values are cloned
////////////////////////////////////////////////////////////////////////////////

        void inheritRegisters (AqlItemBlock const* src,
                               AqlItemBlock* dst,
                               size_t row);
        
        void inheritRegisters (AqlItemBlock const* src,
                               AqlItemBlock* dst,
                               size_t,
                               size_t);
        
////////////////////////////////////////////////////////////////////////////////
/// @brief the following is internal to pull one more block and append it to
/// our _buffer deque. Returns true if a new block was appended and false if
/// the dependent node is exhausted.
////////////////////////////////////////////////////////////////////////////////

        bool getBlock (size_t atLeast, size_t atMost);

////////////////////////////////////////////////////////////////////////////////
/// @brief getSomeWithoutRegisterClearout, same as above, however, this
/// is the actual worker which does not clear out registers at the end
/// the idea is that somebody who wants to call the generic functionality
/// in a derived class but wants to modify the results before the register
/// cleanup can use this method, internal use only
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* getSomeWithoutRegisterClearout (size_t atLeast, size_t atMost);

////////////////////////////////////////////////////////////////////////////////
/// @brief clearRegisters, clears out registers holding values that are no
/// longer needed by later nodes
////////////////////////////////////////////////////////////////////////////////

        void clearRegisters (AqlItemBlock* result);

      public:

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

        virtual int64_t count () const {
          return _dependencies[0]->count();
        }

        virtual int64_t remaining ();

        ExecutionNode const* getPlanNode () const {
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
/// @brief the execution engine
////////////////////////////////////////////////////////////////////////////////

        ExecutionEngine* _engine;

////////////////////////////////////////////////////////////////////////////////
/// @brief the transaction for this query
////////////////////////////////////////////////////////////////////////////////

        triagens::arango::AqlTransaction* _trx;

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

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief batch size value
////////////////////////////////////////////////////////////////////////////////

        static size_t const DefaultBatchSize;

    };

  }  // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
