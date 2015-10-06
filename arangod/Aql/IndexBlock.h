////////////////////////////////////////////////////////////////////////////////
/// @brief AQL IndexBlock
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Michael Hackstein
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_INDEX_BLOCK_H
#define ARANGODB_AQL_INDEX_BLOCK_H 1

#include "Aql/Collection.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/IndexNode.h"
#include "Utils/AqlTransaction.h"
#include "VocBase/shaped-json.h"

struct TRI_doc_mptr_copy_t;
struct TRI_edge_index_iterator_t;
struct TRI_hash_index_element_multi_s;

namespace triagens {
  namespace aql {

    class AqlItemBlock;

    class ExecutionEngine;

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

      NonConstExpression (
          size_t const orM,
          size_t const andM,
          size_t const opM,
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

// -----------------------------------------------------------------------------
// --SECTION--                                                   IndexRangeBlock
// -----------------------------------------------------------------------------

    class IndexBlock : public ExecutionBlock {

      public:

        IndexBlock (ExecutionEngine* engine,
                    IndexNode const* ep);

        ~IndexBlock ();

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize, here we fetch all docs from the database
////////////////////////////////////////////////////////////////////////////////

        int initialize () override;

////////////////////////////////////////////////////////////////////////////////
/// @brief initializeCursor, here we release our docs from this collection
////////////////////////////////////////////////////////////////////////////////

        int initializeCursor (AqlItemBlock* items, size_t pos) override;

        AqlItemBlock* getSome (size_t atLeast, size_t atMost) override final;

////////////////////////////////////////////////////////////////////////////////
// skip between atLeast and atMost, returns the number actually skipped . . .
// will only return less than atLeast if there aren't atLeast many
// things to skip overall.
////////////////////////////////////////////////////////////////////////////////

        size_t skipSome (size_t atLeast, size_t atMost) override final;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------
      
      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief Forwards _iterator to the next available index
////////////////////////////////////////////////////////////////////////////////

        void startNextIterator ();

////////////////////////////////////////////////////////////////////////////////
/// @brief Initializes the indexes
////////////////////////////////////////////////////////////////////////////////

        bool initIndexes ();

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not one of the bounds expressions requires V8
////////////////////////////////////////////////////////////////////////////////

        bool hasV8Expression () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief build the bounds expressions
////////////////////////////////////////////////////////////////////////////////

        void buildExpressions ();

////////////////////////////////////////////////////////////////////////////////
/// @brief free _condition if it belongs to us
////////////////////////////////////////////////////////////////////////////////

        void freeCondition ();

////////////////////////////////////////////////////////////////////////////////
/// @brief continue fetching of documents
////////////////////////////////////////////////////////////////////////////////

        bool readIndex (size_t atMost);

////////////////////////////////////////////////////////////////////////////////
// @brief: sorts the index range conditions and resets _posInRanges to 0
////////////////////////////////////////////////////////////////////////////////

        void sortConditions ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief collection
////////////////////////////////////////////////////////////////////////////////

        Collection const* _collection;

////////////////////////////////////////////////////////////////////////////////
/// @brief document buffer
////////////////////////////////////////////////////////////////////////////////

        std::vector<TRI_doc_mptr_copy_t> _documents;

////////////////////////////////////////////////////////////////////////////////
/// @brief current position in _allDocs
////////////////////////////////////////////////////////////////////////////////

        size_t _posInDocs;

////////////////////////////////////////////////////////////////////////////////
/// @brief current position in _indexes
////////////////////////////////////////////////////////////////////////////////

        size_t _currentIndex;

////////////////////////////////////////////////////////////////////////////////
/// @brief _indexes holds all Indexes used in this block
////////////////////////////////////////////////////////////////////////////////

        std::vector<Index const*> _indexes;

////////////////////////////////////////////////////////////////////////////////
/// @brief _nonConstExpressions, list of all non const expressions, mapped
/// by their _condition node path indexes
////////////////////////////////////////////////////////////////////////////////
        
        std::vector<NonConstExpression> _nonConstExpressions;

////////////////////////////////////////////////////////////////////////////////
/// @brief _inVars, a vector containing for each expression above
/// a vector of Variable*, used to execute the expression
/////////////////////////////////////////////////////////////////////////////////
        
        std::vector<std::vector<Variable const*>> _inVars;

////////////////////////////////////////////////////////////////////////////////
/// @brief _inRegs, a vector containing for each expression above
/// a vector of RegisterId, used to execute the expression
////////////////////////////////////////////////////////////////////////////////
        
        std::vector<std::vector<RegisterId>> _inRegs;

////////////////////////////////////////////////////////////////////////////////
/// @brief _iterator: holds the index iterator found using
/// getIndexIterator (if any) so that it can be read in chunks and not
/// necessarily all at once.
////////////////////////////////////////////////////////////////////////////////

        triagens::arango::IndexIterator* _iterator;

////////////////////////////////////////////////////////////////////////////////
/// @brief _condition: holds the complete condition this Block can serve for
////////////////////////////////////////////////////////////////////////////////
        
        AstNode const* _condition;
        
////////////////////////////////////////////////////////////////////////////////
/// @brief _condition: holds the complete condition this Block can serve for
////////////////////////////////////////////////////////////////////////////////
        
        AstNode* _evaluatedCondition;
        
////////////////////////////////////////////////////////////////////////////////
/// @brief _flag: since readIndex for primary, hash, edges indexes reads the
/// whole index, this is <true> if initIndex has been called but readIndex has
/// not been called, otherwise it is <false> to avoid rereading the entire index
/// with successive calls to readIndex.
//////////////////////////////////////////////////////////////////////////////////

        bool _flag;
        size_t _posInRanges;
        std::vector<size_t> _sortCoords;

////////////////////////////////////////////////////////////////////////////////
/// @brief _freeCondition: whether or not the _condition is owned by the
/// IndexRangeBlock and must be freed
////////////////////////////////////////////////////////////////////////////////

        bool _freeCondition;

        bool _hasV8Expression;

    };

  }  // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

