////////////////////////////////////////////////////////////////////////////////
/// @brief AQL ModificationBlock(s)
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
/// @author Jan Steemann
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_MODIFICATION_BLOCKS_H
#define ARANGODB_AQL_MODIFICATION_BLOCKS_H 1

#include "Basics/Common.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Utils/AqlTransaction.h"
#include "VocBase/shaped-json.h"

struct TRI_df_marker_s;
struct TRI_doc_mptr_copy_t;
struct TRI_json_t;

namespace triagens {
  namespace aql {

    class ExecutionEngine;

// -----------------------------------------------------------------------------
// --SECTION--                                                 ModificationBlock
// -----------------------------------------------------------------------------

    class ModificationBlock : public ExecutionBlock {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        ModificationBlock (ExecutionEngine*, 
                           ModificationNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        virtual ~ModificationBlock ();

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* getSome (size_t atLeast,
                               size_t atMost) override final;

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse 
////////////////////////////////////////////////////////////////////////////////

        virtual AqlItemBlock* work (std::vector<AqlItemBlock*>&) = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a key from the AqlValue passed
////////////////////////////////////////////////////////////////////////////////
          
        int extractKey (AqlValue const&,
                        TRI_document_collection_t const*,
                        std::string&);

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a master pointer from the marker passed
////////////////////////////////////////////////////////////////////////////////

        void constructMptr (TRI_doc_mptr_copy_t*,
                            TRI_df_marker_s const*) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a shard key value has changed
////////////////////////////////////////////////////////////////////////////////
                
        bool isShardKeyChange (struct TRI_json_t const*,
                               struct TRI_json_t const*,
                               bool) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a shard key was set when it must not be set
////////////////////////////////////////////////////////////////////////////////
                
        bool isShardKeyError (struct TRI_json_t const*) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief process the result of a data-modification operation
////////////////////////////////////////////////////////////////////////////////

        void handleResult (int,
                           bool,
                           std::string const *errorMessage = nullptr);

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief output register ($OLD)
////////////////////////////////////////////////////////////////////////////////

        RegisterId _outRegOld;

////////////////////////////////////////////////////////////////////////////////
/// @brief output register ($NEW)
////////////////////////////////////////////////////////////////////////////////

        RegisterId _outRegNew;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection
////////////////////////////////////////////////////////////////////////////////

        Collection const* _collection;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not we're a DB server in a cluster
////////////////////////////////////////////////////////////////////////////////

        bool _isDBServer;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the collection uses the default sharding attributes
////////////////////////////////////////////////////////////////////////////////

        bool _usesDefaultSharding;

////////////////////////////////////////////////////////////////////////////////
/// @brief temporary string buffer for extracting system attributes
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::StringBuffer _buffer;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                       RemoveBlock
// -----------------------------------------------------------------------------

    class RemoveBlock : public ModificationBlock {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        RemoveBlock (ExecutionEngine*, 
                     RemoveNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~RemoveBlock ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse for removing data
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* work (std::vector<AqlItemBlock*>&) override final;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                       InsertBlock
// -----------------------------------------------------------------------------

    class InsertBlock : public ModificationBlock {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        InsertBlock (ExecutionEngine*, 
                     InsertNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~InsertBlock ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse for inserting data
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* work (std::vector<AqlItemBlock*>&) override final;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                       UpdateBlock
// -----------------------------------------------------------------------------

    class UpdateBlock : public ModificationBlock {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        UpdateBlock (ExecutionEngine*, 
                     UpdateNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~UpdateBlock ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse for updating data
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* work (std::vector<AqlItemBlock*>&) override final;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                      ReplaceBlock
// -----------------------------------------------------------------------------

    class ReplaceBlock : public ModificationBlock {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        ReplaceBlock (ExecutionEngine*,
                      ReplaceNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~ReplaceBlock ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse for replacing data
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* work (std::vector<AqlItemBlock*>&) override final;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                       UpsertBlock
// -----------------------------------------------------------------------------

    class UpsertBlock : public ModificationBlock {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        UpsertBlock (ExecutionEngine*, 
                     UpsertNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~UpsertBlock ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse for updating data
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* work (std::vector<AqlItemBlock*>&) override final;

    };

  }  // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
