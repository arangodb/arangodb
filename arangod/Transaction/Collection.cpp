////////////////////////////////////////////////////////////////////////////////
/// @brief transaction collection
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Collection.h"
#include "BasicsC/logging.h"
#include "Utils/Exception.h"
#include "VocBase/key-generator.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::transaction;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction collection
////////////////////////////////////////////////////////////////////////////////

Collection::Collection (TRI_vocbase_col_t* collection,
                        Collection::AccessType accessType,
                        bool responsibility,
                        bool locked)
  : _collection(collection),
    _initialRevision(0),
    _accessType(accessType),
    _responsibility(responsibility),
    _locked(locked),
    _used(false) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the transaction collection
////////////////////////////////////////////////////////////////////////////////

Collection::~Collection () {
  done();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a new revision
////////////////////////////////////////////////////////////////////////////////

TRI_voc_tick_t Collection::generateRevision () {
  return TRI_NewTickServer();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new key
////////////////////////////////////////////////////////////////////////////////

string Collection::generateKey (TRI_voc_tick_t revision) {
  // no key specified, now create one
  TRI_key_generator_t* keyGenerator = static_cast<TRI_key_generator_t*>(primary()->_keyGenerator);

  // create key using key generator
  string key(keyGenerator->generateKey(keyGenerator, revision));

  if (key.empty()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_OUT_OF_KEYS);
  }

  return key;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a key
////////////////////////////////////////////////////////////////////////////////

void Collection::validateKey (std::string const& key) {
  TRI_key_generator_t* keyGenerator = static_cast<TRI_key_generator_t*>(primary()->_keyGenerator);

  int res = keyGenerator->validateKey(keyGenerator, key);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finalise usage of the collection
////////////////////////////////////////////////////////////////////////////////

int Collection::done () {
  int res = unlock();
  unuse();

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief use the collection
////////////////////////////////////////////////////////////////////////////////

int Collection::use () {
  if (hasResponsibility()) {
    if (! _used) {
      TRI_vocbase_col_t* collection = TRI_UseCollectionByIdVocBase(_collection->_vocbase, id());

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

int Collection::unuse () {
  if (hasResponsibility()) {
    if (_used) {
      TRI_ReleaseCollectionVocBase(_collection->_vocbase, _collection);
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
      TRI_document_collection_t* p = primary();

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
      TRI_document_collection_t* p = primary();

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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
