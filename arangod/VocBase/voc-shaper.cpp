////////////////////////////////////////////////////////////////////////////////
/// @brief json shaper used to compute the shape of an json object
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
/// @author Dr. Frank Celler
/// @author Martin Schoenert
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "voc-shaper.h"

#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/associative.h"
#include "Basics/hashes.h"
#include "Basics/locks.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Basics/utf8-helper.h"
#include "Utils/Exception.h"
#include "VocBase/document-collection.h"
#include "Wal/LogfileManager.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief collection-based shaper
////////////////////////////////////////////////////////////////////////////////

typedef struct voc_shaper_s {
  TRI_shaper_t                 base;

  TRI_associative_synced_t     _attributeNames;
  TRI_associative_synced_t     _attributeIds;
  TRI_associative_synced_t     _shapeDictionary;
  TRI_associative_synced_t     _shapeIds;

  TRI_associative_pointer_t    _accessors;

  std::atomic<TRI_shape_aid_t> _nextAid;
  std::atomic<TRI_shape_sid_t> _nextSid;

  TRI_document_collection_t*   _collection;

  triagens::basics::Mutex      _accessorLock;
  triagens::basics::Mutex      _shapeLock;
  triagens::basics::Mutex      _attributeLock;
}
voc_shaper_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts an attribute id from a marker
////////////////////////////////////////////////////////////////////////////////

static inline TRI_shape_aid_t GetAttributeId (void const* marker) {
  TRI_df_marker_t const* p = static_cast<TRI_df_marker_t const*>(marker);

  if (p != nullptr) {
    if (p->_type == TRI_DF_MARKER_ATTRIBUTE) {
      return reinterpret_cast<TRI_df_attribute_marker_t const*>(p)->_aid;
    }
    else if (p->_type == TRI_WAL_MARKER_ATTRIBUTE) {
      return reinterpret_cast<triagens::wal::attribute_marker_t const*>(p)->_attributeId;
    }
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts an attribute name from a marker
////////////////////////////////////////////////////////////////////////////////

static inline char const* GetAttributeName (void const* marker) {
  TRI_df_marker_t const* p = static_cast<TRI_df_marker_t const*>(marker);

  if (p != nullptr) {
    if (p->_type == TRI_DF_MARKER_ATTRIBUTE) {
      return reinterpret_cast<char const*>(p) + sizeof(TRI_df_attribute_marker_t);
    }
    else if (p->_type == TRI_WAL_MARKER_ATTRIBUTE) {
      return reinterpret_cast<char const*>(p) + sizeof(triagens::wal::attribute_marker_t);
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the attribute name of a key
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyAttributeName (TRI_associative_synced_t* array, void const* key) {
  char const* k = (char const*) key;

  return TRI_FnvHashString(k);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the attribute name of an element
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementAttributeName (TRI_associative_synced_t* array, void const* element) {
  return TRI_FnvHashString(GetAttributeName(element));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an attribute name and an attribute
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyAttributeName (TRI_associative_synced_t* array, void const* key, void const* element) {
  char const* k = (char const*) key;

  return TRI_EqualString(k, GetAttributeName(element));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute identifier by name
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_aid_t LookupAttributeByName (TRI_shaper_t* shaper,
                                              char const* name) {
  TRI_ASSERT(name != nullptr);

  voc_shaper_t* s = reinterpret_cast<voc_shaper_t*>(shaper);
  std::function<TRI_shape_aid_t(void const*)> callback = [](void const* element) -> TRI_shape_aid_t {
    if (element == nullptr) {
      return 0;
    }
    return GetAttributeId(element);
  };

  return TRI_ProcessByKeyAssociativeSynced<TRI_shape_aid_t>(&s->_attributeNames, name, callback);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds or creates an attribute identifier by name
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_aid_t FindOrCreateAttributeByName (TRI_shaper_t* shaper,
                                                    char const* name) {
  // check if the attribute exists
  TRI_shape_aid_t aid = LookupAttributeByName(shaper, name);

  if (aid != 0) {
    // yes
    return aid;
  }

  // increase attribute id value
  voc_shaper_t* s = reinterpret_cast<voc_shaper_t*>(shaper);
  aid = s->_nextAid++;

  int res = TRI_ERROR_NO_ERROR;
  TRI_document_collection_t* document = s->_collection;

  try {
    triagens::wal::AttributeMarker marker(document->_vocbase->_id, document->_info._cid, aid, std::string(name));

    // lock the index and check that the element is still missing
    {
      MUTEX_LOCKER(s->_attributeLock);
      void const* p = TRI_LookupByKeyAssociativeSynced(&s->_attributeNames, name);

      // if the element appeared, return the aid
      if (p != nullptr) {
        return GetAttributeId(p);
      }

      TRI_IF_FAILURE("ShaperWriteAttributeMarker") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      // write marker into wal
      triagens::wal::SlotInfoCopy slotInfo = triagens::wal::LogfileManager::instance()->allocateAndWrite(marker, false);

      if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
        // throw an exception which is caught at the end of this function
        THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
      }

      void* f TRI_UNUSED = TRI_InsertKeyAssociativeSynced(&s->_attributeIds, &aid, const_cast<void*>(slotInfo.mem), false);
      TRI_ASSERT(f == nullptr);

      // enter into the dictionaries
      f = TRI_InsertKeyAssociativeSynced(&s->_attributeNames, name, const_cast<void*>(slotInfo.mem), false);
      TRI_ASSERT(f == nullptr);
    }

    return aid;
  }
  catch (triagens::arango::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  LOG_WARNING("could not save attribute marker in log: %s", TRI_errno_string(res));

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the attribute id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyAttributeId (TRI_associative_synced_t* array, void const* key) {
  TRI_shape_aid_t const* k = static_cast<TRI_shape_aid_t const*>(key);

  return TRI_FnvHashPointer(k, sizeof(TRI_shape_aid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the attribute
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementAttributeId (TRI_associative_synced_t* array, void const* element) {
  TRI_shape_aid_t aid = GetAttributeId(element);

  return TRI_FnvHashPointer(&aid, sizeof(TRI_shape_aid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an attribute name and an attribute
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyAttributeId (TRI_associative_synced_t* array, void const* key, void const* element) {
  TRI_shape_aid_t const* k = static_cast<TRI_shape_aid_t const*>(key);
  TRI_shape_aid_t aid = GetAttributeId(element);

  return *k == aid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute name by identifier
////////////////////////////////////////////////////////////////////////////////

static char const* LookupAttributeId (TRI_shaper_t* shaper,
                                      TRI_shape_aid_t aid) {
  voc_shaper_t* s = reinterpret_cast<voc_shaper_t*>(shaper);
  std::function<char const*(void const*)> callback = [](void const* element) -> char const* {
    if (element == nullptr) {
      return nullptr;
    }
    return GetAttributeName(element);
  };

  return TRI_ProcessByKeyAssociativeSynced<char const*>(&s->_attributeIds, &aid, callback);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the shapes
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementShape (TRI_associative_synced_t* array,
                                  void const* element) {
  TRI_shape_t const* shape = static_cast<TRI_shape_t const*>(element);
  TRI_ASSERT(shape != nullptr);
  char const* s = reinterpret_cast<char const*>(shape);

  return TRI_FnvHashPointer(s + sizeof(TRI_shape_sid_t), shape->_size - sizeof(TRI_shape_sid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares shapes
////////////////////////////////////////////////////////////////////////////////

static bool EqualElementShape (TRI_associative_synced_t* array,
                               void const* left,
                               void const* right) {
  TRI_shape_t const* l = static_cast<TRI_shape_t const*>(left);
  TRI_shape_t const* r = static_cast<TRI_shape_t const*>(right);
  char const* ll = reinterpret_cast<char const*>(l);
  char const* rr = reinterpret_cast<char const*>(r);

  return (l->_size == r->_size)
    && memcmp(ll + sizeof(TRI_shape_sid_t),
              rr + sizeof(TRI_shape_sid_t),
              l->_size - sizeof(TRI_shape_sid_t)) == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a shape
/// if the function returns non-nullptr, the return value is a pointer to an
/// already existing shape and the value must not be freed
/// if the function returns nullptr, it has not found the shape and was not able
/// to create it. The value must then be freed by the caller
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_t const* FindShape (TRI_shaper_t* shaper,
                                     TRI_shape_t* shape,
                                     bool create) {
  voc_shaper_t* s = reinterpret_cast<voc_shaper_t*>(shaper);
  TRI_shape_t const* found = TRI_LookupBasicShapeShaper(shape);

  if (found == nullptr) {
    found = static_cast<TRI_shape_t const*>(TRI_LookupByElementAssociativeSynced(&s->_shapeDictionary, shape));
  }

  // shape found, free argument and return
  if (found != nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, shape);

    return found;
  }

  // not found
  if (! create) {
    return nullptr;
  }

  // get next shape id
  TRI_shape_sid_t const sid = s->_nextSid++;
  shape->_sid = sid;

  TRI_document_collection_t* document = s->_collection;

  int res = TRI_ERROR_NO_ERROR;

  try {
    triagens::wal::ShapeMarker marker(document->_vocbase->_id, document->_info._cid, shape);

    // lock the index and check the element is still missing
    MUTEX_LOCKER(s->_shapeLock);

    found = static_cast<TRI_shape_t const*>(TRI_LookupByElementAssociativeSynced(&s->_shapeDictionary, shape));

    if (found != nullptr) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, shape);

      return found;
    }

    TRI_IF_FAILURE("ShaperWriteShapeMarker") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    // write marker into wal
    triagens::wal::SlotInfoCopy slotInfo = triagens::wal::LogfileManager::instance()->allocateAndWrite(marker, false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }

    char const* m = static_cast<char const*>(slotInfo.mem) + sizeof(triagens::wal::shape_marker_t);
    TRI_shape_t const* result = reinterpret_cast<TRI_shape_t const*>(m);

    void* f = TRI_InsertKeyAssociativeSynced(&s->_shapeIds, &sid, (void*) m, false);
    if (f != nullptr) {
      LOG_ERROR("logic error when inserting shape");
    }
    TRI_ASSERT(f == nullptr);

    f = TRI_InsertElementAssociativeSynced(&s->_shapeDictionary, (void*) m, false);
    if (f != nullptr) {
      LOG_ERROR("logic error when inserting shape");
    }
    TRI_ASSERT(f == nullptr);

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, shape);
    return result;
  }
  catch (triagens::arango::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  LOG_WARNING("could not save shape marker in log: %s", TRI_errno_string(res));

  // must not free the shape here, as the caller is going to free it...

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the shape id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyShapeId (TRI_associative_synced_t* array,
                                void const* key) {
  TRI_shape_sid_t const* k = static_cast<TRI_shape_sid_t const*>(key);

  return TRI_FnvHashPointer(k, sizeof(TRI_shape_sid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the shape
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementShapeId (TRI_associative_synced_t* array,
                                    void const* element) {
  TRI_shape_t const* shape = static_cast<TRI_shape_t const*>(element);
  TRI_ASSERT(shape != nullptr);

  return TRI_FnvHashPointer(&shape->_sid, sizeof(TRI_shape_sid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a shape id and a shape
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyShapeId (TRI_associative_synced_t* array,
                             void const* key,
                             void const* element) {
  TRI_shape_sid_t const* k = static_cast<TRI_shape_sid_t const*>(key);
  TRI_shape_t const* shape = static_cast<TRI_shape_t const*>(element);
  TRI_ASSERT(shape != nullptr);

  return *k == shape->_sid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a shape by identifier
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_t const* LookupShapeId (TRI_shaper_t* shaper,
                                         TRI_shape_sid_t sid) {
  TRI_shape_t const* shape = TRI_LookupSidBasicShapeShaper(sid);

  if (shape == nullptr) {
    voc_shaper_t* s = (voc_shaper_t*) shaper;
    shape = static_cast<TRI_shape_t const*>(TRI_LookupByKeyAssociativeSynced(&s->_shapeIds, &sid));
  }

  return shape;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the accessor
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementAccessor (TRI_associative_pointer_t* array, void const* element) {
  TRI_shape_access_t const* ee = static_cast<TRI_shape_access_t const*>(element);
  uint64_t v[2];

  v[0] = ee->_sid;
  v[1] = ee->_pid;

  return TRI_FnvHashPointer(v, sizeof(v));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an accessor
////////////////////////////////////////////////////////////////////////////////

static bool EqualElementAccessor (TRI_associative_pointer_t* array, void const* left, void const* right) {
  TRI_shape_access_t const* ll = static_cast<TRI_shape_access_t const*>(left);
  TRI_shape_access_t const* rr = static_cast<TRI_shape_access_t const*>(right);

  return ll->_sid == rr->_sid && ll->_pid == rr->_pid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a shaper
////////////////////////////////////////////////////////////////////////////////

static int InitStep1VocShaper (voc_shaper_t* shaper) {
  shaper->base.findOrCreateAttributeByName = FindOrCreateAttributeByName;
  shaper->base.lookupAttributeByName       = LookupAttributeByName;
  shaper->base.lookupAttributeId           = LookupAttributeId;
  shaper->base.findShape                   = FindShape;
  shaper->base.lookupShapeId               = LookupShapeId;

  int res = TRI_InitAssociativeSynced(&shaper->_attributeNames,
                                      TRI_UNKNOWN_MEM_ZONE,
                                      HashKeyAttributeName,
                                      HashElementAttributeName,
                                      EqualKeyAttributeName,
                                      0);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  res = TRI_InitAssociativeSynced(&shaper->_attributeIds,
                                  TRI_UNKNOWN_MEM_ZONE,
                                  HashKeyAttributeId,
                                  HashElementAttributeId,
                                  EqualKeyAttributeId,
                                  0);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyAssociativeSynced(&shaper->_attributeNames);

    return res;
  }

  res = TRI_InitAssociativeSynced(&shaper->_shapeDictionary,
                                  TRI_UNKNOWN_MEM_ZONE,
                                  0,
                                  HashElementShape,
                                  0,
                                  EqualElementShape);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyAssociativeSynced(&shaper->_attributeIds);
    TRI_DestroyAssociativeSynced(&shaper->_attributeNames);

    return res;
  }

  res = TRI_InitAssociativeSynced(&shaper->_shapeIds,
                                  TRI_UNKNOWN_MEM_ZONE,
                                  HashKeyShapeId,
                                  HashElementShapeId,
                                  EqualKeyShapeId,
                                  0);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyAssociativeSynced(&shaper->_shapeDictionary);
    TRI_DestroyAssociativeSynced(&shaper->_attributeIds);
    TRI_DestroyAssociativeSynced(&shaper->_attributeNames);

    return res;
  }

  res = TRI_InitAssociativePointer(&shaper->_accessors,
                                   TRI_UNKNOWN_MEM_ZONE,
                                   0,
                                   HashElementAccessor,
                                   0,
                                   EqualElementAccessor);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyAssociativeSynced(&shaper->_shapeIds);
    TRI_DestroyAssociativeSynced(&shaper->_shapeDictionary);
    TRI_DestroyAssociativeSynced(&shaper->_attributeIds);
    TRI_DestroyAssociativeSynced(&shaper->_attributeNames);

    return res;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyAssociativePointer(&shaper->_accessors);
    TRI_DestroyAssociativeSynced(&shaper->_shapeIds);
    TRI_DestroyAssociativeSynced(&shaper->_shapeDictionary);
    TRI_DestroyAssociativeSynced(&shaper->_attributeIds);
    TRI_DestroyAssociativeSynced(&shaper->_attributeNames);

    return res;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a shaper
////////////////////////////////////////////////////////////////////////////////

static int InitStep2VocShaper (voc_shaper_t* shaper) {
  shaper->_nextAid = 1;                              // id of next attribute to hand out
  shaper->_nextSid = TRI_FirstCustomShapeIdShaper(); // id of next shape to hand out

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a shaper
////////////////////////////////////////////////////////////////////////////////

TRI_shaper_t* TRI_CreateVocShaper (TRI_vocbase_t* vocbase,
                                   TRI_document_collection_t* document) {
  voc_shaper_t* shaper = new voc_shaper_t;

  shaper->_collection = document;

  int res = TRI_InitShaper(&shaper->base, TRI_UNKNOWN_MEM_ZONE);

  if (res != TRI_ERROR_NO_ERROR) {
    delete shaper;

    return nullptr;
  }

  res = InitStep1VocShaper(shaper);

  if (res != TRI_ERROR_NO_ERROR) {
    delete shaper;

    return nullptr;
  }

  res = InitStep2VocShaper(shaper);

  if (res != TRI_ERROR_NO_ERROR) {
    delete shaper;

    return nullptr;
  }

  // and return
  return &shaper->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a shaper, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVocShaper (TRI_shaper_t* s) {
  voc_shaper_t* shaper = (voc_shaper_t*) s;

  TRI_ASSERT(shaper != nullptr);

  TRI_DestroyAssociativeSynced(&shaper->_attributeNames);
  TRI_DestroyAssociativeSynced(&shaper->_attributeIds);
  TRI_DestroyAssociativeSynced(&shaper->_shapeDictionary);
  TRI_DestroyAssociativeSynced(&shaper->_shapeIds);

  for (size_t i = 0; i < shaper->_accessors._nrAlloc; ++i) {
    TRI_shape_access_t* accessor = (TRI_shape_access_t*) shaper->_accessors._table[i];

    if (accessor != nullptr) {
      TRI_FreeShapeAccessor(accessor);
    }
  }
  TRI_DestroyAssociativePointer(&shaper->_accessors);
  TRI_DestroyShaper(s);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a shaper and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVocShaper (TRI_shaper_t* shaper) {
  TRI_DestroyVocShaper(shaper);

  delete shaper;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the shaper
////////////////////////////////////////////////////////////////////////////////

int TRI_InitVocShaper (TRI_shaper_t* s) {
  // this is a no-op now as there are no attribute weights anymore
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move a shape marker, called during compaction
////////////////////////////////////////////////////////////////////////////////

int TRI_MoveMarkerVocShaper (TRI_shaper_t* s,
                             TRI_df_marker_t* marker,
                             void* expectedOldPosition) {
  voc_shaper_t* shaper = (voc_shaper_t*) s;

  if (marker->_type == TRI_DF_MARKER_SHAPE) {
    char* p = ((char*) marker) + sizeof(TRI_df_shape_marker_t);
    TRI_shape_t* l = (TRI_shape_t*) p;
    void* f;

    MUTEX_LOCKER(shaper->_shapeLock);

    if (expectedOldPosition != nullptr) {
      char* old = static_cast<char*>(expectedOldPosition);
      void const* found = TRI_LookupByKeyAssociativeSynced(&shaper->_shapeIds, &l->_sid);

      if (found != nullptr) {
        if (old + sizeof(TRI_df_shape_marker_t) != found &&
            old + sizeof(triagens::wal::shape_marker_t) != found) {
          LOG_TRACE("got unexpected shape position");
          // do not insert if position doesn't match the expectation
          // this is done to ensure that the WAL collector doesn't insert a shape pointer
          // that has already been garbage collected by the compactor thread
          return TRI_ERROR_NO_ERROR;
        }
      }
    }

    // remove the old marker
    // and re-insert the marker with the new pointer
    f = TRI_InsertKeyAssociativeSynced(&shaper->_shapeIds, &l->_sid, l, true);

    // note: this assertion is wrong if the recovery collects the shape in the WAL and it has not been transferred
    // into the collection datafile yet
    // TRI_ASSERT(f != nullptr);
    if (f != nullptr) {
      LOG_TRACE("shape already existed in shape ids array");
    }

    // same for the shape dictionary
    // delete and re-insert
    f = TRI_InsertElementAssociativeSynced(&shaper->_shapeDictionary, l, true);

    // note: this assertion is wrong if the recovery collects the shape in the WAL and it has not been transferred
    // into the collection datafile yet
    // TRI_ASSERT(f != nullptr);
    if (f != nullptr) {
      LOG_TRACE("shape already existed in shape dictionary");
    }
  }
  else if (marker->_type == TRI_DF_MARKER_ATTRIBUTE) {
    TRI_df_attribute_marker_t* m = (TRI_df_attribute_marker_t*) marker;
    char* p = ((char*) m) + sizeof(TRI_df_attribute_marker_t);
    void* f;

    MUTEX_LOCKER(shaper->_attributeLock);
    
    if (expectedOldPosition != nullptr) {
      void const* found = TRI_LookupByKeyAssociativeSynced(&shaper->_attributeNames, p);

      if (found != nullptr && found != expectedOldPosition) {
        // do not insert if position doesn't match the expectation
        // this is done to ensure that the WAL collector doesn't insert a shape pointer
        // that has already been garbage collected by the compactor thread
        LOG_TRACE("got unexpected attribute position");
        return TRI_ERROR_NO_ERROR;
      }
    }

    // remove attribute by name (p points to new location of name, but names
    // are identical in old and new marker)
    // and re-insert same attribute with adjusted pointer
    f = TRI_InsertKeyAssociativeSynced(&shaper->_attributeNames, p, m, true);

    // note: this assertion is wrong if the recovery collects the attribute in the WAL and it has not been transferred
    // into the collection datafile yet
    // TRI_ASSERT(f != nullptr);
    if (f != nullptr) {
      LOG_TRACE("attribute already existed in attribute names dictionary");
    }

    // same for attribute ids
    // delete and re-insert same attribute with adjusted pointer
    f = TRI_InsertKeyAssociativeSynced(&shaper->_attributeIds, &m->_aid, m, true);

    // note: this assertion is wrong if the recovery collects the attribute in the WAL and it has not been transferred
    // into the collection datafile yet
    // TRI_ASSERT(f != nullptr);
    if (f != nullptr) {
      LOG_TRACE("attribute already existed in attribute ids dictionary");
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a shape, called when opening a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertShapeVocShaper (TRI_shaper_t* s,
                              TRI_df_marker_t const* marker,
                              bool warnIfDuplicate) {
  char const* p = reinterpret_cast<char const*>(marker);
  if (marker->_type == TRI_DF_MARKER_SHAPE) {
    p += sizeof(TRI_df_shape_marker_t);
  }
  else if (marker->_type == TRI_WAL_MARKER_SHAPE) {
    p += sizeof(triagens::wal::shape_marker_t);
  }
  else {
    return TRI_ERROR_INTERNAL;
  }

  TRI_shape_t* l = (TRI_shape_t*) p;

  LOG_TRACE("found shape %lu", (unsigned long) l->_sid);

  voc_shaper_t* shaper = (voc_shaper_t*) s;

  void* f;
  f = TRI_InsertElementAssociativeSynced(&shaper->_shapeDictionary, l, false);
  if (warnIfDuplicate && f != nullptr) {
    char const* name = shaper->_collection->_info._name;
#ifdef TRI_ENABLE_MAINTAINER_MODE
    LOG_ERROR("found duplicate shape in collection '%s'", name);
    TRI_ASSERT(false);
#else
    LOG_TRACE("found duplicate shape in collection '%s'", name);
#endif
  }

  f = TRI_InsertKeyAssociativeSynced(&shaper->_shapeIds, &l->_sid, l, false);
  if (warnIfDuplicate && f != nullptr) {
    char const* name = shaper->_collection->_info._name;

#ifdef TRI_ENABLE_MAINTAINER_MODE
    LOG_ERROR("found duplicate shape in collection '%s'", name);
    TRI_ASSERT(false);
#else
    LOG_TRACE("found duplicate shape in collection '%s'", name);
#endif
  }

  if (shaper->_nextSid <= l->_sid) {
    shaper->_nextSid = l->_sid + 1;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert an attribute, called when opening a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertAttributeVocShaper (TRI_shaper_t* s,
                                  TRI_df_marker_t const* marker,
                                  bool warnIfDuplicate) {
  char* name = nullptr;
  TRI_shape_aid_t aid = 0;

  if (marker->_type == TRI_DF_MARKER_ATTRIBUTE) {
    name = ((char*) marker) + sizeof(TRI_df_attribute_marker_t);
    aid = reinterpret_cast<TRI_df_attribute_marker_t const*>(marker)->_aid;
  }
  else if (marker->_type == TRI_WAL_MARKER_ATTRIBUTE) {
    name = ((char*) marker) + sizeof(triagens::wal::attribute_marker_t);
    aid = reinterpret_cast<triagens::wal::attribute_marker_t const*>(marker)->_attributeId;
  }
  else {
    return TRI_ERROR_INTERNAL;
  }
  
  TRI_ASSERT(aid != 0);
  LOG_TRACE("found attribute '%s', aid: %lu", name, (unsigned long) aid);
   
  // remove an existing temporary attribute if present
  voc_shaper_t* shaper = reinterpret_cast<voc_shaper_t*>(s);

  void* found;
  found = TRI_InsertKeyAssociativeSynced(&shaper->_attributeNames, name, (void*) marker, false);

  if (warnIfDuplicate && found != nullptr) {
    char const* cname = shaper->_collection->_info._name;

#ifdef TRI_ENABLE_MAINTAINER_MODE
    LOG_ERROR("found duplicate attribute name '%s' in collection '%s'", name, cname);
#else
    LOG_TRACE("found duplicate attribute name '%s' in collection '%s'", name, cname);
#endif
  }

  found = TRI_InsertKeyAssociativeSynced(&shaper->_attributeIds, &aid, (void*) marker, false);

  if (warnIfDuplicate && found != nullptr) {
    char const* cname = shaper->_collection->_info._name;

#ifdef TRI_ENABLE_MAINTAINER_MODE
    LOG_ERROR("found duplicate attribute id '%llu' in collection '%s'", (unsigned long long) aid, cname);
#else
    LOG_TRACE("found duplicate attribute id '%llu' in collection '%s'", (unsigned long long) aid, cname);
#endif
  }

  // no lock is necessary here as we are the only users of the shaper at this time
  if (shaper->_nextAid <= aid) {
    shaper->_nextAid = aid + 1;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an accessor for a shaper
////////////////////////////////////////////////////////////////////////////////

TRI_shape_access_t const* TRI_FindAccessorVocShaper (TRI_shaper_t* s,
                                                     TRI_shape_sid_t sid,
                                                     TRI_shape_pid_t pid) {
  TRI_shape_access_t search;
  search._sid = sid;
  search._pid = pid;

  voc_shaper_t* shaper = (voc_shaper_t*) s;

  MUTEX_LOCKER(shaper->_accessorLock);

  TRI_shape_access_t const* found = static_cast<TRI_shape_access_t const*>(TRI_LookupByElementAssociativePointer(&shaper->_accessors, &search));

  if (found == nullptr) {
    found = TRI_ShapeAccessor(&shaper->base, sid, pid);

    // TRI_ShapeAccessor can return a NULL pointer
    if (found != nullptr) {
      TRI_InsertElementAssociativePointer(&shaper->_accessors, const_cast<void*>(static_cast<void const*>(found)), true);
    }
  }

  return found;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a sub-shape
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExtractShapedJsonVocShaper (TRI_shaper_t* shaper,
                                     TRI_shaped_json_t const* document,
                                     TRI_shape_sid_t sid,
                                     TRI_shape_pid_t pid,
                                     TRI_shaped_json_t* result,
                                     TRI_shape_t const** shape) {
  TRI_shape_access_t const* accessor = TRI_FindAccessorVocShaper(shaper, document->_sid, pid);

  if (accessor == nullptr) {
    LOG_TRACE("failed to get accessor for sid %lu and path %lu",
              (unsigned long) document->_sid,
              (unsigned long) pid);

    return false;
  }

  if (accessor->_resultSid == TRI_SHAPE_ILLEGAL) {
    LOG_TRACE("expecting any object for path %lu, got nothing",
              (unsigned long) pid);

    *shape = nullptr;

    return sid == TRI_SHAPE_ILLEGAL;
  }

  *shape = shaper->lookupShapeId(shaper, accessor->_resultSid);

  if (*shape == nullptr) {
    LOG_TRACE("expecting any object for path %lu, got unknown shape id %lu",
              (unsigned long) pid,
              (unsigned long) accessor->_resultSid);

    *shape = nullptr;

    return sid == TRI_SHAPE_ILLEGAL;
  }

  if (sid != 0 && sid != accessor->_resultSid) {
    LOG_TRACE("expecting sid %lu for path %lu, got sid %lu",
              (unsigned long) sid,
              (unsigned long) pid,
              (unsigned long) accessor->_resultSid);

    return false;
  }

  bool ok = TRI_ExecuteShapeAccessor(accessor, document, result);

  if (! ok) {
    LOG_TRACE("failed to get accessor for sid %lu and path %lu",
              (unsigned long) document->_sid,
              (unsigned long) pid);

    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief temporary structure for attributes
////////////////////////////////////////////////////////////////////////////////

typedef struct attribute_entry_s {
  char*             _attribute;
  TRI_shaped_json_t _value;
}
attribute_entry_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief sort attribure names
////////////////////////////////////////////////////////////////////////////////

static int AttributeNameComparator (void const* lhs,
                                    void const* rhs) {
  attribute_entry_t const* l = static_cast<attribute_entry_t const*>(lhs);
  attribute_entry_t const* r = static_cast<attribute_entry_t const*>(rhs);

  if (l->_attribute == nullptr || r->_attribute == nullptr) {
    // error !
    return -1;
  }

  return TRI_compare_utf8(l->_attribute, r->_attribute);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a sorted vector of attributes
////////////////////////////////////////////////////////////////////////////////

static int FillAttributesVector (TRI_vector_t* vector,
                                 TRI_shaped_json_t const* shapedJson,
                                 TRI_shape_t const* shape,
                                 TRI_shaper_t const* shaper) {

  TRI_InitVector(vector, TRI_UNKNOWN_MEM_ZONE, sizeof(attribute_entry_t));

  // ...........................................................................
  // Determine the number of fixed sized values
  // ...........................................................................

  char const* charShape = (char const*) shape;

  charShape = charShape + sizeof(TRI_shape_t);
  TRI_shape_size_t fixedEntries = *((TRI_shape_size_t*)(charShape));

  // ...........................................................................
  // Determine the number of variable sized values
  // ...........................................................................

  charShape = charShape + sizeof(TRI_shape_size_t);
  TRI_shape_size_t variableEntries = *((TRI_shape_size_t*)(charShape));

  // ...........................................................................
  // It may happen that the shaped_json_array is 'empty {}'
  // ...........................................................................

  if (fixedEntries + variableEntries == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  // ...........................................................................
  // Determine the list of shape identifiers
  // ...........................................................................

  charShape = charShape + sizeof(TRI_shape_size_t);
  TRI_shape_sid_t const* sids = (TRI_shape_sid_t const*) charShape;

  charShape = charShape + (sizeof(TRI_shape_sid_t) * (fixedEntries + variableEntries));
  TRI_shape_aid_t const* aids = (TRI_shape_aid_t const*) charShape;

  charShape = charShape + (sizeof(TRI_shape_aid_t) * (fixedEntries + variableEntries));
  TRI_shape_size_t const* offsets = (TRI_shape_size_t const*) charShape;

  for (TRI_shape_size_t i = 0; i < fixedEntries; ++i) {
    attribute_entry_t attribute;

    char const* a = shaper->lookupAttributeId((TRI_shaper_t*) shaper, aids[i]);

    if (a == nullptr) {
      return TRI_ERROR_INTERNAL;
    }

    char* copy = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, a);

    if (copy == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    attribute._attribute          = copy;
    attribute._value._sid         = sids[i];
    attribute._value._data.data   = shapedJson->_data.data + offsets[i];
    attribute._value._data.length = (uint32_t) (offsets[i + 1] - offsets[i]);

    TRI_PushBackVector(vector, &attribute);
  }

  offsets = (TRI_shape_size_t const*) shapedJson->_data.data;

  for (TRI_shape_size_t i = 0; i < variableEntries; ++i) {
    attribute_entry_t attribute;

    char const* a = shaper->lookupAttributeId((TRI_shaper_t*) shaper, aids[i + fixedEntries]);

    if (a == nullptr) {
      return TRI_ERROR_INTERNAL;
    }

    char* copy = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, a);

    if (copy == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    attribute._attribute          = copy;
    attribute._value._sid         = sids[i + fixedEntries];
    attribute._value._data.data   = shapedJson->_data.data + offsets[i];
    attribute._value._data.length = (uint32_t) (offsets[i + 1] - offsets[i]);

    TRI_PushBackVector(vector, &attribute);
  }

  // sort the attributes by attribute name
  qsort(vector->_buffer, TRI_LengthVector(vector), sizeof(attribute_entry_t), AttributeNameComparator);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a vector of attributes
////////////////////////////////////////////////////////////////////////////////

static void DestroyAttributesVector (TRI_vector_t* vector) {
  size_t const n = TRI_LengthVector(vector);

  for (size_t i = 0; i < n; ++i) {
    attribute_entry_t* entry = static_cast<attribute_entry_t*>(TRI_AtVector(vector, i));

    if (entry->_attribute != nullptr) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, entry->_attribute);
    }
  }

  TRI_DestroyVector(vector);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares two shapes
///
/// You must either supply (leftDocument, leftObject) or leftShaped.
/// You must either supply (rightDocument, rightObject) or rightShaped.
////////////////////////////////////////////////////////////////////////////////

int TRI_CompareShapeTypes (char const* leftDocument,
                           TRI_shaped_sub_t* leftObject,
                           TRI_shaped_json_t const* leftShaped,
                           TRI_shaper_t* leftShaper,
                           char const* rightDocument,
                           TRI_shaped_sub_t* rightObject,
                           TRI_shaped_json_t const* rightShaped,
                           TRI_shaper_t* rightShaper) {

  TRI_shape_t const* leftShape;
  TRI_shape_t const* rightShape;
  TRI_shaped_json_t left;
  TRI_shape_type_t leftType;
  TRI_shape_type_t rightType;
  TRI_shaped_json_t leftElement;
  TRI_shaped_json_t right;
  TRI_shaped_json_t rightElement;
  int result;

  // left is either a shaped json or a shaped sub object
  if (leftDocument != nullptr) {
    left._sid = leftObject->_sid;
    left._data.length = leftObject->_length;
    left._data.data = const_cast<char*>(leftDocument) + leftObject->_offset;
  }
  else {
    left = *leftShaped;
  }

  // right is either a shaped json or a shaped sub object
  if (rightDocument != nullptr) {
    right._sid = rightObject->_sid;
    right._data.length = rightObject->_length;
    right._data.data = const_cast<char*>(rightDocument) + rightObject->_offset;
  }
  else {
    right = *rightShaped;
  }

  // get shape and type
  if (leftShaper == rightShaper && left._sid == right._sid) {
    // identical collection and shape
    leftShape = rightShape = leftShaper->lookupShapeId(leftShaper, left._sid);
  }
  else {
    // different shapes
    leftShape  = leftShaper->lookupShapeId(leftShaper, left._sid);
    rightShape = rightShaper->lookupShapeId(rightShaper, right._sid);
  }

  if (leftShape == nullptr || rightShape == nullptr) {
    LOG_ERROR("shape not found");
    TRI_ASSERT(false);
  }

  leftType   = leftShape->_type;
  rightType  = rightShape->_type;

  // ...........................................................................
  // check ALL combinations of leftType and rightType
  // ...........................................................................

  switch (leftType) {

    // .........................................................................
    // illegal type
    // .........................................................................

    case TRI_SHAPE_ILLEGAL: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: {
          return 0;
        }
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: {
          return -1;
        }
      } // end of switch (rightType)
    } // end of case TRI_SHAPE_ILLEGAL

    // .........................................................................
    // nullptr
    // .........................................................................

    case TRI_SHAPE_NULL: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: {
          return 1;
        }
        case TRI_SHAPE_NULL: {
          return 0;
        }
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: {
          return -1;
        }
      } // end of switch (rightType)
    } // end of case TRI_SHAPE_NULL

    // .........................................................................
    // BOOLEAN
    // .........................................................................

    case TRI_SHAPE_BOOLEAN: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL:
        case TRI_SHAPE_NULL: {
          return 1;
        }
        case TRI_SHAPE_BOOLEAN: {
          // check which is false and which is true!
          if ( *((TRI_shape_boolean_t*)(left._data.data)) == *((TRI_shape_boolean_t*)(right._data.data)) ) {
            return 0;
          }
          if ( *((TRI_shape_boolean_t*)(left._data.data)) < *((TRI_shape_boolean_t*)(right._data.data)) ) {
            return -1;
          }
          return 1;
        }
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: {
          return -1;
        }
      } // end of switch (rightType)
    } // end of case TRI_SHAPE_BOOLEAN

    // .........................................................................
    // NUMBER
    // .........................................................................

    case TRI_SHAPE_NUMBER: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL:
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN: {
          return 1;
        }
        case TRI_SHAPE_NUMBER: {
          // compare the numbers
          if ( *((TRI_shape_number_t*)(left._data.data)) == *((TRI_shape_number_t*)(right._data.data)) ) {
            return 0;
          }
          if ( *((TRI_shape_number_t*)(left._data.data)) < *((TRI_shape_number_t*)(right._data.data)) ) {
            return -1;
          }
          return 1;
        }
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: {
          return -1;
        }
      } // end of switch (rightType)
    } // end of case TRI_SHAPE_NUMBER

    // .........................................................................
    // STRING
    // .........................................................................

    case TRI_SHAPE_SHORT_STRING:
    case TRI_SHAPE_LONG_STRING: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL:
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER: {
          return 1;
        }
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING: {
          char* leftString;
          char* rightString;

          // compare strings
          // extract the strings
          if (leftType == TRI_SHAPE_SHORT_STRING) {
            leftString = (char*) (sizeof(TRI_shape_length_short_string_t) + left._data.data);
          }
          else {
            leftString = (char*) (sizeof(TRI_shape_length_long_string_t) + left._data.data);
          }

          if (rightType == TRI_SHAPE_SHORT_STRING) {
            rightString = (char*) (sizeof(TRI_shape_length_short_string_t) + right._data.data);
          }
          else {
            rightString = (char*) (sizeof(TRI_shape_length_long_string_t) + right._data.data);
          }

          return TRI_compare_utf8(leftString, rightString);
        }
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: {
          return -1;
        }
      } // end of switch (rightType)
    } // end of case TRI_SHAPE_LONG/SHORT_STRING


    // .........................................................................
    // HOMOGENEOUS LIST
    // .........................................................................

    case TRI_SHAPE_HOMOGENEOUS_LIST:
    case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
    case TRI_SHAPE_LIST: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL:
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING: {
          return 1;
        }
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
        case TRI_SHAPE_LIST: {
          // unfortunately recursion: check the types of all the entries
          size_t leftListLength = *((TRI_shape_length_list_t*) left._data.data);
          size_t rightListLength = *((TRI_shape_length_list_t*) right._data.data);
          size_t listLength;

          // determine the smallest list
          if (leftListLength > rightListLength) {
            listLength = rightListLength;
          }
          else {
            listLength = leftListLength;
          }

          for (size_t j = 0; j < listLength; ++j) {
            if (leftType == TRI_SHAPE_HOMOGENEOUS_LIST) {
              TRI_AtHomogeneousListShapedJson((const TRI_homogeneous_list_shape_t*)(leftShape),
                                              &left,
                                              j,
                                              &leftElement);
            }
            else if (leftType == TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
              TRI_AtHomogeneousSizedListShapedJson((const TRI_homogeneous_sized_list_shape_t*)(leftShape),
                                                   &left,
                                                   j,
                                                   &leftElement);
            }
            else {
              TRI_AtListShapedJson((const TRI_list_shape_t*)(leftShape),
                                   &left,
                                   j,
                                   &leftElement);
            }


            if (rightType == TRI_SHAPE_HOMOGENEOUS_LIST) {
              TRI_AtHomogeneousListShapedJson((const TRI_homogeneous_list_shape_t*)(rightShape),
                                              &right,
                                              j,
                                              &rightElement);
            }
            else if (rightType == TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
              TRI_AtHomogeneousSizedListShapedJson((const TRI_homogeneous_sized_list_shape_t*)(rightShape),
                                                   &right,
                                                   j,
                                                   &rightElement);
            }
            else {
              TRI_AtListShapedJson((const TRI_list_shape_t*)(rightShape),
                                   &right,
                                   j,
                                   &rightElement);
            }

            result = TRI_CompareShapeTypes(nullptr,
                                           nullptr,
                                           &leftElement,
                                           leftShaper,
                                           nullptr,
                                           nullptr,
                                           &rightElement,
                                           rightShaper);

            if (result != 0) {
              return result;
            }
          }

          // up to listLength everything matches
          if (leftListLength < rightListLength) {
            return -1;
          }
          else if (leftListLength > rightListLength) {
            return 1;
          }
          return 0;
        }


        case TRI_SHAPE_ARRAY:
        {
          return -1;
        }
      } // end of switch (rightType)
    } // end of case TRI_SHAPE_LIST ...

    // .........................................................................
    // ARRAY
    // .........................................................................

    case TRI_SHAPE_ARRAY: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL:
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
        case TRI_SHAPE_LIST: {
          return 1;
        }

        case TRI_SHAPE_ARRAY: {
          // ...................................................................
          // We are comparing a left JSON array with another JSON array on the right
          // ...................................................................

          // ...................................................................
          // generate the left and right lists sorted by attribute names
          // ...................................................................

          TRI_vector_t leftSorted;
          TRI_vector_t rightSorted;

          bool error = false;
          if (FillAttributesVector(&leftSorted, &left, leftShape, leftShaper) != TRI_ERROR_NO_ERROR) {
            error = true;
          }

          if (FillAttributesVector(&rightSorted, &right, rightShape, rightShaper) != TRI_ERROR_NO_ERROR) {
            error = true;
          }

          size_t const leftLength  = TRI_LengthVector(&leftSorted);
          size_t const rightLength = TRI_LengthVector(&rightSorted);
          size_t const numElements = (leftLength < rightLength ? leftLength : rightLength);

          result = 0;

          for (size_t i = 0; i < numElements; ++i) {
            attribute_entry_t const* l = static_cast<attribute_entry_t const*>(TRI_AtVector(&leftSorted, i));
            attribute_entry_t const* r = static_cast<attribute_entry_t const*>(TRI_AtVector(&rightSorted, i));

            result = TRI_compare_utf8(l->_attribute, r->_attribute);

            if (result != 0) {
              break;
            }

            result = TRI_CompareShapeTypes(nullptr,
                                           nullptr,
                                           &l->_value,
                                           leftShaper,
                                           nullptr,
                                           nullptr,
                                           &r->_value,
                                           rightShaper);

            if (result != 0) {
              break;
            }
          }

          if (result == 0) {
            // .................................................................
            // The comparisions above indicate that the shaped_json_arrays are equal,
            // however one more check to determine if the number of elements in the arrays
            // are equal.
            // .................................................................
            if (leftLength < rightLength) {
              result = -1;
            }
            else if (leftLength > rightLength) {
              result = 1;
            }
          }

          // clean up
          DestroyAttributesVector(&leftSorted);
          DestroyAttributesVector(&rightSorted);

          if (error) {
            return -1;
          }

          return result;
        }
      } // end of switch (rightType)
    } // end of case TRI_SHAPE_ARRAY
  } // end of switch (leftType)

  TRI_ASSERT(false);
  return 0; //shut the vc++ up
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
