////////////////////////////////////////////////////////////////////////////////
/// @brief AQL item block
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

#ifndef ARANGODB_AQL_AQL_ITEM_BLOCK_H
#define ARANGODB_AQL_AQL_ITEM_BLOCK_H 1

#include "Basics/Common.h"
#include "Aql/AqlValue.h"
#include "Aql/Types.h"
#include "VocBase/document-collection.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                      AqlItemBlock
// -----------------------------------------------------------------------------

// an <AqlItemBlock> is a <nrItems>x<nrRegs> vector of <AqlValue>s (not
// pointers). The size of an <AqlItemBlock> is the number of items. Entries in a
// given column (i.e. all the values of a given register for all items in the
// block) have the same type and belong to the same collection. The document
// collection for a particular column are accessed via <getDocumentCollection>,
// and the entire array of document collections is accessed by
// <getDocumentCollections>. There is no access to an entire item, only access to
// particular registers of an item (via getValue). 

    class AqlItemBlock {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the block
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock (size_t nrItems, 
                      RegisterId nrRegs);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the block
////////////////////////////////////////////////////////////////////////////////

        ~AqlItemBlock ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief getValue, get the value of a register
////////////////////////////////////////////////////////////////////////////////

        AqlValue getValue (size_t index, RegisterId varNr) const {
          return _data[index * _nrRegs + varNr];
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief setValue, set the current value of a register
////////////////////////////////////////////////////////////////////////////////

      void setValue (size_t index, RegisterId varNr, AqlValue value) {
        TRI_ASSERT(_data.capacity() > index * _nrRegs + varNr);
        TRI_ASSERT(_data.at(index * _nrRegs + varNr).isEmpty());
        _data[index * _nrRegs + varNr] = value;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief eraseValue, erase the current value of a register not freeing it
/// this is used if the value is stolen and later released from elsewhere
////////////////////////////////////////////////////////////////////////////////

        void eraseValue (size_t index, RegisterId varNr) {
          _data[index * _nrRegs + varNr].erase();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getDocumentCollection
////////////////////////////////////////////////////////////////////////////////

        inline TRI_document_collection_t const* getDocumentCollection (RegisterId varNr) const {
          return _docColls[varNr];
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief setDocumentCollection, set the current value of a variable or attribute
////////////////////////////////////////////////////////////////////////////////

        inline void setDocumentCollection (RegisterId varNr, TRI_document_collection_t const* docColl) {
          _docColls[varNr] = docColl;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for _nrRegs
////////////////////////////////////////////////////////////////////////////////

        inline RegisterId getNrRegs () const {
          return _nrRegs;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for _nrItems
////////////////////////////////////////////////////////////////////////////////

        inline size_t size () const {
          return _nrItems;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for _data
////////////////////////////////////////////////////////////////////////////////

        inline vector<AqlValue>& getData () {
          return _data;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for _docColls
////////////////////////////////////////////////////////////////////////////////

        vector<TRI_document_collection_t const*>& getDocumentCollections () {
          return _docColls;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief shrink the block to the specified number of rows
////////////////////////////////////////////////////////////////////////////////

        void shrink (size_t nrItems);

////////////////////////////////////////////////////////////////////////////////
/// @brief slice/clone
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* slice (size_t from, 
                             size_t to) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief slice/clone chosen columns for a subset
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* slice (std::vector<size_t>& chosen, 
                             size_t from, 
                             size_t to);

////////////////////////////////////////////////////////////////////////////////
/// @brief splice multiple blocks, note that the new block now owns all
/// AqlValue pointers in the old blocks, therefore, the latter are all
/// set to nullptr, just to be sure.
////////////////////////////////////////////////////////////////////////////////

        static AqlItemBlock* splice (std::vector<AqlItemBlock*> const& blocks);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

      public:

        std::vector<AqlValue> _data;

        std::vector<TRI_document_collection_t const*> _docColls;

        size_t     _nrItems;

        RegisterId _nrRegs;


    };

  }  // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

