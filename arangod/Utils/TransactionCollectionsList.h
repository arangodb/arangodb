////////////////////////////////////////////////////////////////////////////////
/// @brief transaction collections list
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

#ifndef TRIAGENS_UTILS_TRANSACTION_COLLECTION_LIST_H
#define TRIAGENS_UTILS_TRANSACTION_COLLECTION_LIST_H 1

#include "Basics/Common.h"

#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"

#include "Utils/TransactionCollection.h"


namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                  class TransactionCollectionsList
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                          typedefs
// -----------------------------------------------------------------------------

    class TransactionCollectionsList {

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for contained collections list
////////////////////////////////////////////////////////////////////////////////

      typedef map<TRI_transaction_cid_t, TransactionCollection*> ListType;

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

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief TransactionCollectionsList copy ctor
////////////////////////////////////////////////////////////////////////////////

        TransactionCollectionsList& operator= (const TransactionCollectionsList&);
        TransactionCollectionsList (const TransactionCollectionsList&);

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create a list with a single collection, based on the collection id
////////////////////////////////////////////////////////////////////////////////

        TransactionCollectionsList (TRI_vocbase_t* const vocbase, 
                                    const TRI_transaction_cid_t cid,
                                    TRI_transaction_type_e accessType) : 
          _vocbase(vocbase),
          _collections(),
          _names(),
          _readOnly(true),
          _error(TRI_ERROR_NO_ERROR) {

          addCollection(cid, accessType);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a list with multiple collection names, read/write
////////////////////////////////////////////////////////////////////////////////
        
        TransactionCollectionsList (TRI_vocbase_t* const vocbase,
                                    const vector<string>& readCollections,
                                    const vector<string>& writeCollections) : 
          _vocbase(vocbase),
          _collections(),
          _names(),
          _readOnly(true),
          _error(TRI_ERROR_NO_ERROR) {

          for (size_t i = 0; i < readCollections.size(); ++i) {
            addCollection(readCollections[i], TRI_TRANSACTION_READ);
          }

          for (size_t i = 0; i < writeCollections.size(); ++i) {
            addCollection(writeCollections[i], TRI_TRANSACTION_WRITE);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a list with multiple collection names, read-only
////////////////////////////////////////////////////////////////////////////////
        
        TransactionCollectionsList (TRI_vocbase_t* const vocbase,
                                    const vector<string>& readCollections) :
          _vocbase(vocbase),
          _collections(),
          _names(),
          _readOnly(true),
          _error(TRI_ERROR_NO_ERROR) {

          for (size_t i = 0; i < readCollections.size(); ++i) {
            addCollection(readCollections[i], TRI_TRANSACTION_READ);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a collections list
////////////////////////////////////////////////////////////////////////////////

        ~TransactionCollectionsList () {
          ListType::iterator it;

          for (it = _collections.begin(); it != _collections.end(); ++it) {
            delete (*it).second;
          }

          _collections.clear();
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
/// @brief whether or not the transaction is read-only
////////////////////////////////////////////////////////////////////////////////

        inline bool isReadOnly () const {
          return _readOnly;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get potential error raised during list setup
////////////////////////////////////////////////////////////////////////////////

        inline int getError () const {
          return _error;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all collections
////////////////////////////////////////////////////////////////////////////////

        const vector<TransactionCollection*> getCollections () {
          ListType::iterator it;
          vector<TransactionCollection*> collections;

          for (it = _collections.begin(); it != _collections.end(); ++it) {
            collections.push_back((*it).second);
          }
    
          return collections; 
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the number of collections
////////////////////////////////////////////////////////////////////////////////

        inline size_t size () const {
          return _collections.size();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection id based on a collection name
////////////////////////////////////////////////////////////////////////////////

        TRI_transaction_cid_t getCidByName (const string& name) const {
          map<string, TRI_transaction_cid_t>::const_iterator it = _names.find(name);

          if (it == _names.end()) {
            return 0;
          }

          return (*it).second;
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
/// @brief add a collection to the list, using the collection id
////////////////////////////////////////////////////////////////////////////////

        TransactionCollection* addCollection (const TRI_transaction_cid_t cid, 
                                              TRI_transaction_type_e type) { 
          if (type == TRI_TRANSACTION_WRITE) {
            _readOnly = false;
          }

          ListType::iterator it;
          
          // check if we already have the collection in our list
          it = _collections.find(cid);
          if (it != _collections.end()) {
            // yes, now update the collection in the list
            TransactionCollection* c = (*it).second;

            if (type == TRI_TRANSACTION_WRITE && type != c->getAccessType()) {
              // upgrade the type
              c->setAccessType(type);
            }

            return c;
          }
          else {
            // collection not yet contained in the list
            _collections[cid] = new TransactionCollection(cid, type);

            return _collections[cid];
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to the list, using the collection name
////////////////////////////////////////////////////////////////////////////////

        TransactionCollection* addCollection (const string& name,
                                              TRI_transaction_type_e type) {

          if (name.empty()) {
            _error = TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
            return 0;
          }

          TRI_transaction_cid_t cid;
         
          // look at the first character in the collection name 
          const char first = name[0];
          if (first >= '0' && first <= '9') {
            // name is passed as a string with the collection id inside

            // look up the collection name for the id
            cid = triagens::basics::StringUtils::uint64(name);

            char* n = TRI_GetCollectionNameByIdVocBase(_vocbase, cid);
            if (n == 0) {
              _error = TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
              return 0;
            }
          
            _names[name] = cid;
            _names[string(n)] = cid;

            TRI_Free(TRI_UNKNOWN_MEM_ZONE, n);
          }
          else {
            // name is passed as a "real" name
            TRI_vocbase_col_t* col = TRI_LookupCollectionByNameVocBase(_vocbase, name.c_str());
            if (col == 0) {
              _error = TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
              return 0;
            }

            cid = col->_cid;
            
            _names[name] = cid;
          }

          return addCollection(cid, type);
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
/// @brief vocbase
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_t* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief the list of collections
////////////////////////////////////////////////////////////////////////////////

        ListType _collections;

////////////////////////////////////////////////////////////////////////////////
/// @brief name to cid translator
////////////////////////////////////////////////////////////////////////////////

        std::map<std::string, TRI_transaction_cid_t> _names;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the transaction is read-only
////////////////////////////////////////////////////////////////////////////////

        bool _readOnly;

////////////////////////////////////////////////////////////////////////////////
/// @brief error number
////////////////////////////////////////////////////////////////////////////////

        int _error;

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
