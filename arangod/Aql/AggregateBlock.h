////////////////////////////////////////////////////////////////////////////////
/// @brief AQL AggregateBlock
///
/// @file
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
/// @author Jan Steemann
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_AGGREGATE_BLOCK_H
#define ARANGODB_AQL_AGGREGATE_BLOCK_H 1

#include "Basics/Common.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"

namespace triagens {
  namespace utils {
    class AqlTransaction;
  }

  namespace aql {

    class AqlItemBlock;
    class ExecutionEngine;

// -----------------------------------------------------------------------------
// --SECTION--                                                   AggregatorGroup
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief details about the current group
///////////////////////////////////////////////////////////////////////////////

    struct AggregatorGroup {
      std::vector<AqlValue> groupValues;
      std::vector<TRI_document_collection_t const*> collections;

      std::vector<AqlItemBlock*> groupBlocks;
      size_t firstRow;
      size_t lastRow;
      size_t groupLength;
      bool rowsAreValid;
      bool const count;

      AggregatorGroup () = delete;

      explicit AggregatorGroup (bool);

      ~AggregatorGroup ();

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

      void addValues (AqlItemBlock const* src, 
                      RegisterId groupRegister);
    };

// -----------------------------------------------------------------------------
// --SECTION--                                              SortedAggregateBlock
// -----------------------------------------------------------------------------

    class SortedAggregateBlock : public ExecutionBlock  {

      public:

        SortedAggregateBlock (ExecutionEngine*,
                              AggregateNode const*);

        ~SortedAggregateBlock ();

        int initialize () override;

      private:

        int getOrSkipSome (size_t atLeast,
                           size_t atMost,
                           bool skipping,
                           AqlItemBlock*& result,
                           size_t& skipped) override;

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
/// @brief the optional register that contains the input expression values for 
/// each group
////////////////////////////////////////////////////////////////////////////////

        RegisterId _expressionRegister;

////////////////////////////////////////////////////////////////////////////////
/// @brief the optional register that contains the values for each group
/// if no values should be returned, then this has a value of MaxRegisterId
/// this register is also used for counting in case WITH COUNT INTO var is used
////////////////////////////////////////////////////////////////////////////////

        RegisterId _groupRegister;

////////////////////////////////////////////////////////////////////////////////
/// @brief list of variables names for the registers
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::string> _variableNames;
        
    };

// -----------------------------------------------------------------------------
// --SECTION--                                              HashedAggregateBlock
// -----------------------------------------------------------------------------

    class HashedAggregateBlock : public ExecutionBlock  {

      public:

        HashedAggregateBlock (ExecutionEngine*,
                              AggregateNode const*);

        ~HashedAggregateBlock ();

        int initialize () override;

      private:

        int getOrSkipSome (size_t atLeast,
                           size_t atMost,
                           bool skipping,
                           AqlItemBlock*& result,
                           size_t& skipped) override;

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief pairs, consisting of out register and in register
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::pair<RegisterId, RegisterId>> _aggregateRegisters;

////////////////////////////////////////////////////////////////////////////////
/// @brief the optional register that contains the values for each group
/// if no values should be returned, then this has a value of MaxRegisterId
/// this register is also used for counting in case WITH COUNT INTO var is used
////////////////////////////////////////////////////////////////////////////////

        RegisterId _groupRegister;
        
////////////////////////////////////////////////////////////////////////////////
/// @brief hasher for a vector of AQL values
////////////////////////////////////////////////////////////////////////////////

        struct GroupKeyHash {
          GroupKeyHash (triagens::arango::AqlTransaction* trx,
                        std::vector<TRI_document_collection_t const*>& colls)
            : _trx(trx),
              _colls(colls),
              _num(colls.size()) {
          }

          size_t operator() (std::vector<AqlValue> const& value) const;
          
          triagens::arango::AqlTransaction* _trx;
          std::vector<TRI_document_collection_t const*>& _colls;
          size_t const _num;
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief comparator for a vector of AQL values
////////////////////////////////////////////////////////////////////////////////
        
        struct GroupKeyEqual {
          GroupKeyEqual (triagens::arango::AqlTransaction* trx,
                         std::vector<TRI_document_collection_t const*>& colls)
            : _trx(trx),
              _colls(colls) {
          }

          bool operator() (std::vector<AqlValue> const&,
                           std::vector<AqlValue> const&) const;
          
          triagens::arango::AqlTransaction* _trx;
          std::vector<TRI_document_collection_t const*>& _colls;
        };
        
    };

  }  // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
