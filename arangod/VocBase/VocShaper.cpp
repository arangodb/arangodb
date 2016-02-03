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

#include "VocShaper.h"
#include "Basics/Exceptions.h"
#include "Basics/fasthash.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/WriteLocker.h"
#include "Basics/associative.h"
#include "Basics/hashes.h"
#include "Basics/Logger.h"
#include "Basics/tri-strings.h"
#include "Basics/Utf8Helper.h"
#include "VocBase/document-collection.h"
#include "Wal/LogfileManager.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts an attribute id from a marker
////////////////////////////////////////////////////////////////////////////////

static inline TRI_shape_aid_t GetAttributeId(void const* marker) {
  TRI_df_marker_t const* p = static_cast<TRI_df_marker_t const*>(marker);

  if (p != nullptr) {
    if (p->_type == TRI_DF_MARKER_ATTRIBUTE) {
      return reinterpret_cast<TRI_df_attribute_marker_t const*>(p)->_aid;
    }
    if (p->_type == TRI_WAL_MARKER_ATTRIBUTE) {
      return reinterpret_cast<arangodb::wal::attribute_marker_t const*>(p)
          ->_attributeId;
    }
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts an attribute name from a marker
////////////////////////////////////////////////////////////////////////////////

static inline char const* GetAttributeName(void const* marker) {
  TRI_df_marker_t const* p = static_cast<TRI_df_marker_t const*>(marker);

  if (p != nullptr) {
    if (p->_type == TRI_DF_MARKER_ATTRIBUTE) {
      return reinterpret_cast<char const*>(p) +
             sizeof(TRI_df_attribute_marker_t);
    }
    if (p->_type == TRI_WAL_MARKER_ATTRIBUTE) {
      return reinterpret_cast<char const*>(p) +
             sizeof(arangodb::wal::attribute_marker_t);
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the attribute name of a key
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyAttributeName(TRI_associative_pointer_t*,
                                     void const* key) {
  return TRI_FnvHashString((char const*)key);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the attribute name of an element
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementAttributeName(TRI_associative_pointer_t*,
                                         void const* element) {
  return TRI_FnvHashString(GetAttributeName(element));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an attribute name and an attribute
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyAttributeName(TRI_associative_pointer_t*, void const* key,
                                  void const* element) {
  return TRI_EqualString((char const*)key, GetAttributeName(element));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the attribute id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyAttributeId(TRI_associative_pointer_t*,
                                   void const* key) {
  TRI_shape_aid_t const* k = static_cast<TRI_shape_aid_t const*>(key);
  return TRI_FnvHashPointer(k, sizeof(TRI_shape_aid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the attribute
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementAttributeId(TRI_associative_pointer_t*,
                                       void const* element) {
  TRI_shape_aid_t aid = GetAttributeId(element);
  return TRI_FnvHashPointer(&aid, sizeof(TRI_shape_aid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an attribute name and an attribute
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyAttributeId(TRI_associative_pointer_t*, void const* key,
                                void const* element) {
  TRI_shape_aid_t const* k = static_cast<TRI_shape_aid_t const*>(key);
  TRI_shape_aid_t aid = GetAttributeId(element);

  return *k == aid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the shapes
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementShape(TRI_associative_pointer_t*,
                                 void const* element) {
  auto shape = static_cast<TRI_shape_t const*>(element);
  TRI_ASSERT(shape != nullptr);
  char const* s = reinterpret_cast<char const*>(shape);
  return TRI_FnvHashPointer(
      s + sizeof(TRI_shape_sid_t),
      static_cast<size_t>(shape->_size - sizeof(TRI_shape_sid_t)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares shapes
////////////////////////////////////////////////////////////////////////////////

static bool EqualElementShape(TRI_associative_pointer_t*, void const* left,
                              void const* right) {
  auto l = static_cast<TRI_shape_t const*>(left);
  auto r = static_cast<TRI_shape_t const*>(right);
  char const* ll = reinterpret_cast<char const*>(l);
  char const* rr = reinterpret_cast<char const*>(r);

  return (l->_size == r->_size) &&
         memcmp(ll + sizeof(TRI_shape_sid_t), rr + sizeof(TRI_shape_sid_t),
                static_cast<size_t>(l->_size) - sizeof(TRI_shape_sid_t)) == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the shape id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyShapeId(TRI_associative_pointer_t*, void const* key) {
  auto k = static_cast<TRI_shape_sid_t const*>(key);
  return TRI_FnvHashPointer(k, sizeof(TRI_shape_sid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the shape
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementShapeId(TRI_associative_pointer_t*,
                                   void const* element) {
  auto shape = static_cast<TRI_shape_t const*>(element);
  TRI_ASSERT(shape != nullptr);
  return TRI_FnvHashPointer(&shape->_sid, sizeof(TRI_shape_sid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a shape id and a shape
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyShapeId(TRI_associative_pointer_t*, void const* key,
                            void const* element) {
  auto k = static_cast<TRI_shape_sid_t const*>(key);
  auto shape = static_cast<TRI_shape_t const*>(element);
  TRI_ASSERT(shape != nullptr);

  return *k == shape->_sid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the accessor
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementAccessor(TRI_associative_pointer_t*,
                                    void const* element) {
  auto ee = static_cast<TRI_shape_access_t const*>(element);
  uint64_t v[2];

  v[0] = ee->_sid;
  v[1] = ee->_pid;

  return TRI_FnvHashPointer(v, sizeof(v));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an accessor
////////////////////////////////////////////////////////////////////////////////

static bool EqualElementAccessor(TRI_associative_pointer_t*, void const* left,
                                 void const* right) {
  auto l = static_cast<TRI_shape_access_t const*>(left);
  auto r = static_cast<TRI_shape_access_t const*>(right);

  return l->_sid == r->_sid && l->_pid == r->_pid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the attribute path identifier
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashPidKeyAttributePath(TRI_associative_pointer_t*,
                                        void const* key) {
  return TRI_FnvHashPointer(key, sizeof(TRI_shape_pid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the attribute path
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashPidElementAttributePath(TRI_associative_pointer_t*,
                                            void const* element) {
  auto e = static_cast<TRI_shape_path_t const*>(element);

  return TRI_FnvHashPointer(&e->_pid, sizeof(TRI_shape_pid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an attribute path identifier and an attribute path
////////////////////////////////////////////////////////////////////////////////

static bool EqualPidKeyAttributePath(TRI_associative_pointer_t*,
                                     void const* key, void const* element) {
  auto k = static_cast<TRI_shape_pid_t const*>(key);
  auto e = static_cast<TRI_shape_path_t const*>(element);

  return *k == e->_pid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the attribute path name
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashNameKeyAttributePath(TRI_associative_pointer_t*,
                                         void const* key) {
  return TRI_FnvHashString(static_cast<char const*>(key));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the attribute path
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashNameElementAttributePath(TRI_associative_pointer_t*,
                                             void const* element) {
  char const* e = static_cast<char const*>(element);
  TRI_shape_path_t const* ee = static_cast<TRI_shape_path_t const*>(element);

  return TRI_FnvHashPointer(
      e + sizeof(TRI_shape_path_t) + ee->_aidLength * sizeof(TRI_shape_aid_t),
      ee->_nameLength - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an attribute name and an attribute
////////////////////////////////////////////////////////////////////////////////

static bool EqualNameKeyAttributePath(TRI_associative_pointer_t*,
                                      void const* key, void const* element) {
  char const* k = static_cast<char const*>(key);
  char const* e = static_cast<char const*>(element);
  TRI_shape_path_t const* ee = static_cast<TRI_shape_path_t const*>(element);

  return TRI_EqualString(k, e + sizeof(TRI_shape_path_t) +
                                ee->_aidLength * sizeof(TRI_shape_aid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a shaper
////////////////////////////////////////////////////////////////////////////////

VocShaper::VocShaper(TRI_memory_zone_t* memoryZone,
                     TRI_document_collection_t* document)
    : Shaper(),
      _memoryZone(memoryZone),
      _collection(document),
      _nextPid(1),
      _nextAid(1),  // id of next attribute to hand out
      _nextSid(Shaper::firstCustomShapeId()) {  // id of next shape to hand out

  TRI_InitAssociativePointer(&_attributeNames, TRI_UNKNOWN_MEM_ZONE,
                             HashKeyAttributeName, HashElementAttributeName,
                             EqualKeyAttributeName, 0);

  TRI_InitAssociativePointer(&_attributeIds, TRI_UNKNOWN_MEM_ZONE,
                             HashKeyAttributeId, HashElementAttributeId,
                             EqualKeyAttributeId, 0);

  TRI_InitAssociativePointer(&_shapeDictionary, TRI_UNKNOWN_MEM_ZONE, 0,
                             HashElementShape, 0, EqualElementShape);

  TRI_InitAssociativePointer(&_shapeIds, TRI_UNKNOWN_MEM_ZONE, HashKeyShapeId,
                             HashElementShapeId, EqualKeyShapeId, 0);

  for (size_t i = 0; i < NUM_SHAPE_ACCESSORS; ++i) {
    TRI_InitAssociativePointer(&_accessors[i], TRI_UNKNOWN_MEM_ZONE, 0,
                               HashElementAccessor, 0, EqualElementAccessor);
  }

  TRI_InitAssociativePointer(
      &_attributePathsByName, _memoryZone, HashNameKeyAttributePath,
      HashNameElementAttributePath, EqualNameKeyAttributePath, 0);

  TRI_InitAssociativePointer(
      &_attributePathsByPid, _memoryZone, HashPidKeyAttributePath,
      HashPidElementAttributePath, EqualPidKeyAttributePath, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a shaper
////////////////////////////////////////////////////////////////////////////////

VocShaper::~VocShaper() {
  size_t const n = _attributePathsByName._nrAlloc;

  // only free pointers in attributePathsByName
  // (attributePathsByPid contains the same pointers!)
  for (size_t i = 0; i < n; ++i) {
    void* data = _attributePathsByName._table[i];

    if (data != nullptr) {
      TRI_Free(_memoryZone, data);
    }
  }

  TRI_DestroyAssociativePointer(&_attributePathsByName);
  TRI_DestroyAssociativePointer(&_attributePathsByPid);

  TRI_DestroyAssociativePointer(&_attributeNames);
  TRI_DestroyAssociativePointer(&_attributeIds);
  TRI_DestroyAssociativePointer(&_shapeDictionary);
  TRI_DestroyAssociativePointer(&_shapeIds);

  for (size_t i = 0; i < NUM_SHAPE_ACCESSORS; ++i) {
    for (size_t j = 0; j < _accessors[i]._nrAlloc; ++j) {
      auto accessor = static_cast<TRI_shape_access_t*>(_accessors[i]._table[j]);

      if (accessor != nullptr) {
        TRI_FreeShapeAccessor(accessor);
      }
    }
    TRI_DestroyAssociativePointer(&_accessors[i]);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a shape by identifier
////////////////////////////////////////////////////////////////////////////////

TRI_shape_t const* VocShaper::lookupShapeId(TRI_shape_sid_t sid) {
  TRI_shape_t const* shape = Shaper::lookupSidBasicShape(sid);

  if (shape == nullptr) {
    READ_LOCKER(readLocker, _shapeIdsLock);

    shape = static_cast<TRI_shape_t const*>(
        TRI_LookupByKeyAssociativePointer(&_shapeIds, &sid));
  }

  return shape;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute name by identifier
////////////////////////////////////////////////////////////////////////////////

char const* VocShaper::lookupAttributeId(TRI_shape_aid_t aid) {
  {
    READ_LOCKER(readLocker, _attributeIdsLock);

    auto element = static_cast<void const*>(
        TRI_LookupByKeyAssociativePointer(&_attributeIds, &aid));

    if (element != nullptr) {
      return GetAttributeName(element);
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute path by identifier
////////////////////////////////////////////////////////////////////////////////

TRI_shape_path_t const* VocShaper::lookupAttributePathByPid(
    TRI_shape_pid_t pid) {
  READ_LOCKER(readLocker, _attributePathsByPidLock);

  return static_cast<TRI_shape_path_t const*>(
      TRI_LookupByKeyAssociativePointer(&_attributePathsByPid, &pid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an attribute path by identifier
////////////////////////////////////////////////////////////////////////////////

TRI_shape_pid_t VocShaper::findOrCreateAttributePathByName(char const* name) {
  TRI_shape_path_t const* path = findShapePathByName(name, true);

  return path == nullptr ? 0 : path->_pid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute path by identifier
////////////////////////////////////////////////////////////////////////////////

TRI_shape_pid_t VocShaper::lookupAttributePathByName(char const* name) {
  TRI_shape_path_t const* path = findShapePathByName(name, false);

  return path == nullptr ? 0 : path->_pid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the attribute name for an attribute path
////////////////////////////////////////////////////////////////////////////////

char const* VocShaper::attributeNameShapePid(TRI_shape_pid_t pid) {
  TRI_shape_path_t const* path = lookupAttributePathByPid(pid);

  if (path == nullptr) {
    return nullptr;
  }

  char const* e = (char const*)path;

  return e + sizeof(TRI_shape_path_t) +
         path->_aidLength * sizeof(TRI_shape_aid_t);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute identifier by name
////////////////////////////////////////////////////////////////////////////////

TRI_shape_aid_t VocShaper::lookupAttributeByName(char const* name) {
  TRI_ASSERT(name != nullptr);

  {
    READ_LOCKER(readLocker, _attributeNamesLock);

    auto element = static_cast<void const*>(
        TRI_LookupByKeyAssociativePointer(&_attributeNames, name));

    if (element != nullptr) {
      return GetAttributeId(element);
    }
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds or creates an attribute identifier by name
////////////////////////////////////////////////////////////////////////////////

TRI_shape_aid_t VocShaper::findOrCreateAttributeByName(char const* name) {
  // check if the attribute exists
  TRI_shape_aid_t aid = lookupAttributeByName(name);

  if (aid != 0) {
    // yes
    return aid;
  }

  // increase attribute id value
  aid = _nextAid++;

  int res = TRI_ERROR_NO_ERROR;
  TRI_document_collection_t* document = _collection;

  try {
    arangodb::wal::AttributeMarker marker(
        document->_vocbase->_id, document->_info.id(), aid, std::string(name));

    // lock the index and check that the element is still missing
    {
      MUTEX_LOCKER(mutexLocker, _attributeCreateLock);

      void const* p;
      {
        READ_LOCKER(readLocker, _attributeNamesLock);
        p = TRI_LookupByKeyAssociativePointer(&_attributeNames, name);
      }

      // if the element appeared, return the aid
      if (p != nullptr) {
        return GetAttributeId(p);
      }

      TRI_IF_FAILURE("ShaperWriteAttributeMarker") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      {
        // make room for one more element
        WRITE_LOCKER(writeLocker, _attributeIdsLock);
        if (!TRI_ReserveAssociativePointer(&_attributeIds, 1)) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
        }
      }

      {
        // make room for one more element
        WRITE_LOCKER(writeLocker, _attributeNamesLock);
        if (!TRI_ReserveAssociativePointer(&_attributeNames, 1)) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
        }
      }

      // write marker into wal
      arangodb::wal::SlotInfoCopy slotInfo =
          arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker,
                                                                      false);

      if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
        // throw an exception which is caught at the end of this function
        THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
      }

      void* TRI_UNUSED f;
      {
        WRITE_LOCKER(writeLocker, _attributeIdsLock);
        f = TRI_InsertKeyAssociativePointer(
            &_attributeIds, &aid, const_cast<void*>(slotInfo.mem), false);
      }
      TRI_ASSERT(f == nullptr);

      // enter into the dictionaries
      {
        WRITE_LOCKER(writeLocker, _attributeNamesLock);
        f = TRI_InsertKeyAssociativePointer(
            &_attributeNames, name, const_cast<void*>(slotInfo.mem), false);
      }
      TRI_ASSERT(f == nullptr);
    }

    return aid;
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  LOG(WARN) << "could not save attribute marker in log: " << TRI_errno_string(res);

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a shape
/// if the function returns non-nullptr, the return value is a pointer to an
/// already existing shape and the value must not be freed
/// if the function returns nullptr, it has not found the shape and was not able
/// to create it. The value must then be freed by the caller
////////////////////////////////////////////////////////////////////////////////

TRI_shape_t const* VocShaper::findShape(TRI_shape_t* shape, bool create) {
  TRI_shape_t const* found = Shaper::lookupBasicShape(shape);

  if (found == nullptr) {
    READ_LOCKER(readLocker, _shapeDictionaryLock);
    found = static_cast<TRI_shape_t const*>(
        TRI_LookupByElementAssociativePointer(&_shapeDictionary, shape));
  }

  // shape found, free argument and return
  if (found != nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, shape);

    return found;
  }

  // not found
  if (!create) {
    return nullptr;
  }

  // get next shape id
  TRI_shape_sid_t const sid = _nextSid++;
  shape->_sid = sid;

  TRI_document_collection_t* document = _collection;

  int res = TRI_ERROR_NO_ERROR;

  try {
    arangodb::wal::ShapeMarker marker(document->_vocbase->_id,
                                      document->_info.id(), shape);

    // lock the index and check the element is still missing
    MUTEX_LOCKER(mutexLocker, _shapeCreateLock);

    {
      READ_LOCKER(readLocker, _shapeDictionaryLock);
      found = static_cast<TRI_shape_t const*>(
          TRI_LookupByElementAssociativePointer(&_shapeDictionary, shape));
    }

    if (found != nullptr) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, shape);

      return found;
    }

    TRI_IF_FAILURE("ShaperWriteShapeMarker") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    {
      // make room for one more element
      WRITE_LOCKER(writeLocker, _shapeIdsLock);
      if (!TRI_ReserveAssociativePointer(&_shapeIds, 1)) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
    }

    {
      // make room for one more element
      WRITE_LOCKER(writeLocker, _shapeDictionaryLock);
      if (!TRI_ReserveAssociativePointer(&_shapeDictionary, 1)) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
    }

    // write marker into wal
    arangodb::wal::SlotInfoCopy slotInfo =
        arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker,
                                                                    false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }

    char const* m = static_cast<char const*>(slotInfo.mem) +
                    sizeof(arangodb::wal::shape_marker_t);

    TRI_ASSERT(m != nullptr);
    TRI_shape_t const* result = reinterpret_cast<TRI_shape_t const*>(m);

    {
      WRITE_LOCKER(writeLocker, _shapeIdsLock);
      void* f =
          TRI_InsertKeyAssociativePointer(&_shapeIds, &sid, (void*)m, false);

      if (f != nullptr) {
        LOG(ERR) << "logic error when inserting shape into id dictionary";
      }

      TRI_ASSERT(f == nullptr);  // will abort here
    }

    {
      WRITE_LOCKER(writeLocker, _shapeDictionaryLock);
      void* f = TRI_InsertElementAssociativePointer(&_shapeDictionary, (void*)m,
                                                    false);

      if (f != nullptr) {
        LOG(ERR) << "logic error when inserting shape into dictionary";
      }

      TRI_ASSERT(f == nullptr);  // will abort here
    }

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, shape);
    return result;
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  LOG(WARN) << "could not save shape marker in log: " << TRI_errno_string(res);

  // must not free the shape here, as the caller is going to free it...

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move a shape marker, called during compaction
////////////////////////////////////////////////////////////////////////////////

int VocShaper::moveMarker(TRI_df_marker_t* marker, void* expectedOldPosition) {
  if (marker->_type == TRI_DF_MARKER_SHAPE) {
    char* p = ((char*)marker) + sizeof(TRI_df_shape_marker_t);
    TRI_shape_t* l = (TRI_shape_t*)p;

    MUTEX_LOCKER(mutexLocker, _shapeCreateLock);

    if (expectedOldPosition != nullptr) {
      char* old = static_cast<char*>(expectedOldPosition);
      void const* found;
      {
        READ_LOCKER(readLocker, _shapeIdsLock);
        found = TRI_LookupByKeyAssociativePointer(&_shapeIds, &l->_sid);
      }

      if (found != nullptr) {
        if (old + sizeof(TRI_df_shape_marker_t) != found &&
            old + sizeof(arangodb::wal::shape_marker_t) != found) {
          LOG(TRACE) << "got unexpected shape position";
          // do not insert if position doesn't match the expectation
          // this is done to ensure that the WAL collector doesn't insert a
          // shape pointer
          // that has already been garbage collected by the compactor thread
          return TRI_ERROR_NO_ERROR;
        }
      }
    }

    // remove the old marker
    // and re-insert the marker with the new pointer
    void* f;
    {
      WRITE_LOCKER(writeLocker, _shapeIdsLock);
      f = TRI_InsertKeyAssociativePointer(&_shapeIds, &l->_sid, l, true);
    }

    // note: this assertion is wrong if the recovery collects the shape in the
    // WAL and it has not been transferred
    // into the collection datafile yet
    // TRI_ASSERT(f != nullptr);
    if (f != nullptr) {
      LOG(TRACE) << "shape already existed in shape ids array";
    }

    // same for the shape dictionary
    // delete and re-insert
    {
      WRITE_LOCKER(writeLocker, _shapeDictionaryLock);
      f = TRI_InsertElementAssociativePointer(&_shapeDictionary, l, true);
    }

    // note: this assertion is wrong if the recovery collects the shape in the
    // WAL and it has not been transferred
    // into the collection datafile yet
    // TRI_ASSERT(f != nullptr);
    if (f != nullptr) {
      LOG(TRACE) << "shape already existed in shape dictionary";
    }
  } else if (marker->_type == TRI_DF_MARKER_ATTRIBUTE) {
    TRI_df_attribute_marker_t* m = (TRI_df_attribute_marker_t*)marker;
    char* p = ((char*)m) + sizeof(TRI_df_attribute_marker_t);

    MUTEX_LOCKER(mutexLocker, _attributeCreateLock);

    if (expectedOldPosition != nullptr) {
      void const* found;
      {
        READ_LOCKER(readLocker, _attributeNamesLock);
        found = TRI_LookupByKeyAssociativePointer(&_attributeNames, p);
      }

      if (found != nullptr && found != expectedOldPosition) {
        // do not insert if position doesn't match the expectation
        // this is done to ensure that the WAL collector doesn't insert a shape
        // pointer
        // that has already been garbage collected by the compactor thread
        LOG(TRACE) << "got unexpected attribute position";
        return TRI_ERROR_NO_ERROR;
      }
    }

    // remove attribute by name (p points to new location of name, but names
    // are identical in old and new marker)
    // and re-insert same attribute with adjusted pointer
    void* f;
    {
      WRITE_LOCKER(writeLocker, _attributeNamesLock);
      f = TRI_InsertKeyAssociativePointer(&_attributeNames, p, m, true);
    }

    // note: this assertion is wrong if the recovery collects the attribute in
    // the WAL and it has not been transferred
    // into the collection datafile yet
    // TRI_ASSERT(f != nullptr);
    if (f != nullptr) {
      LOG(TRACE) << "attribute already existed in attribute names dictionary";
    }

    // same for attribute ids
    // delete and re-insert same attribute with adjusted pointer
    {
      WRITE_LOCKER(writeLocker, _attributeIdsLock);
      f = TRI_InsertKeyAssociativePointer(&_attributeIds, &m->_aid, m, true);
    }

    // note: this assertion is wrong if the recovery collects the attribute in
    // the WAL and it has not been transferred
    // into the collection datafile yet
    // TRI_ASSERT(f != nullptr);
    if (f != nullptr) {
      LOG(TRACE) << "attribute already existed in attribute ids dictionary";
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a shape, called when opening a collection
////////////////////////////////////////////////////////////////////////////////

int VocShaper::insertShape(TRI_df_marker_t const* marker,
                           bool warnIfDuplicate) {
  char const* p = reinterpret_cast<char const*>(marker);

  if (marker->_type == TRI_DF_MARKER_SHAPE) {
    p += sizeof(TRI_df_shape_marker_t);
  } else if (marker->_type == TRI_WAL_MARKER_SHAPE) {
    p += sizeof(arangodb::wal::shape_marker_t);
  } else {
    return TRI_ERROR_INTERNAL;
  }

  TRI_shape_t* l = (TRI_shape_t*)p;

  LOG(TRACE) << "found shape " << l->_sid;

  MUTEX_LOCKER(mutexLocker, _shapeCreateLock);

  void* f;
  {
    WRITE_LOCKER(writeLocker, _shapeDictionaryLock);
    f = TRI_InsertElementAssociativePointer(&_shapeDictionary, l, false);
  }

  if (warnIfDuplicate && f != nullptr) {
    std::string name = _collection->_info.name();
    bool const isIdentical = EqualElementShape(nullptr, f, l);
    if (isIdentical) {
      // duplicate shape, but with identical content. simply ignore it
      LOG(TRACE) << "found duplicate shape markers for id " << l->_sid << " in collection '" << name.c_str() << "' in shape dictionary";
    } else {
      LOG(ERR) << "found heterogenous shape markers for id " << l->_sid << " in collection '" << name.c_str() << "' in shape dictionary";
#ifdef TRI_ENABLE_MAINTAINER_MODE
      TRI_ASSERT(false);
#endif
    }
  }

  {
    WRITE_LOCKER(writeLocker, _shapeIdsLock);
    f = TRI_InsertKeyAssociativePointer(&_shapeIds, &l->_sid, l, false);
  }

  if (warnIfDuplicate && f != nullptr) {
    std::string name = _collection->_info.name();
    bool const isIdentical = EqualElementShape(nullptr, f, l);

    if (isIdentical) {
      // duplicate shape, but with identical content. simply ignore it
      LOG(TRACE) << "found duplicate shape markers for id " << l->_sid << " in collection '" << name.c_str() << "' in shape ids table";
    } else {
      LOG(ERR) << "found heterogenous shape markers for id " << l->_sid << " in collection '" << name.c_str() << "' in shape ids table";
#ifdef TRI_ENABLE_MAINTAINER_MODE
      TRI_ASSERT(false);
#endif
    }
  }

  // no lock is necessary here as we are the only users of the shaper at this
  // time (opening the collection)
  if (_nextSid <= l->_sid) {
    _nextSid = l->_sid + 1;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert an attribute, called when opening a collection
////////////////////////////////////////////////////////////////////////////////

int VocShaper::insertAttribute(TRI_df_marker_t const* marker,
                               bool warnIfDuplicate) {
  char* name = nullptr;
  TRI_shape_aid_t aid = 0;

  if (marker->_type == TRI_DF_MARKER_ATTRIBUTE) {
    name = ((char*)marker) + sizeof(TRI_df_attribute_marker_t);
    aid = reinterpret_cast<TRI_df_attribute_marker_t const*>(marker)->_aid;
  } else if (marker->_type == TRI_WAL_MARKER_ATTRIBUTE) {
    name = ((char*)marker) + sizeof(arangodb::wal::attribute_marker_t);
    aid = reinterpret_cast<arangodb::wal::attribute_marker_t const*>(marker)
              ->_attributeId;
  } else {
    return TRI_ERROR_INTERNAL;
  }

  TRI_ASSERT(aid != 0);
  LOG(TRACE) << "found attribute '" << name << "', aid: " << aid;

  // remove an existing temporary attribute if present
  MUTEX_LOCKER(mutexLocker, _attributeCreateLock);

  void* found;
  {
    WRITE_LOCKER(writeLocker, _attributeNamesLock);
    found = TRI_InsertKeyAssociativePointer(&_attributeNames, name,
                                            (void*)marker, false);
  }

  if (warnIfDuplicate && found != nullptr) {
    std::string cname = _collection->_info.name();
    bool const isIdentical = (TRI_EqualString(name, GetAttributeName(found)) &&
                              aid == GetAttributeId(found));

    if (isIdentical) {
      // duplicate attribute, but with identical content. simply ignore it
      LOG(TRACE) << "found duplicate attribute name '" << name << "' in collection '" << cname.c_str() << "'";
    } else {
      LOG(ERR) << "found heterogenous attribute name '" << name << "' in collection '" << cname.c_str() << "'";
    }
  }

  {
    WRITE_LOCKER(writeLocker, _attributeIdsLock);
    found = TRI_InsertKeyAssociativePointer(&_attributeIds, &aid, (void*)marker,
                                            false);
  }

  if (warnIfDuplicate && found != nullptr) {
    std::string cname = _collection->_info.name();
    bool const isIdentical = TRI_EqualString(name, GetAttributeName(found));

    if (isIdentical) {
      // duplicate attribute, but with identical content. simply ignore it
      LOG(TRACE) << "found duplicate attribute id '" << aid << "' in collection '" << cname.c_str() << "'";
    } else {
      LOG(ERR) << "found heterogenous attribute id '" << aid << "' in collection '" << cname.c_str() << "'";
    }
  }

  // no lock is necessary here as we are the only users of the shaper at this
  // time (opening the collection)
  if (_nextAid <= aid) {
    _nextAid = aid + 1;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an accessor
////////////////////////////////////////////////////////////////////////////////

TRI_shape_access_t const* VocShaper::findAccessor(TRI_shape_sid_t sid,
                                                  TRI_shape_pid_t pid) {
  TRI_shape_access_t search = {sid, pid, 0, nullptr};

  size_t const i = static_cast<size_t>(
      fasthash64(&sid, sizeof(TRI_shape_sid_t),
                 fasthash64(&pid, sizeof(TRI_shape_pid_t), 0x87654321)) %
      NUM_SHAPE_ACCESSORS);

  TRI_shape_access_t const* found = nullptr;
  {
    READ_LOCKER(readLocker, _accessorLock[i]);

    found = static_cast<TRI_shape_access_t const*>(
        TRI_LookupByElementAssociativePointer(&_accessors[i], &search));

    if (found != nullptr) {
      return found;
    }
  }

  // not found... time for us to create the accessor ourselves!
  TRI_shape_access_t* accessor = TRI_ShapeAccessor(this, sid, pid);

  // TRI_ShapeAccessor can return a NULL pointer
  if (accessor == nullptr) {
    return nullptr;
  }

  // acquire the write-lock and try to insert our own accessor

  {
    WRITE_LOCKER(writeLocker, _accessorLock[i]);
    found = static_cast<TRI_shape_access_t const*>(
        TRI_InsertElementAssociativePointer(
            &_accessors[i],
            const_cast<void*>(static_cast<void const*>(accessor)), false));
  }

  if (found != nullptr) {
    // someone else inserted the same accessor in the period after we release
    // the read-lock
    // but before we acquired the write-lock
    // this is ok, and we can return the concurrently built accessor now
    TRI_FreeShapeAccessor(accessor);

    return found;
  }

  return const_cast<TRI_shape_access_t const*>(accessor);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a sub-shape
////////////////////////////////////////////////////////////////////////////////

bool VocShaper::extractShapedJson(TRI_shaped_json_t const* document,
                                  TRI_shape_sid_t sid, TRI_shape_pid_t pid,
                                  TRI_shaped_json_t* result,
                                  TRI_shape_t const** shape) {
  TRI_shape_access_t const* accessor = findAccessor(document->_sid, pid);

  if (accessor == nullptr) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
    LOG(TRACE) << "failed to get accessor for sid " << document->_sid << " and path " << pid;
#endif
    return false;
  }

  if (accessor->_resultSid == TRI_SHAPE_ILLEGAL) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
    LOG(TRACE) << "expecting any object for path " << pid << ", got nothing";
#endif
    *shape = nullptr;

    return sid == TRI_SHAPE_ILLEGAL;
  }

  *shape = lookupShapeId(accessor->_resultSid);

  if (*shape == nullptr) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
    LOG(TRACE) << "expecting any object for path " << pid << ", got unknown shape id " << accessor->_resultSid;
#endif
    *shape = nullptr;

    return sid == TRI_SHAPE_ILLEGAL;
  }

  if (sid != 0 && sid != accessor->_resultSid) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
    LOG(TRACE) << "expecting sid " << sid << " for path " << pid << ", got sid " << accessor->_resultSid;
#endif
    return false;
  }

  bool ok = TRI_ExecuteShapeAccessor(accessor, document, result);

  if (!ok) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
    LOG(TRACE) << "failed to get accessor for sid " << document->_sid << " and path " << pid;
#endif
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a shape path by identifier
////////////////////////////////////////////////////////////////////////////////

TRI_shape_path_t const* VocShaper::findShapePathByName(char const* name,
                                                       bool create) {
  char* buffer;
  char* end;
  char* prev;
  char* ptr;

  TRI_ASSERT(name != nullptr);

  void const* p;
  {
    READ_LOCKER(readLocker, _attributePathsByNameLock);
    p = TRI_LookupByKeyAssociativePointer(&_attributePathsByName, name);
  }

  if (p != nullptr) {
    return (TRI_shape_path_t const*)p;
  }

  // create an attribute path
  size_t len = strlen(name);

  // lock the index and check that the element is still missing
  MUTEX_LOCKER(mutexLocker, _attributePathsCreateLock);

  // if the element appeared, return the pid
  {
    READ_LOCKER(readLocker, _attributePathsByNameLock);
    p = TRI_LookupByKeyAssociativePointer(&_attributePathsByName, name);
  }

  if (p != nullptr) {
    return (TRI_shape_path_t const*)p;
  }

  // split path into attribute pieces
  size_t count = 0;
  TRI_shape_aid_t* aids = static_cast<TRI_shape_aid_t*>(
      TRI_Allocate(_memoryZone, len * sizeof(TRI_shape_aid_t), false));

  if (aids == nullptr) {
    LOG(ERR) << "out of memory in shaper";
    return nullptr;
  }

  buffer = ptr = TRI_DuplicateString(_memoryZone, name, len);

  if (buffer == nullptr) {
    TRI_Free(_memoryZone, aids);
    LOG(ERR) << "out of memory in shaper";
    return nullptr;
  }

  end = buffer + len + 1;
  prev = buffer;

  for (; ptr < end; ++ptr) {
    if (*ptr == '.' || *ptr == '\0') {
      *ptr = '\0';

      if (ptr != prev) {
        if (create) {
          aids[count++] = findOrCreateAttributeByName(prev);
        } else {
          aids[count] = lookupAttributeByName(prev);

          if (aids[count] == 0) {
            TRI_FreeString(_memoryZone, buffer);
            TRI_Free(_memoryZone, aids);
            return nullptr;
          }

          ++count;
        }
      }

      prev = ptr + 1;
    }
  }

  TRI_FreeString(_memoryZone, buffer);

  // create element
  size_t total =
      sizeof(TRI_shape_path_t) + (len + 1) + (count * sizeof(TRI_shape_aid_t));
  TRI_shape_path_t* result =
      static_cast<TRI_shape_path_t*>(TRI_Allocate(_memoryZone, total, false));

  if (result == nullptr) {
    TRI_Free(_memoryZone, aids);
    LOG(ERR) << "out of memory in shaper";
    return nullptr;
  }

  result->_pid = _nextPid++;
  result->_nameLength = (uint32_t)len + 1;
  result->_aidLength = count;

  memcpy(((char*)result) + sizeof(TRI_shape_path_t), aids,
         count * sizeof(TRI_shape_aid_t));
  memcpy(((char*)result) + sizeof(TRI_shape_path_t) +
             count * sizeof(TRI_shape_aid_t),
         name, len + 1);

  TRI_Free(_memoryZone, aids);

  {
    // make room for one more element
    WRITE_LOCKER(writeLocker, _attributePathsByNameLock);
    if (!TRI_ReserveAssociativePointer(&_attributePathsByName, 1)) {
      TRI_Free(_memoryZone, result);
      return nullptr;
    }
  }

  {
    // make room for one more element
    WRITE_LOCKER(writeLocker, _attributePathsByPidLock);
    if (!TRI_ReserveAssociativePointer(&_attributePathsByPid, 1)) {
      TRI_Free(_memoryZone, result);
      return nullptr;
    }
  }

  {
    WRITE_LOCKER(writeLocker, _attributePathsByNameLock);
    void const* f = TRI_InsertKeyAssociativePointer(&_attributePathsByName,
                                                    name, result, false);

    if (f != nullptr) {
      LOG(WARN) << "duplicate shape path " << result->_pid;
    }

    TRI_ASSERT(f == nullptr);  // will abort here
  }

  {
    WRITE_LOCKER(writeLocker, _attributePathsByPidLock);
    void const* f = TRI_InsertKeyAssociativePointer(
        &_attributePathsByPid, &result->_pid, result, false);

    if (f != nullptr) {
      LOG(WARN) << "duplicate shape path " << result->_pid;
    }

    TRI_ASSERT(f == nullptr);  // will abort here
  }

  // return pid
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief temporary structure for attributes
////////////////////////////////////////////////////////////////////////////////

typedef struct attribute_entry_s {
  char* _attribute;
  TRI_shaped_json_t _value;
} attribute_entry_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief sort attribure names
////////////////////////////////////////////////////////////////////////////////

static int AttributeNameComparator(void const* lhs, void const* rhs) {
  auto l = static_cast<attribute_entry_t const*>(lhs);
  auto r = static_cast<attribute_entry_t const*>(rhs);

  if (l->_attribute == nullptr || r->_attribute == nullptr) {
    // error !
    return -1;
  }

  return TRI_compare_utf8(l->_attribute, r->_attribute);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a sorted vector of attributes
////////////////////////////////////////////////////////////////////////////////

static int FillAttributesVector(TRI_vector_t* vector,
                                TRI_shaped_json_t const* shapedJson,
                                TRI_shape_t const* shape, VocShaper* shaper) {
  TRI_InitVector(vector, TRI_UNKNOWN_MEM_ZONE, sizeof(attribute_entry_t));

  // ...........................................................................
  // Determine the number of fixed sized values
  // ...........................................................................

  char const* charShape = (char const*)shape;

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
  TRI_shape_sid_t const* sids = (TRI_shape_sid_t const*)charShape;

  charShape =
      charShape + (sizeof(TRI_shape_sid_t) * (fixedEntries + variableEntries));
  TRI_shape_aid_t const* aids = (TRI_shape_aid_t const*)charShape;

  charShape =
      charShape + (sizeof(TRI_shape_aid_t) * (fixedEntries + variableEntries));
  TRI_shape_size_t const* offsets = (TRI_shape_size_t const*)charShape;

  for (TRI_shape_size_t i = 0; i < fixedEntries; ++i) {
    char const* a = shaper->lookupAttributeId(aids[i]);

    if (a == nullptr) {
      return TRI_ERROR_INTERNAL;
    }

    char* copy = TRI_DuplicateString(TRI_UNKNOWN_MEM_ZONE, a);

    if (copy == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    attribute_entry_t attribute;
    attribute._attribute = copy;
    attribute._value._sid = sids[i];
    attribute._value._data.data = shapedJson->_data.data + offsets[i];
    attribute._value._data.length = (uint32_t)(offsets[i + 1] - offsets[i]);

    TRI_PushBackVector(vector, &attribute);
  }

  offsets = (TRI_shape_size_t const*)shapedJson->_data.data;

  for (TRI_shape_size_t i = 0; i < variableEntries; ++i) {
    char const* a = shaper->lookupAttributeId(aids[i + fixedEntries]);

    if (a == nullptr) {
      return TRI_ERROR_INTERNAL;
    }

    char* copy = TRI_DuplicateString(TRI_UNKNOWN_MEM_ZONE, a);

    if (copy == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    attribute_entry_t attribute;
    attribute._attribute = copy;
    attribute._value._sid = sids[i + fixedEntries];
    attribute._value._data.data = shapedJson->_data.data + offsets[i];
    attribute._value._data.length = (uint32_t)(offsets[i + 1] - offsets[i]);

    TRI_PushBackVector(vector, &attribute);
  }

  // sort the attributes by attribute name
  qsort(vector->_buffer, TRI_LengthVector(vector), sizeof(attribute_entry_t),
        AttributeNameComparator);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a vector of attributes
////////////////////////////////////////////////////////////////////////////////

static void DestroyAttributesVector(TRI_vector_t* vector) {
  size_t const n = TRI_LengthVector(vector);

  for (size_t i = 0; i < n; ++i) {
    attribute_entry_t* entry =
        static_cast<attribute_entry_t*>(TRI_AtVector(vector, i));

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

int TRI_CompareShapeTypes(char const* leftDocument,
                          TRI_shaped_sub_t const* leftObject,
                          TRI_shaped_json_t const* leftShaped,
                          VocShaper* leftShaper, char const* rightDocument,
                          TRI_shaped_sub_t const* rightObject,
                          TRI_shaped_json_t const* rightShaped,
                          VocShaper* rightShaper) {
  TRI_shape_t const* rightShape;
  TRI_shaped_json_t left;
  TRI_shaped_json_t leftElement;
  TRI_shaped_json_t right;
  TRI_shaped_json_t rightElement;

  // left is either a shaped json or a shaped sub object
  if (leftDocument != nullptr) {
    TRI_ASSERT(leftObject != nullptr);
    left._sid = leftObject->_sid;
    TRI_InspectShapedSub(leftObject, leftDocument, left);
  } else {
    left = *leftShaped;
  }

  // right is either a shaped json or a shaped sub object
  if (rightDocument != nullptr) {
    TRI_ASSERT(rightObject != nullptr);
    right._sid = rightObject->_sid;
    TRI_InspectShapedSub(rightObject, rightDocument, right);
  } else {
    right = *rightShaped;
  }

  // get left shape and type
  TRI_shape_t const* leftShape = leftShaper->lookupShapeId(left._sid);

  // get right shape and type
  if (leftShaper == rightShaper && left._sid == right._sid) {
    if (left._sid == BasicShapes::TRI_SHAPE_SID_ILLEGAL) {
      // Both sides have shape_sid illegal
      return 0;
    }
    // identical collection and shape
    rightShape = leftShape;
  } else {
    // different shapes
    rightShape = rightShaper->lookupShapeId(right._sid);
  }

  if (left._sid == BasicShapes::TRI_SHAPE_SID_ILLEGAL) {
    return -1;
  }
  if (right._sid == BasicShapes::TRI_SHAPE_SID_ILLEGAL) {
    return 1;
  }

  if (leftShape == nullptr || rightShape == nullptr) {
    LOG(ERR) << "shape not found";
    TRI_ASSERT(false);
    return -1;
  }

  TRI_shape_type_t leftType = leftShape->_type;
  TRI_shape_type_t rightType = rightShape->_type;

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
      }  // end of switch (rightType)
    }    // end of case TRI_SHAPE_ILLEGAL

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
      }  // end of switch (rightType)
    }    // end of case TRI_SHAPE_NULL

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
          if (*((TRI_shape_boolean_t*)(left._data.data)) ==
              *((TRI_shape_boolean_t*)(right._data.data))) {
            return 0;
          }
          if (*((TRI_shape_boolean_t*)(left._data.data)) <
              *((TRI_shape_boolean_t*)(right._data.data))) {
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
      }  // end of switch (rightType)
    }    // end of case TRI_SHAPE_BOOLEAN

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
          if (*((TRI_shape_number_t*)(left._data.data)) ==
              *((TRI_shape_number_t*)(right._data.data))) {
            return 0;
          }
          if (*((TRI_shape_number_t*)(left._data.data)) <
              *((TRI_shape_number_t*)(right._data.data))) {
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
      }  // end of switch (rightType)
    }    // end of case TRI_SHAPE_NUMBER

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
          size_t leftLength;
          size_t rightLength;

          // compare strings
          // extract the strings
          if (leftType == TRI_SHAPE_SHORT_STRING) {
            leftString = (char*)(sizeof(TRI_shape_length_short_string_t) +
                                 left._data.data);
            leftLength =
                (size_t) * ((TRI_shape_length_short_string_t*)left._data.data) -
                1;
          } else {
            leftString = (char*)(sizeof(TRI_shape_length_long_string_t) +
                                 left._data.data);
            leftLength =
                (size_t) * ((TRI_shape_length_long_string_t*)left._data.data) -
                1;
          }

          if (rightType == TRI_SHAPE_SHORT_STRING) {
            rightString = (char*)(sizeof(TRI_shape_length_short_string_t) +
                                  right._data.data);
            rightLength =
                (size_t) *
                    ((TRI_shape_length_short_string_t*)right._data.data) -
                1;
          } else {
            rightString = (char*)(sizeof(TRI_shape_length_long_string_t) +
                                  right._data.data);
            rightLength =
                (size_t) * ((TRI_shape_length_long_string_t*)right._data.data) -
                1;
          }

          return TRI_compare_utf8(leftString, leftLength, rightString,
                                  rightLength);
        }
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: {
          return -1;
        }
      }  // end of switch (rightType)
    }    // end of case TRI_SHAPE_LONG/SHORT_STRING

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
          size_t leftListLength = *((TRI_shape_length_list_t*)left._data.data);
          size_t rightListLength =
              *((TRI_shape_length_list_t*)right._data.data);
          size_t listLength;

          // determine the smallest list
          if (leftListLength > rightListLength) {
            listLength = rightListLength;
          } else {
            listLength = leftListLength;
          }

          for (size_t j = 0; j < listLength; ++j) {
            if (leftType == TRI_SHAPE_HOMOGENEOUS_LIST) {
              TRI_AtHomogeneousListShapedJson(
                  (const TRI_homogeneous_list_shape_t*)(leftShape), &left, j,
                  &leftElement);
            } else if (leftType == TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
              TRI_AtHomogeneousSizedListShapedJson(
                  (const TRI_homogeneous_sized_list_shape_t*)(leftShape), &left,
                  j, &leftElement);
            } else {
              TRI_AtListShapedJson((const TRI_list_shape_t*)(leftShape), &left,
                                   j, &leftElement);
            }

            if (rightType == TRI_SHAPE_HOMOGENEOUS_LIST) {
              TRI_AtHomogeneousListShapedJson(
                  (const TRI_homogeneous_list_shape_t*)(rightShape), &right, j,
                  &rightElement);
            } else if (rightType == TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
              TRI_AtHomogeneousSizedListShapedJson(
                  (const TRI_homogeneous_sized_list_shape_t*)(rightShape),
                  &right, j, &rightElement);
            } else {
              TRI_AtListShapedJson((const TRI_list_shape_t*)(rightShape),
                                   &right, j, &rightElement);
            }

            int result = TRI_CompareShapeTypes(nullptr, nullptr, &leftElement,
                                               leftShaper, nullptr, nullptr,
                                               &rightElement, rightShaper);

            if (result != 0) {
              return result;
            }
          }

          // up to listLength everything matches
          if (leftListLength < rightListLength) {
            return -1;
          } else if (leftListLength > rightListLength) {
            return 1;
          }
          return 0;
        }

        case TRI_SHAPE_ARRAY: {
          return -1;
        }
      }  // end of switch (rightType)
    }    // end of case TRI_SHAPE_LIST ...

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
          // We are comparing a left JSON array with another JSON array on the
          // right
          // ...................................................................

          // ...................................................................
          // generate the left and right lists sorted by attribute names
          // ...................................................................

          TRI_vector_t leftSorted;
          TRI_vector_t rightSorted;

          bool error = false;
          if (FillAttributesVector(&leftSorted, &left, leftShape, leftShaper) !=
              TRI_ERROR_NO_ERROR) {
            error = true;
          }

          if (FillAttributesVector(&rightSorted, &right, rightShape,
                                   rightShaper) != TRI_ERROR_NO_ERROR) {
            error = true;
          }

          size_t const leftLength = TRI_LengthVector(&leftSorted);
          size_t const rightLength = TRI_LengthVector(&rightSorted);
          size_t const numElements =
              (leftLength < rightLength ? leftLength : rightLength);

          int result = 0;

          for (size_t i = 0; i < numElements; ++i) {
            attribute_entry_t const* l = static_cast<attribute_entry_t const*>(
                TRI_AtVector(&leftSorted, i));
            attribute_entry_t const* r = static_cast<attribute_entry_t const*>(
                TRI_AtVector(&rightSorted, i));

            // a binary comparison is sufficient here as we're only interested
            // in if the attribute names are
            // identical. the attribute names are from ShapedJson, so they have
            // been normalized already
            result = strcmp(l->_attribute, r->_attribute);
            // result = TRI_compare_utf8(l->_attribute, r->_attribute);

            if (result != 0) {
              break;
            }

            result = TRI_CompareShapeTypes(nullptr, nullptr, &l->_value,
                                           leftShaper, nullptr, nullptr,
                                           &r->_value, rightShaper);

            if (result != 0) {
              break;
            }
          }

          if (result == 0) {
            // .................................................................
            // The comparisions above indicate that the shaped_json_arrays are
            // equal,
            // however one more check to determine if the number of elements in
            // the arrays
            // are equal.
            // .................................................................
            if (leftLength < rightLength) {
              result = -1;
            } else if (leftLength > rightLength) {
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
      }  // end of switch (rightType)
    }    // end of case TRI_SHAPE_ARRAY
  }      // end of switch (leftType)

  TRI_ASSERT(false);
  return 0;  // shut the vc++ up
}

void TRI_InspectShapedSub(TRI_shaped_sub_t const* element,
                          char const* shapedJson, TRI_shaped_json_t& shaped) {
  if (element->_sid <= BasicShapes::TRI_SHAPE_SID_SHORT_STRING) {
    shaped._data.data = (char*)&element->_value._data;
    shaped._data.length = BasicShapes::TypeLengths[element->_sid];
  } else {
    shaped._data.data = const_cast<char*>(shapedJson) +
                        element->_value._position._offset;  // ONLY IN INDEX
    shaped._data.length = element->_value._position._length;
  }
}

void TRI_InspectShapedSub(TRI_shaped_sub_t const* element,
                          TRI_doc_mptr_t const* mptr, char const*& ptr,
                          size_t& length) {
  if (element->_sid <= BasicShapes::TRI_SHAPE_SID_SHORT_STRING) {
    ptr = (char const*)&element->_value._data;
    length = BasicShapes::TypeLengths[element->_sid];
  } else {
    ptr = mptr->getShapedJsonPtr() +
          element->_value._position._offset;  // ONLY IN INDEX
    length = element->_value._position._length;
  }
}

void TRI_FillShapedSub(TRI_shaped_sub_t* element,
                       TRI_shaped_json_t const* shapedObject, char const* ptr) {
  element->_sid = shapedObject->_sid;

  if (element->_sid <= BasicShapes::TRI_SHAPE_SID_SHORT_STRING) {
    if (shapedObject->_data.data != nullptr) {
      memcpy((char*)&element->_value._data, shapedObject->_data.data,
             BasicShapes::TypeLengths[element->_sid]);
    }
  } else {
    element->_value._position._length = shapedObject->_data.length;
    element->_value._position._offset =
        static_cast<uint32_t>(((char const*)shapedObject->_data.data) - ptr);
  }
}
