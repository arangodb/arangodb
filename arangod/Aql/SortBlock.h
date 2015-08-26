////////////////////////////////////////////////////////////////////////////////
/// @brief AQL SortBlock
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_SORT_BLOCK_H
#define ARANGODB_AQL_SORT_BLOCK_H 1

#include "Basics/Common.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Utils/AqlTransaction.h"

namespace triagens {
  namespace aql {

    class AqlItemBlock;

    class ExecutionEngine;

// -----------------------------------------------------------------------------
// --SECTION--                                                         SortBlock
// -----------------------------------------------------------------------------

    class SortBlock : public ExecutionBlock  {

      public:

        SortBlock (ExecutionEngine*,
                   SortNode const*);

        ~SortBlock ();

        int initialize () override;

        int initializeCursor (AqlItemBlock* items, size_t pos) override final;

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
            OurLessThan (triagens::arango::AqlTransaction* trx,
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
            triagens::arango::AqlTransaction* _trx;
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

  }  // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
