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

#include "CollectionScanner.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                          struct CollectionScanner
// -----------------------------------------------------------------------------
      
// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

CollectionScanner::CollectionScanner (triagens::arango::AqlTransaction* trx,
                                      TRI_transaction_collection_t* trxCollection) 
  : trx(trx), 
    trxCollection(trxCollection),
    totalCount(0) {
}
  
CollectionScanner::~CollectionScanner () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                    struct RandomCollectionScanner
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------
  
RandomCollectionScanner::RandomCollectionScanner (triagens::arango::AqlTransaction* trx,
                                                  TRI_transaction_collection_t* trxCollection) 
  : CollectionScanner(trx, trxCollection),
    step(0) {
}

int RandomCollectionScanner::scan (std::vector<TRI_doc_mptr_copy_t>& docs,
                                   size_t batchSize) {
  return trx->readRandom(trxCollection,
                         docs,
                         initialPosition,
                         position,
                         static_cast<uint64_t>(batchSize),
                         step,
                         totalCount);
}

int RandomCollectionScanner::forward (size_t batchSize, size_t& skipped) {
  // Basic implementation, no gain
  std::vector<TRI_doc_mptr_copy_t> unusedDocs;
  unusedDocs.reserve(batchSize);
  int res = scan(unusedDocs, batchSize);
  skipped += unusedDocs.size();
  // TRI_doc_mptr_copy_t is never freed
  unusedDocs.clear();
  return res;
}


// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

void RandomCollectionScanner::reset () {
  initialPosition.reset();
  position.reset();
  step = 0;
}

// -----------------------------------------------------------------------------
// --SECTION--                                    struct LinearCollectionScanner
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

LinearCollectionScanner::LinearCollectionScanner (triagens::arango::AqlTransaction* trx,
                                                  TRI_transaction_collection_t* trxCollection) 
  : CollectionScanner(trx, trxCollection) {

}

int LinearCollectionScanner::scan (std::vector<TRI_doc_mptr_copy_t>& docs,
                                   size_t batchSize) {
  uint64_t skip = 0;
  return trx->readIncremental(trxCollection,
                              docs,
                              position,
                              static_cast<uint64_t>(batchSize),
                              skip,
                              UINT64_MAX,
                              totalCount);
}

int LinearCollectionScanner::forward (size_t batchSize, size_t& skipped) {
  // Basic implementation, no gain
  std::vector<TRI_doc_mptr_copy_t> unusedDocs;
  uint64_t toSkip = static_cast<uint64_t>(batchSize);

  int res = trx->readIncremental(trxCollection,
                                 unusedDocs,
                                 position,
                                 0,
                                 toSkip, // Will be modified. Will reach 0 if batchSize many docs have been skipped
                                 UINT64_MAX,
                                 totalCount);
  uint64_t reallySkipped = static_cast<uint64_t>(batchSize) - toSkip;
  skipped += static_cast<size_t>(reallySkipped);
  TRI_ASSERT(unusedDocs.empty());
  return res;
}
// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

void LinearCollectionScanner::reset () {
  position.reset();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
