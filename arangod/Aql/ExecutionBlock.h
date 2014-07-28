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

#include "VocBase/document-collection.h"

#include "Aql/ExecutionPlan.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                      AqlDocuments
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief struct AqlDocument, used to pipe documents through executions
/// the execution engine keeps one AqlDocument struct for each document
/// that is piped through the engine. Note that the document can exist in
/// one of two formats throughout its lifetime in the engine. When it resides
/// originally in a datafile, it stays there unchanged and we only store
/// a (pointer to) a copy of the TRI_doc_mptr_t struct. As soon as it is 
/// modified (or created on the fly anyway), or if it originally resides
/// in the WAL, we keep the document as a TRI_json_t, wrapped by a Json
/// struct. That is, the following struct has the following invariant:
/// Either the whole struct is empty and thus _json is empty and _mptr is
/// a nullptr. Otherwise, either _json is empty and _mptr is not a nullptr,
/// or _json is non-empty and _mptr is anullptr.
/// Additionally, the struct contains another TRI_json_t holding the
/// current state of the LET variables (and possibly some other
/// computations). This is the _vars attribute.
/// Note that both Json subobjects are constructed as AUTOFREE.
////////////////////////////////////////////////////////////////////////////////

    struct AqlDocument {
      triagens::basics::Json      _json;
      TRI_doc_mptr_copy_t*        _mptr;
      triagens::basics::Json      _vars;

////////////////////////////////////////////////////////////////////////////////
/// @brief convenience constructors
////////////////////////////////////////////////////////////////////////////////

      AqlDocument ()
        : _json(), _mptr(nullptr), _vars() {
      }

      AqlDocument (TRI_doc_mptr_t* mptr)
        : _json(), _vars() {
        _mptr = new TRI_doc_mptr_copy_t(*mptr);
      }

      AqlDocument (triagens::basics::Json json)
        : _json(json), _mptr(nullptr), _vars() {
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

      ~AqlDocument () {
        if (_mptr != nullptr) {
          delete _mptr;
        }
      }

    };

  }  // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


