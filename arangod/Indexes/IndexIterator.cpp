////////////////////////////////////////////////////////////////////////////////
/// @brief index iterator
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
/// @author Michael Hackstein
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "IndexIterator.h"
#include "Utils/CollectionNameResolver.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                       struct IndexIteratorContext
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief context for an index iterator
////////////////////////////////////////////////////////////////////////////////

IndexIteratorContext::IndexIteratorContext (TRI_vocbase_t* vocbase,
                                            CollectionNameResolver* resolver)
  : vocbase(vocbase),
    resolver(resolver),
    ownsResolver(resolver == nullptr),
    isRunningInCluster(false) {

}

IndexIteratorContext::~IndexIteratorContext () {
  if (ownsResolver) {
    delete resolver;
  }
}

CollectionNameResolver const* IndexIteratorContext::getResolver () {
  if (resolver == nullptr) {
    TRI_ASSERT(ownsResolver);
    resolver = new CollectionNameResolver(vocbase);
  }
  return resolver;
}

// -----------------------------------------------------------------------------
// --SECTION--                                               class IndexIterator
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief default destructor. Does not free anything
////////////////////////////////////////////////////////////////////////////////

IndexIterator::~IndexIterator () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default implementation for next
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* IndexIterator::next () {
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default implementation for reset
////////////////////////////////////////////////////////////////////////////////

void IndexIterator::reset () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
