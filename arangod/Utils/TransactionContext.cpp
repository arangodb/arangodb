////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "TransactionContext.h"
#include "VocBase/Ditch.h"
#include "VocBase/document-collection.h"

using namespace arangodb;

//////////////////////////////////////////////////////////////////////////////
/// @brief create the context
//////////////////////////////////////////////////////////////////////////////

TransactionContext::TransactionContext(TRI_vocbase_t* vocbase) 
    : _vocbase(vocbase), _resolver(nullptr) {}

//////////////////////////////////////////////////////////////////////////////
/// @brief destroy the context
//////////////////////////////////////////////////////////////////////////////

TransactionContext::~TransactionContext() {
  for (auto& it : _ditches) {
    // we're done with this ditch
    auto& ditch = it.second;
    ditch->ditches()->freeDocumentDitch(ditch, true /* fromTransaction */);
    // If some external entity is still using the ditch, it is kept!
  }
}
  
//////////////////////////////////////////////////////////////////////////////
/// @brief order a document ditch for the collection
//////////////////////////////////////////////////////////////////////////////

DocumentDitch* TransactionContext::orderDitch(TRI_document_collection_t* document) {
  TRI_voc_cid_t cid = document->_info.id();

  auto it = _ditches.find(cid);

  if (it != _ditches.end()) {
    // tell everyone else this ditch is still in use,
    // at least until the transaction is over
    (*it).second->setUsedByTransaction();
    // ditch already exists, return it
    return (*it).second;
  }

  // this method will not throw, but may return a nullptr
  auto ditch = document->ditches()->createDocumentDitch(true, __FILE__, __LINE__);

  if (ditch != nullptr) {
    try {
      _ditches.emplace(cid, ditch);
    }
    catch (...) {
      ditch->ditches()->freeDocumentDitch(ditch, true);
      ditch = nullptr; // return a nullptr
    }
  }

  return ditch;
}
  
//////////////////////////////////////////////////////////////////////////////
/// @brief return the ditch for a collection
/// this will return a nullptr if no ditch exists
//////////////////////////////////////////////////////////////////////////////
  
DocumentDitch* TransactionContext::ditch(TRI_voc_cid_t cid) const {
  auto it = _ditches.find(cid);

  if (it == _ditches.end()) {
    return nullptr;
  }
  return (*it).second;
}

