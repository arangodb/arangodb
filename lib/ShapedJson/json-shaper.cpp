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

#include "json-shaper.h"

#include "Basics/associative.h"
#include "Basics/hashes.h"
#include "Basics/logging.h"
#include "Basics/string-buffer.h"
#include "Basics/tri-strings.h"
#include "Basics/vector.h"

// #define DEBUG_JSON_SHAPER 1

// -----------------------------------------------------------------------------
// --SECTION--                                                           GLOBALS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief basic shape types (shared between all shapers)
////////////////////////////////////////////////////////////////////////////////

static TRI_basic_shapes_t BasicShapes;

// -----------------------------------------------------------------------------
// --SECTION--                                                      ARRAY SHAPER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief attribute identifier mapping
////////////////////////////////////////////////////////////////////////////////

typedef struct attribute_2_id_s {
  TRI_shape_aid_t _aid;         // attribute identifier
  TRI_shape_size_t _size;       // size of the attribute name in name[] including '\0'

  // char name[]
}
attribute_2_id_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief simple, array-based shaper
////////////////////////////////////////////////////////////////////////////////

typedef struct array_shaper_s {
  TRI_shaper_t base;

  TRI_associative_pointer_t _attributeNames;
  TRI_vector_pointer_t _attributes;

  TRI_associative_pointer_t _shapeDictionary;
  TRI_vector_pointer_t _shapes;

  // todo: add attribute weight structure
}
array_shaper_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the attribute path identifier
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashPidKeyAttributePath (TRI_associative_synced_t* array, void const* key) {
  return TRI_FnvHashPointer(key, sizeof(TRI_shape_pid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the attribute path
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashPidElementAttributePath (TRI_associative_synced_t* array, void const* element) {
  TRI_shape_path_t const* e;

  e = (TRI_shape_path_t const*) element;

  return TRI_FnvHashPointer(&e->_pid, sizeof(TRI_shape_pid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an attribute path identifier and an attribute path
////////////////////////////////////////////////////////////////////////////////

static bool EqualPidKeyAttributePath (TRI_associative_synced_t* array, void const* key, void const* element) {
  TRI_shape_pid_t const* k;
  TRI_shape_path_t const* e;

  k = (TRI_shape_pid_t const*) key;
  e = (TRI_shape_path_t const*) element;

  return *k == e->_pid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute path by identifier
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_path_t const* LookupAttributePathByPid (TRI_shaper_t* shaper, TRI_shape_pid_t pid) {
  return static_cast<TRI_shape_path_t const*>(TRI_LookupByKeyAssociativeSynced(&shaper->_attributePathsByPid, &pid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the attribute path name
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashNameKeyAttributePath (TRI_associative_synced_t* array, void const* key) {
  char const* k;

  k = (char const*) key;

  return TRI_FnvHashString(k);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the attribute path
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashNameElementAttributePath (TRI_associative_synced_t* array, void const* element) {
  char const* e;
  TRI_shape_path_t const* ee;

  e = (char const*) element;
  ee = (TRI_shape_path_t const*) element;

  return TRI_FnvHashPointer(e + sizeof(TRI_shape_path_t) + ee->_aidLength * sizeof(TRI_shape_aid_t),
                            ee->_nameLength - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an attribute name and an attribute
////////////////////////////////////////////////////////////////////////////////

static bool EqualNameKeyAttributePath (TRI_associative_synced_t* array, void const* key, void const* element) {
  char const* k;
  char const* e;
  TRI_shape_path_t const* ee;

  k = (char const*) key;
  e = (char const*) element;
  ee = (TRI_shape_path_t const*) element;

  return TRI_EqualString(k,e + sizeof(TRI_shape_path_t) + ee->_aidLength * sizeof(TRI_shape_aid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a shape path by identifier
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_path_t const* FindShapePathByName (TRI_shaper_t* shaper,
                                                    char const* name,
                                                    bool create,
                                                    bool isLocked) {
  TRI_shape_aid_t* aids;
  TRI_shape_path_t* result;
  size_t count;
  size_t len;
  size_t total;
  char* buffer;
  char* end;
  char* prev;
  char* ptr;
  void const* p;

  TRI_ASSERT(name != nullptr);

  p = TRI_LookupByKeyAssociativeSynced(&shaper->_attributePathsByName, name);

  if (p != nullptr) {
    return (TRI_shape_path_t const*) p;
  }

  // create a attribute path
  len = strlen(name);

  // lock the index and check that the element is still missing
  TRI_LockMutex(&shaper->_attributePathLock);

  // if the element appeared, return the pid
  p = TRI_LookupByKeyAssociativeSynced(&shaper->_attributePathsByName, name);

  if (p != nullptr) {
    TRI_UnlockMutex(&shaper->_attributePathLock);
    return (TRI_shape_path_t const*) p;
  }

  // split path into attribute pieces
  count = 0;
  aids = static_cast<TRI_shape_aid_t*>(TRI_Allocate(shaper->_memoryZone, len * sizeof(TRI_shape_aid_t), false));

  if (aids == nullptr) {
    TRI_UnlockMutex(&shaper->_attributePathLock);
    LOG_ERROR("out of memory in shaper");
    return nullptr;
  }

  buffer = ptr = TRI_DuplicateString2Z(shaper->_memoryZone, name, len);

  if (buffer == nullptr) {
    TRI_UnlockMutex(&shaper->_attributePathLock);
    TRI_Free(shaper->_memoryZone, aids);
    LOG_ERROR("out of memory in shaper");
    return nullptr;
  }

  end = buffer + len + 1;
  prev = buffer;

  for (;  ptr < end;  ++ptr) {
    if (*ptr == '.' || *ptr == '\0') {
      *ptr = '\0';

      if (ptr != prev) {
        if (create) {
          aids[count++] = shaper->findOrCreateAttributeByName(shaper, prev);
        }
        else {
          aids[count] = shaper->lookupAttributeByName(shaper, prev);

          if (aids[count] == 0) {
            TRI_FreeString(shaper->_memoryZone, buffer);
            TRI_UnlockMutex(&shaper->_attributePathLock);
            TRI_Free(shaper->_memoryZone, aids);
            return nullptr;
          }

          ++count;
        }
      }

      prev = ptr + 1;
    }
  }

  TRI_FreeString(shaper->_memoryZone, buffer);

  // create element
  total = sizeof(TRI_shape_path_t) + (len + 1) + (count * sizeof(TRI_shape_aid_t));
  result = static_cast<TRI_shape_path_t*>(TRI_Allocate(shaper->_memoryZone, total, false));

  if (result == nullptr) {
    TRI_UnlockMutex(&shaper->_attributePathLock);
    TRI_Free(shaper->_memoryZone, aids);
    LOG_ERROR("out of memory in shaper");
    return nullptr;
  }

  result->_pid = shaper->_nextPid++;
  result->_nameLength = (uint32_t) len + 1;
  result->_aidLength = count;

  memcpy(((char*) result) + sizeof(TRI_shape_path_t), aids, count * sizeof(TRI_shape_aid_t));
  memcpy(((char*) result) + sizeof(TRI_shape_path_t) + count * sizeof(TRI_shape_aid_t), name, len + 1);

  TRI_Free(shaper->_memoryZone, aids);

  void const* f = TRI_InsertKeyAssociativeSynced(&shaper->_attributePathsByName, name, result, false);
  if (f != nullptr) {
    LOG_WARNING("duplicate shape path %lu", (unsigned long) result->_pid);
  }
  TRI_ASSERT(f == nullptr);

  f = TRI_InsertKeyAssociativeSynced(&shaper->_attributePathsByPid, &result->_pid, result, false);
  if (f != nullptr) {
    LOG_WARNING("duplicate shape path %lu", (unsigned long)result->_pid);
  }
  TRI_ASSERT(f == nullptr);

  // return pid
  TRI_UnlockMutex(&shaper->_attributePathLock);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an attribute path by identifier
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_pid_t FindOrCreateAttributePathByName (TRI_shaper_t* shaper,
                                                        char const* name,
                                                        bool isLocked) {
  TRI_shape_path_t const* path = FindShapePathByName(shaper, name, true, isLocked);

  return path == nullptr ? 0 : path->_pid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute path by identifier
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_pid_t LookupAttributePathByName (TRI_shaper_t* shaper,
                                                  char const* name) {

  // do not create an unknown attribute (therefore isTemporary will be ignored)
  TRI_shape_path_t const* path = FindShapePathByName(shaper, name, false, true);

  return path == nullptr ? 0 : path->_pid;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                            SHAPER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the attribute path
////////////////////////////////////////////////////////////////////////////////

char const* TRI_AttributeNameShapePid (TRI_shaper_t* shaper,
                                       TRI_shape_pid_t pid) {
  TRI_shape_path_t const* path;
  char const* e;

  path = shaper->lookupAttributePathByPid(shaper, pid);
  e = (char const*) path;

  return e + sizeof(TRI_shape_path_t) + path->_aidLength * sizeof(TRI_shape_aid_t);
}

// -----------------------------------------------------------------------------
// --SECTION--                                               protected functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the shaper
////////////////////////////////////////////////////////////////////////////////

int TRI_InitShaper (TRI_shaper_t* shaper, TRI_memory_zone_t* zone) {
  int res;

  shaper->_memoryZone = zone;

  res = TRI_InitAssociativeSynced(&shaper->_attributePathsByName,
                                  zone,
                                  HashNameKeyAttributePath,
                                  HashNameElementAttributePath,
                                  EqualNameKeyAttributePath,
                                  0);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  res = TRI_InitAssociativeSynced(&shaper->_attributePathsByPid,
                                  zone,
                                  HashPidKeyAttributePath,
                                  HashPidElementAttributePath,
                                  EqualPidKeyAttributePath,
                                  0);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyAssociativeSynced(&shaper->_attributePathsByName);

    return res;
  }

  TRI_InitMutex(&shaper->_attributePathLock);

  shaper->_nextPid = 1;

  shaper->lookupAttributePathByPid = LookupAttributePathByPid;
  shaper->findOrCreateAttributePathByName = FindOrCreateAttributePathByName;
  shaper->lookupAttributePathByName = LookupAttributePathByName;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the shaper
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyShaper (TRI_shaper_t* shaper) {
  size_t const n = shaper->_attributePathsByName._nrAlloc;

  // only free pointers in attributePathsByName
  // (attributePathsByPid contains the same pointers!)
  for (size_t i = 0; i < n; ++i) {
    void* data = shaper->_attributePathsByName._table[i];

    if (data != nullptr) {
      TRI_Free(shaper->_memoryZone, data);
    }
  }

  TRI_DestroyAssociativeSynced(&shaper->_attributePathsByName);
  TRI_DestroyAssociativeSynced(&shaper->_attributePathsByPid);

  TRI_DestroyMutex(&shaper->_attributePathLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the shaper
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShaper (TRI_shaper_t* shaper) {
  TRI_DestroyShaper(shaper);
  TRI_Free(shaper->_memoryZone, shaper);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the sid for a basic type
////////////////////////////////////////////////////////////////////////////////

TRI_shape_sid_t TRI_LookupBasicSidShaper (TRI_shape_type_e type) {
  switch (type) {
    case TRI_SHAPE_NULL:
      return BasicShapes._sidNull;
    case TRI_SHAPE_BOOLEAN:
      return BasicShapes._sidBoolean;
    case TRI_SHAPE_NUMBER:
      return BasicShapes._sidNumber;
    case TRI_SHAPE_SHORT_STRING:
      return BasicShapes._sidShortString;
    case TRI_SHAPE_LONG_STRING:
      return BasicShapes._sidLongString;
    case TRI_SHAPE_LIST:
      return BasicShapes._sidList;
    default: {
    }
  }

  LOG_ERROR("encountered an illegal shape type");

  TRI_ASSERT(false);
  return TRI_SHAPE_ILLEGAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a shape is of primitive type
////////////////////////////////////////////////////////////////////////////////

TRI_shape_t* TRI_LookupSidBasicShapeShaper (TRI_shape_sid_t sid) {
  if (sid >= TRI_FirstCustomShapeIdShaper() || sid == 0) {
    return nullptr;
  }

  if (sid == BasicShapes._sidNull) {
    return &BasicShapes._shapeNull;
  }
  else if (sid == BasicShapes._sidBoolean) {
    return &BasicShapes._shapeBoolean;
  }
  else if (sid == BasicShapes._sidNumber) {
    return &BasicShapes._shapeNumber;
  }
  else if (sid == BasicShapes._sidShortString) {
    return &BasicShapes._shapeShortString;
  }
  else if (sid == BasicShapes._sidLongString) {
    return &BasicShapes._shapeLongString;
  }
  else if (sid == BasicShapes._sidList) {
    return &BasicShapes._shapeList;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a shape is of primitive type
////////////////////////////////////////////////////////////////////////////////

TRI_shape_t* TRI_LookupBasicShapeShaper (TRI_shape_t const* shape) {
  if (shape->_type == TRI_SHAPE_NULL) {
    return &BasicShapes._shapeNull;
  }
  else if (shape->_type == TRI_SHAPE_BOOLEAN) {
    return &BasicShapes._shapeBoolean;
  }
  else if (shape->_type == TRI_SHAPE_NUMBER) {
    return &BasicShapes._shapeNumber;
  }
  else if (shape->_type == TRI_SHAPE_SHORT_STRING) {
    return &BasicShapes._shapeShortString;
  }
  else if (shape->_type == TRI_SHAPE_LONG_STRING) {
    return &BasicShapes._shapeLongString;
  }
  else if (shape->_type == TRI_SHAPE_LIST) {
    return &BasicShapes._shapeList;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises global basic shape types
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseShaper () {
  TRI_shape_t* shape;

  memset(&BasicShapes, 0, sizeof(TRI_basic_shapes_t));

  // NULL
  shape = &BasicShapes._shapeNull;
  shape->_size = sizeof(TRI_null_shape_t);
  shape->_type = TRI_SHAPE_NULL;
  shape->_dataSize = 0;
  shape->_sid = BasicShapes._sidNull = 1;

  // BOOLEAN
  shape = &BasicShapes._shapeBoolean;
  shape->_size = sizeof(TRI_boolean_shape_t);
  shape->_type = TRI_SHAPE_BOOLEAN;
  shape->_dataSize = sizeof(TRI_shape_boolean_t);
  shape->_sid = BasicShapes._sidBoolean = 2;

  // NUMBER
  shape = &BasicShapes._shapeNumber;
  shape->_size = sizeof(TRI_number_shape_t);
  shape->_type = TRI_SHAPE_NUMBER;
  shape->_dataSize = sizeof(TRI_shape_number_t);
  shape->_sid = BasicShapes._sidNumber = 3;

  // SHORT STRING
  shape = &BasicShapes._shapeShortString;
  shape->_size = sizeof(TRI_short_string_shape_t);
  shape->_type = TRI_SHAPE_SHORT_STRING;
  shape->_dataSize = sizeof(TRI_shape_length_short_string_t) + TRI_SHAPE_SHORT_STRING_CUT;
  shape->_sid = BasicShapes._sidShortString = 4;

  // LONG STRING
  shape = &BasicShapes._shapeLongString;
  shape->_size = sizeof(TRI_long_string_shape_t);
  shape->_type = TRI_SHAPE_LONG_STRING;
  shape->_dataSize = TRI_SHAPE_SIZE_VARIABLE;
  shape->_sid = BasicShapes._sidLongString = 5;

  // LIST
  shape = &BasicShapes._shapeList;
  shape->_size = sizeof(TRI_list_shape_t);
  shape->_type = TRI_SHAPE_LIST;
  shape->_dataSize = TRI_SHAPE_SIZE_VARIABLE;
  shape->_sid = BasicShapes._sidList = 6;

  TRI_ASSERT(shape->_sid + 1 == TRI_FirstCustomShapeIdShaper());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown shaper
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownShaper () {
  // nothing to do
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
