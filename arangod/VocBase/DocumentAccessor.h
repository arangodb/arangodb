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

#ifndef ARANGOD_VOC_BASE_DOCUMENT_ACCESSOR_H
#define ARANGOD_VOC_BASE_DOCUMENT_ACCESSOR_H 1

#include "Basics/Common.h"
#include "Basics/JsonHelper.h"
#include "Utils/Transaction.h"
#include "VocBase/document-collection.h"
#include "VocBase/shape-accessor.h"
#include "VocBase/shaped-json.h"
#include "VocBase/Shaper.h"
#include "Wal/Marker.h"

#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

struct TRI_doc_mptr_t;

namespace arangodb {
class CollectionNameResolver;
}

class DocumentAccessor {
 public:
  DocumentAccessor(DocumentAccessor const&);
  DocumentAccessor& operator=(DocumentAccessor const&);

  DocumentAccessor(arangodb::CollectionNameResolver const* resolver,
                   TRI_document_collection_t* document,
                   TRI_doc_mptr_t const* mptr);

  explicit DocumentAccessor(TRI_json_t const* json);
  explicit DocumentAccessor(VPackSlice const& slice);

  ~DocumentAccessor();

 public:
  bool hasKey(std::string const& attribute) const;

  bool isObject() const;

  bool isArray() const;

  size_t length() const;

  DocumentAccessor& get(char const* name, size_t nameLength);

  DocumentAccessor& get(std::string const& name);

  DocumentAccessor& at(int64_t index);

  arangodb::basics::Json toJson();

 private:
  void setToNull();

  void lookupJsonAttribute(char const* name, size_t nameLength);

  void lookupDocumentAttribute(char const* name, size_t nameLength);

 private:
  arangodb::CollectionNameResolver const* _resolver;

  TRI_document_collection_t* _document;

  TRI_doc_mptr_t const* _mptr;

  std::unique_ptr<TRI_json_t> _json;  // the JSON that we own

  TRI_json_t const* _current;  // the JSON that we point to
};

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the key from a marker
////////////////////////////////////////////////////////////////////////////////

static inline std::string TRI_EXTRACT_MARKER_KEY(
    arangodb::Transaction* trx, TRI_df_marker_t const* marker) {
  if (marker->_type == TRI_WAL_MARKER_VPACK_DOCUMENT) {
    auto b = reinterpret_cast<char const*>(marker) +
             sizeof(arangodb::wal::vpack_document_marker_t);
    VPackSlice slice(reinterpret_cast<uint8_t const*>(b), trx->vpackOptions());
    return slice.get(TRI_VOC_ATTRIBUTE_KEY).copyString();
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  // invalid marker type
  TRI_ASSERT(false);
#endif

  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the key from a marker
////////////////////////////////////////////////////////////////////////////////

static inline std::string TRI_EXTRACT_MARKER_KEY(arangodb::Transaction* trx,
                                                 TRI_doc_mptr_t const* mptr) {
  return TRI_EXTRACT_MARKER_KEY(
      trx, static_cast<TRI_df_marker_t const*>(mptr->getDataPtr()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the revision id from a marker
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_rid_t TRI_EXTRACT_MARKER_RID(
    arangodb::Transaction* trx, TRI_df_marker_t const* marker) {
  if (marker->_type == TRI_WAL_MARKER_VPACK_DOCUMENT) {
    auto b = reinterpret_cast<char const*>(marker) +
             sizeof(arangodb::wal::vpack_document_marker_t);
    VPackSlice slice(reinterpret_cast<uint8_t const*>(b), trx->vpackOptions());
    VPackSlice value = slice.get(TRI_VOC_ATTRIBUTE_REV);
    return arangodb::velocypack::readUInt64(value.start() + 1);
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  // invalid marker type
  TRI_ASSERT(false);
#endif

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the revision id from a master pointer
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_rid_t TRI_EXTRACT_MARKER_RID(arangodb::Transaction* trx,
                                                   TRI_doc_mptr_t const* mptr) {
  return TRI_EXTRACT_MARKER_RID(
      trx, static_cast<TRI_df_marker_t const*>(mptr->getDataPtr()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares the key from a master pointer to the given key
////////////////////////////////////////////////////////////////////////////////

static inline bool TRI_MATCHES_MARKER_KEY(arangodb::Transaction* trx,
                                          TRI_doc_mptr_t const* mptr,
                                          char const* key) {
  auto marker = static_cast<TRI_df_marker_t const*>(mptr->getDataPtr());

  if (marker->_type == TRI_WAL_MARKER_VPACK_DOCUMENT) {
    auto b = reinterpret_cast<char const*>(marker) +
             sizeof(arangodb::wal::vpack_document_marker_t);
    VPackSlice slice(reinterpret_cast<uint8_t const*>(b), trx->vpackOptions());
    VPackValueLength len;
    char const* p = slice.get(TRI_VOC_ATTRIBUTE_KEY).getString(len);
    if (len != strlen(key)) {
      return false;
    }
    return (memcmp(p, key, len) == 0);
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  // invalid marker type
  TRI_ASSERT(false);
#endif
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares the key from a master pointer to the given key
////////////////////////////////////////////////////////////////////////////////

static inline bool TRI_MATCHES_MARKER_KEY(arangodb::Transaction* trx,
                                          TRI_doc_mptr_t const* left,
                                          TRI_doc_mptr_t const* right) {
  auto lm = static_cast<TRI_df_marker_t const*>(left->getDataPtr());
  auto rm = static_cast<TRI_df_marker_t const*>(right->getDataPtr());

  if (lm->_type == TRI_WAL_MARKER_VPACK_DOCUMENT &&
      rm->_type == TRI_WAL_MARKER_VPACK_DOCUMENT) {
    auto lb = reinterpret_cast<char const*>(lm) +
              sizeof(arangodb::wal::vpack_document_marker_t);
    VPackSlice ls(reinterpret_cast<uint8_t const*>(lb), trx->vpackOptions());
    VPackValueLength llen;
    char const* p = ls.get(TRI_VOC_ATTRIBUTE_KEY).getString(llen);

    auto rb = reinterpret_cast<char const*>(rm) +
              sizeof(arangodb::wal::vpack_document_marker_t);
    VPackSlice rs(reinterpret_cast<uint8_t const*>(rb), trx->vpackOptions());
    VPackValueLength rlen;
    char const* q = rs.get(TRI_VOC_ATTRIBUTE_KEY).getString(rlen);

    if (llen != rlen) {
      return false;
    }
    return (memcmp(p, q, llen) == 0);
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  // invalid marker type
  TRI_ASSERT(false);
#endif
  return false;
}

#endif
