////////////////////////////////////////////////////////////////////////////////
/// @brief "safe" collection accessor
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_UTILS_COLLECTION_LOCK_H
#define TRIAGENS_UTILS_COLLECTION_LOCK_H 1

#include "Logger/Logger.h"
#include "Basics/StringUtils.h"
#include "ShapedJson/json-shaper.h"
#include "VocBase/primary-collection.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::basics;

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                          class CollectionAccessor
// -----------------------------------------------------------------------------

    class CollectionAccessor {

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief CollectionAccessor
////////////////////////////////////////////////////////////////////////////////

      private:
        CollectionAccessor (const CollectionAccessor&);
        CollectionAccessor& operator= (const CollectionAccessor&);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     private enums
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

        enum LockTypes {
          TYPE_NONE = 0,
          TYPE_READ,
          TYPE_WRITE
        };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the accessor
////////////////////////////////////////////////////////////////////////////////

        CollectionAccessor (TRI_vocbase_t* const vocbase, 
                            const string& name,
                            const TRI_col_type_e type,
                            const bool create) : 
          _vocbase(vocbase), 
          _name(name),
          _type(type),
          _create(create),
          _lockType(TYPE_NONE), 
          _collection(0), 
          _primaryCollection(0) {

          assert(_vocbase);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the accessor
////////////////////////////////////////////////////////////////////////////////

        ~CollectionAccessor () {
          release();
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief use the collection and initialise the accessor
////////////////////////////////////////////////////////////////////////////////

        int use () {
          if (_collection != 0) {
            // we already called use() before
            return TRI_ERROR_NO_ERROR;
          }

          if (_name.empty()) {
            // name is empty. cannot open the collection
            return TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
          }

          // open or create the collection
          if (isdigit(_name[0])) {
            TRI_voc_cid_t id = StringUtils::uint64(_name);

            _collection = TRI_LookupCollectionByIdVocBase(_vocbase, id);
          }
          else {
            if (_type == TRI_COL_TYPE_DOCUMENT) {
              _collection = TRI_FindDocumentCollectionByNameVocBase(_vocbase, _name.c_str(), _create);
            }
            else if (_type == TRI_COL_TYPE_EDGE) {
              _collection = TRI_FindEdgeCollectionByNameVocBase(_vocbase, _name.c_str(), _create);
            }
          }
 
          if (_collection == 0) {
            // collection not found
            return TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
          }

          int result = TRI_UseCollectionVocBase(_vocbase, const_cast<TRI_vocbase_col_s*>(_collection));

          if (TRI_ERROR_NO_ERROR != result) {
            _collection = 0;
            return TRI_set_errno(result);
          }

          LOGGER_TRACE << "using collection " << _name;
          assert(_collection->_collection != 0);
          _primaryCollection = _collection->_collection;

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief free all resources. accessor cannot be used after calling this
////////////////////////////////////////////////////////////////////////////////

        bool unuse () {
          return release();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read-lock the collection
////////////////////////////////////////////////////////////////////////////////

        bool beginRead () {
          return lock(TYPE_READ);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief write-lock the collection
////////////////////////////////////////////////////////////////////////////////

        bool beginWrite () {
          return lock(TYPE_WRITE);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read-unlock the collection
////////////////////////////////////////////////////////////////////////////////
          
        bool endRead () {
          return unlock(TYPE_READ);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief write-unlock the collection
////////////////////////////////////////////////////////////////////////////////

        bool endWrite () {
          return unlock(TYPE_WRITE);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a collection is initialised
////////////////////////////////////////////////////////////////////////////////

        inline bool isValid () const {
          return (_collection != 0);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a lock is already held
////////////////////////////////////////////////////////////////////////////////

        inline bool isLocked () const {
          return (_lockType != TYPE_NONE);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the underlying collection
////////////////////////////////////////////////////////////////////////////////

        inline const bool waitForSync () const {
          assert(_primaryCollection != 0);
          return _primaryCollection->base._waitForSync;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the underlying primary collection
////////////////////////////////////////////////////////////////////////////////

        inline TRI_primary_collection_t* primary () {
          assert(_primaryCollection != 0);
          return _primaryCollection;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the underlying collection's id
////////////////////////////////////////////////////////////////////////////////

        const inline TRI_voc_cid_t cid () const {
          assert(_collection != 0);
          return _collection->_cid;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection's shaper
////////////////////////////////////////////////////////////////////////////////
        
        inline TRI_shaper_t* shaper () const {
          assert(_primaryCollection != 0);
          return _primaryCollection->_shaper;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief release all locks
////////////////////////////////////////////////////////////////////////////////
        
        bool release () {
          if (_collection == 0) {
            return false;
          }

          if (isLocked()) {
            unlock(_lockType);
          }
          
          LOGGER_TRACE << "releasing collection";
          TRI_ReleaseCollectionVocBase(_vocbase, _collection);
          _collection = 0;
          _primaryCollection = 0;

          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the lock type
////////////////////////////////////////////////////////////////////////////////

        inline void setLock (const LockTypes type) {
          _lockType = type;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief lock a collection in read or write mode
////////////////////////////////////////////////////////////////////////////////

        bool lock (const LockTypes type) {
          if (! isValid()) {
            LOGGER_ERROR << "logic error - attempt to lock uninitialised collection " << _name;
            return false;
          }

          if (isLocked()) {
            LOGGER_ERROR << "logic error - attempt to lock already locked collection " << _name;
          }

          assert(_primaryCollection != 0);

          int result = TRI_ERROR_INTERNAL;
          if (TYPE_READ == type) {
            LOGGER_TRACE << "read-locking collection " << _name;
            result = _primaryCollection->beginRead(_primaryCollection);
          }
          else if (TYPE_WRITE == type) {
            LOGGER_TRACE << "write-locking collection " << _name;
            result = _primaryCollection->beginWrite(_primaryCollection);
          }
          else {
            assert(false);
          }

          if (TRI_ERROR_NO_ERROR == result) {
            setLock(type);
            return true;
          }
          
          LOGGER_WARNING << "could not lock collection";

          return false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief unlock a collection 
////////////////////////////////////////////////////////////////////////////////

        bool unlock (const LockTypes type) {
          if (! isValid()) {
            LOGGER_ERROR << "logic error - attempt to unlock uninitialised collection " << _name;
            return false;
          }

          if (! isLocked()) {
            LOGGER_ERROR << "logic error - attempt to unlock non-locked collection " << _name;
          }
          
          assert(_primaryCollection != 0);

          int result = TRI_ERROR_INTERNAL;
          if (TYPE_READ == type) { 
            LOGGER_TRACE << "read-unlocking collection " << _name;
            result = _primaryCollection->endRead(_primaryCollection);
          }
          else if (TYPE_WRITE == type) {
            LOGGER_TRACE << "write-unlocking collection " << _name;
            result = _primaryCollection->endWrite(_primaryCollection);
          }
          else {
            assert(false);
          }

          if (TRI_ERROR_NO_ERROR == result) {
            setLock(TYPE_NONE);
            return true;
          }

          LOGGER_WARNING << "could not unlock collection " << _name;

          return false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the vocbase
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_t* const _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection name / id
////////////////////////////////////////////////////////////////////////////////

        const string _name;

////////////////////////////////////////////////////////////////////////////////
/// @brief type of collection
////////////////////////////////////////////////////////////////////////////////
        
        const TRI_col_type_e _type;

////////////////////////////////////////////////////////////////////////////////
/// @brief create flag for collection
////////////////////////////////////////////////////////////////////////////////

        const bool _create;

////////////////////////////////////////////////////////////////////////////////
/// @brief the type of lock currently held
////////////////////////////////////////////////////////////////////////////////

        LockTypes _lockType;

////////////////////////////////////////////////////////////////////////////////
/// @brief the underlying vocbase collection
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_col_t* _collection;

////////////////////////////////////////////////////////////////////////////////
/// @brief corresponding loaded primary collection
////////////////////////////////////////////////////////////////////////////////

        TRI_primary_collection_t* _primaryCollection;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
