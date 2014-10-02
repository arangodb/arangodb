////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, execution engine
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_EXECUTION_ENGINE_H
#define ARANGODB_AQL_EXECUTION_ENGINE_H 1

#include "Basics/Common.h"

#include "arangod/Aql/AqlItemBlock.h"
#include "arangod/Aql/ExecutionBlock.h"
#include "arangod/Aql/ExecutionPlan.h"
#include "arangod/Aql/ExecutionStats.h"
#include "Utils/AqlTransaction.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                             class ExecutionEngine
// -----------------------------------------------------------------------------

    class ExecutionEngine {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the engine
////////////////////////////////////////////////////////////////////////////////

        ExecutionEngine (AQL_TRANSACTION_V8* trx, Query* query);

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the engine, frees all assigned blocks
////////////////////////////////////////////////////////////////////////////////

        ~ExecutionEngine ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
// @brief create an execution engine from a plan
////////////////////////////////////////////////////////////////////////////////

        static ExecutionEngine* instanciateFromPlan (AQL_TRANSACTION_V8*,
                                                     Query*,
                                                     ExecutionPlan*);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the root block 
////////////////////////////////////////////////////////////////////////////////
        
        ExecutionBlock* root () const {
          TRI_ASSERT(_root != nullptr);
          return _root;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the transaction
////////////////////////////////////////////////////////////////////////////////
        
        AQL_TRANSACTION_V8* getTransaction () const {
          return _trx;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the query
////////////////////////////////////////////////////////////////////////////////
        
        Query* getQuery () const {
          return _query;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* getSome (size_t atLeast, size_t atMost) {
          return _root->getSome(atLeast, atMost);
        }
        
////////////////////////////////////////////////////////////////////////////////
/// @brief getOne
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* getOne () {
          return _root->getSome(1, 1);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief skip
////////////////////////////////////////////////////////////////////////////////

        bool skip (size_t number) {
          return _root->skip(number);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief hasMore
////////////////////////////////////////////////////////////////////////////////

        inline bool hasMore () const {
          return _root->hasMore();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief count
////////////////////////////////////////////////////////////////////////////////

        inline int64_t count () const {
          return _root->count();
        }
        
////////////////////////////////////////////////////////////////////////////////
/// @brief remaining
////////////////////////////////////////////////////////////////////////////////

        inline int64_t remaining () const {
          return _root->remaining();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a block to the engine
////////////////////////////////////////////////////////////////////////////////

        void addBlock (ExecutionBlock*);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief execution statistics for the query
/// note that the statistics are modification by execution blocks
////////////////////////////////////////////////////////////////////////////////

        ExecutionStats               _stats;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief all blocks registered, used for memory management
////////////////////////////////////////////////////////////////////////////////

        std::vector<ExecutionBlock*> _blocks;

////////////////////////////////////////////////////////////////////////////////
/// @brief root block of the engine
////////////////////////////////////////////////////////////////////////////////

        ExecutionBlock*              _root;

////////////////////////////////////////////////////////////////////////////////
/// @brief a pointer to the transaction for this query
////////////////////////////////////////////////////////////////////////////////

        AQL_TRANSACTION_V8*          _trx;

////////////////////////////////////////////////////////////////////////////////
/// @brief a pointer to the query
////////////////////////////////////////////////////////////////////////////////

        Query*                       _query;
    };

  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
