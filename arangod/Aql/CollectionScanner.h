////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, collection scanners
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

#ifndef ARANGODB_AQL_COLLECTION_SCANNER_H
#define ARANGODB_AQL_COLLECTION_SCANNER_H 1

#include "Basics/Common.h"
#include "Utils/AqlTransaction.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                          struct CollectionScanner
// -----------------------------------------------------------------------------

    struct CollectionScanner {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------
  
      CollectionScanner (triagens::arango::AqlTransaction*,
                         TRI_transaction_collection_t*); 
  
      virtual ~CollectionScanner ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

      virtual int scan (std::vector<TRI_doc_mptr_copy_t>&, size_t) = 0;

      virtual void reset () = 0;
 
      triagens::arango::AqlTransaction* trx;
      TRI_transaction_collection_t* trxCollection;
      uint32_t totalCount;
      TRI_voc_size_t position;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                    struct RandomCollectionScanner
// -----------------------------------------------------------------------------

    struct RandomCollectionScanner : public CollectionScanner {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------
  
      RandomCollectionScanner (triagens::arango::AqlTransaction*,
                               TRI_transaction_collection_t*);

      int scan (std::vector<TRI_doc_mptr_copy_t>&,
                size_t);

      void reset ();

      uint32_t initialPosition;
      uint32_t step;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                    struct LinearCollectionScanner
// -----------------------------------------------------------------------------

    struct LinearCollectionScanner : public CollectionScanner {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------
  
      LinearCollectionScanner (triagens::arango::AqlTransaction*,
                               TRI_transaction_collection_t*); 

      int scan (std::vector<TRI_doc_mptr_copy_t>&,
                size_t);
      
      void reset ();
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
