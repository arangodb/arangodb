////////////////////////////////////////////////////////////////////////////////
/// @brief transaction markers
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

#ifndef ARANGODB_TRANSACTION_MARKER_H
#define ARANGODB_TRANSACTION_MARKER_H 1

#include "Basics/Common.h"
#include "Basics/BsonHelper.h"
#include "VocBase/datafile.h"

namespace triagens {
  namespace transaction {

    struct Marker {
      Marker (TRI_df_marker_type_e type,
              size_t size)
        : buffer(new char[sizeof(TRI_df_marker_t) + size]),
          size(sizeof(TRI_df_marker_t) + size) {

        // initialise the marker header
        auto h = header();
        h->_type = type;
        h->_size = static_cast<TRI_voc_size_t>(size);
        h->_crc  = 0;
        h->_tick = 0;
      }

      virtual ~Marker () {
        if (buffer != nullptr) {
          delete buffer;
        }
      }

      inline TRI_df_marker_t* header () const {
        return (TRI_df_marker_t*) buffer;
      }

      inline char* data () const {
        return (char*) buffer + sizeof(TRI_df_marker_t);
      }

      inline void advance (char*& ptr, size_t length) {
        ptr += length;
      }

      template <typename T> void store (char*& ptr, T value) {
        *((T*) ptr) = value;
        advance(ptr, sizeof(T));
      }

      void store (char*& ptr, char const* src, size_t length) {
        memcpy(ptr, src, length);
        advance(ptr, length);
      }

      char*          buffer;
      uint32_t const size;
    };

    struct DocumentMarker : public Marker {
      DocumentMarker (TRI_voc_tick_t databaseId,
                      TRI_voc_cid_t collectionId,
                      TRI_voc_tid_t transactionId,
                      std::string const& key,
                      TRI_voc_tick_t revision,
                      triagens::basics::Bson const& document)
        : Marker(TRI_WAL_MARKER_DOCUMENT,
                 sizeof(TRI_voc_tick_t) + sizeof(TRI_voc_cid_t) + sizeof(TRI_voc_tid_t) + sizeof(TRI_voc_tick_t) + key.size() + 2 + document.getSize()) {

        char* p = data();
        store<TRI_voc_tick_t>(p, databaseId);
        store<TRI_voc_cid_t>(p, collectionId);
        store<TRI_voc_tid_t>(p, transactionId);
        store<TRI_voc_tick_t>(p, revision);

        // store key
        store<uint8_t>(p, (uint8_t) key.size());
        store(p, key.c_str(), key.size());
        store<unsigned char>(p, '\0');

        // store bson
        store(p, (char const*) document.getBuffer(), static_cast<size_t>(document.getSize()));
      }

      ~DocumentMarker () {
      }

    };

    struct RemoveMarker : public Marker {
      RemoveMarker (TRI_voc_tick_t databaseId,
                    TRI_voc_cid_t collectionId,
                    TRI_voc_tid_t transactionId,
                    std::string const& key)
        : Marker(TRI_WAL_MARKER_REMOVE,
                 sizeof(TRI_voc_tick_t) + sizeof(TRI_voc_cid_t) + sizeof(TRI_voc_tid_t) + key.size() + 2) {

        char* p = data();
        store<TRI_voc_tick_t>(p, databaseId);
        store<TRI_voc_cid_t>(p, collectionId);
        store<TRI_voc_tid_t>(p, transactionId);

        // store key
        store<uint8_t>(p, (uint8_t) key.size());
        store(p, key.c_str(), key.size());
        store<unsigned char>(p, '\0');
      }

      ~RemoveMarker () {
      }

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
