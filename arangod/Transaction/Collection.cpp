////////////////////////////////////////////////////////////////////////////////
/// @brief transaction collection 
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Collection.h"
#include "VocBase/vocbase.h"

using namespace triagens::transaction;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction collection
////////////////////////////////////////////////////////////////////////////////

Collection::Collection (TRI_voc_cid_t id,
                        TRI_vocbase_col_t* collection,
                        Collection::AccessType accessType,
                        bool responsibility,
                        bool locked) 
  : _id(id),
    _collection(collection),
    _accessType(accessType),
    _responsibility(responsibility),
    _locked(locked),
    _used(false) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the transaction collection
////////////////////////////////////////////////////////////////////////////////

Collection::~Collection () {
  unlock();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief use the collection
////////////////////////////////////////////////////////////////////////////////

int Collection::use (TRI_vocbase_t* vocbase) {
  if (hasResponsibility()) {
    if (! _used) {
      TRI_vocbase_col_t* collection = TRI_UseCollectionByIdVocBase(vocbase, _id);
  
      if (collection == nullptr) {
        return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
      }

      if (collection->_collection == nullptr) {
        return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
      }

      _used = true;
    }
  }
      
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unuse the collection
////////////////////////////////////////////////////////////////////////////////

int Collection::unuse (TRI_vocbase_t* vocbase) {
  if (hasResponsibility()) {
    if (_used) {
      TRI_ReleaseCollectionVocBase(vocbase, _collection);
      _used = false;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lock the collection
////////////////////////////////////////////////////////////////////////////////

int Collection::lock () {
  int res = TRI_ERROR_NO_ERROR;

  if (hasResponsibility()) {
    if (! isLocked()) { 
      TRI_primary_collection_t* p = primary();

      if (p == nullptr) {
        return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
      }

      if (_accessType == Collection::AccessType::READ) {
        res = p->beginRead(p); 
      }
      else {
        res = p->beginWrite(p); 
      }

      if (res == TRI_ERROR_NO_ERROR) {
        _locked = true;
      }
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlock the collection
////////////////////////////////////////////////////////////////////////////////

int Collection::unlock () {
  int res = TRI_ERROR_NO_ERROR;

  if (hasResponsibility()) {
    if (isLocked()) { 
      TRI_primary_collection_t* p = primary();
      
      if (p == nullptr) {
        return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
      }

      if (_accessType == Collection::AccessType::READ) {
        res = p->endRead(p);
      }
      else {
        res = p->endWrite(p);
      }

      if (res == TRI_ERROR_NO_ERROR) {
        _locked = false;
      }
    }
  }

  return res;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
