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
/// @author Dr. Frank Celler
/// @author Martin Schoenert
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOC_BASE_VOC_SHAPER_H
#define ARANGOD_VOC_BASE_VOC_SHAPER_H 1

#include "Basics/Common.h"
#include "Basics/associative.h"
#include "VocBase/datafile.h"
#include "VocBase/shape-accessor.h"
#include "VocBase/shaped-json.h"
#include "VocBase/Shaper.h"
#include "Wal/Marker.h"

#define NUM_SHAPE_ACCESSORS 8

struct TRI_document_collection_t;

class VocShaper : public Shaper {
 public:
  VocShaper(VocShaper const&);
  VocShaper& operator=(VocShaper const&);

  VocShaper(TRI_memory_zone_t*, TRI_document_collection_t*);

  ~VocShaper();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the shaper's memory zone
  //////////////////////////////////////////////////////////////////////////////

  TRI_memory_zone_t* memoryZone() const { return _memoryZone; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief looks up a shape by identifier
  //////////////////////////////////////////////////////////////////////////////

  TRI_shape_t const* lookupShapeId(TRI_shape_sid_t) override final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief looks up an attribute name by identifier
  //////////////////////////////////////////////////////////////////////////////

  char const* lookupAttributeId(TRI_shape_aid_t) override final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief looks up an attribute path by identifier
  //////////////////////////////////////////////////////////////////////////////

  TRI_shape_path_t const* lookupAttributePathByPid(TRI_shape_pid_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief finds an attribute path by identifier
  //////////////////////////////////////////////////////////////////////////////

  TRI_shape_pid_t findOrCreateAttributePathByName(char const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief looks up an attribute path by identifier
  //////////////////////////////////////////////////////////////////////////////

  TRI_shape_pid_t lookupAttributePathByName(char const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the attribute name for an attribute path
  //////////////////////////////////////////////////////////////////////////////

  char const* attributeNameShapePid(TRI_shape_pid_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief looks up an attribute identifier by name
  //////////////////////////////////////////////////////////////////////////////

  TRI_shape_aid_t lookupAttributeByName(char const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief finds or creates an attribute identifier by name
  //////////////////////////////////////////////////////////////////////////////

  TRI_shape_aid_t findOrCreateAttributeByName(char const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief finds a shape
  /// if the function returns non-nullptr, the return value is a pointer to an
  /// already existing shape and the value must not be freed
  /// if the function returns nullptr, it has not found the shape and was not
  /// able
  /// to create it. The value must then be freed by the caller
  //////////////////////////////////////////////////////////////////////////////

  TRI_shape_t const* findShape(TRI_shape_t*, bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief finds an accessor
  //////////////////////////////////////////////////////////////////////////////

  TRI_shape_access_t const* findAccessor(TRI_shape_sid_t, TRI_shape_pid_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief extracts a sub-shape
  //////////////////////////////////////////////////////////////////////////////

  bool extractShapedJson(TRI_shaped_json_t const* document, TRI_shape_sid_t sid,
                         TRI_shape_pid_t pid, TRI_shaped_json_t* result,
                         TRI_shape_t const** shape);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief looks up a shape path by identifier
  //////////////////////////////////////////////////////////////////////////////

  TRI_shape_path_t const* findShapePathByName(char const* name, bool create);

 private:
  TRI_memory_zone_t* _memoryZone;
  TRI_document_collection_t* _collection;

  // attribute paths
  arangodb::Mutex _attributePathsCreateLock;

  arangodb::basics::ReadWriteLock _attributePathsByNameLock;
  TRI_associative_pointer_t _attributePathsByName;

  arangodb::basics::ReadWriteLock _attributePathsByPidLock;
  TRI_associative_pointer_t _attributePathsByPid;

  // attributes
  arangodb::Mutex _attributeCreateLock;

  arangodb::basics::ReadWriteLock _attributeNamesLock;
  TRI_associative_pointer_t _attributeNames;

  arangodb::basics::ReadWriteLock _attributeIdsLock;
  TRI_associative_pointer_t _attributeIds;

  // shapes
  arangodb::Mutex _shapeCreateLock;

  arangodb::basics::ReadWriteLock _shapeDictionaryLock;
  TRI_associative_pointer_t _shapeDictionary;

  arangodb::basics::ReadWriteLock _shapeIdsLock;
  TRI_associative_pointer_t _shapeIds;

  // accessors
  arangodb::basics::ReadWriteLock _accessorLock[NUM_SHAPE_ACCESSORS];
  TRI_associative_pointer_t _accessors[NUM_SHAPE_ACCESSORS];

  TRI_shape_pid_t _nextPid;
  std::atomic<TRI_shape_aid_t> _nextAid;
  std::atomic<TRI_shape_sid_t> _nextSid;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief helper method for recursive shapes comparison
///
/// You must either supply (leftDocument, leftObject) or leftShaped.
/// You must either supply (rightDocument, rightObject) or rightShaped.
////////////////////////////////////////////////////////////////////////////////

int TRI_CompareShapeTypes(char const* leftDocument,
                          TRI_shaped_sub_t const* leftObject,
                          TRI_shaped_json_t const* leftShaped,
                          VocShaper* leftShaper, char const* rightDocument,
                          TRI_shaped_sub_t const* rightObject,
                          TRI_shaped_json_t const* rightShaped,
                          VocShaper* rightShaper);

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the shape identifier pointer from a marker
////////////////////////////////////////////////////////////////////////////////

static inline void TRI_EXTRACT_SHAPE_IDENTIFIER_MARKER(TRI_shape_sid_t& dst,
                                                       void const* src) {
#if 0
  auto type = static_cast<TRI_df_marker_t const*>(src)->_type;

  if (type == TRI_DOC_MARKER_KEY_DOCUMENT) {
    dst = static_cast<TRI_doc_document_key_marker_t const*>(src)->_shape;
  } else if (type == TRI_DOC_MARKER_KEY_EDGE) {
    dst = static_cast<TRI_doc_edge_key_marker_t const*>(src)->base._shape;
  } else if (type == TRI_WAL_MARKER_DOCUMENT) {
    dst = static_cast<arangodb::wal::document_marker_t const*>(src)->_shape;
  } else if (type == TRI_WAL_MARKER_EDGE) {
    dst = static_cast<arangodb::wal::edge_marker_t const*>(src)->_shape;
  } else {
    dst = 0;
  }
#endif  
  dst = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the shaped JSON pointer from a marker
////////////////////////////////////////////////////////////////////////////////

static inline void TRI_EXTRACT_SHAPED_JSON_MARKER(TRI_shaped_json_t& dst,
                                                  void const* src) {
#if 0  
  auto type = static_cast<TRI_df_marker_t const*>(src)->_type;

  if (type == TRI_DOC_MARKER_KEY_DOCUMENT || type == TRI_DOC_MARKER_KEY_EDGE) {
    dst._sid = static_cast<TRI_doc_document_key_marker_t const*>(src)->_shape;
    dst._data.length =
        static_cast<TRI_df_marker_t const*>(src)->_size -
        static_cast<TRI_doc_document_key_marker_t const*>(src)->_offsetJson;
    dst._data.data =
        const_cast<char*>(static_cast<char const*>(src)) +
        static_cast<TRI_doc_document_key_marker_t const*>(src)->_offsetJson;
  } else if (type == TRI_WAL_MARKER_DOCUMENT) {
    dst._sid =
        static_cast<arangodb::wal::document_marker_t const*>(src)->_shape;
    dst._data.length =
        static_cast<TRI_df_marker_t const*>(src)->_size -
        static_cast<arangodb::wal::document_marker_t const*>(src)->_offsetJson;
    dst._data.data =
        const_cast<char*>(static_cast<char const*>(src)) +
        static_cast<arangodb::wal::document_marker_t const*>(src)->_offsetJson;
  } else if (type == TRI_WAL_MARKER_EDGE) {
    dst._sid = static_cast<arangodb::wal::edge_marker_t const*>(src)->_shape;
    dst._data.length =
        static_cast<TRI_df_marker_t const*>(src)->_size -
        static_cast<arangodb::wal::edge_marker_t const*>(src)->_offsetJson;
    dst._data.data =
        const_cast<char*>(static_cast<char const*>(src)) +
        static_cast<arangodb::wal::edge_marker_t const*>(src)->_offsetJson;
  } else {
    dst._sid = 0;
    dst._data.length = 0;
    dst._data.data = nullptr;
  }
#endif
    
  dst._sid = 0;
  dst._data.length = 0;
  dst._data.data = nullptr;
}

void TRI_InspectShapedSub(TRI_shaped_sub_t const*, char const*,
                          TRI_shaped_json_t&);

void TRI_InspectShapedSub(TRI_shaped_sub_t const*, TRI_doc_mptr_t const*,
                          char const*&, size_t&);

void TRI_FillShapedSub(TRI_shaped_sub_t*, TRI_shaped_json_t const*,
                       char const*);

#endif
