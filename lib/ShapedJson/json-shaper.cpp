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

TRI_shape_pid_t const BasicShapes::TRI_SHAPE_SID_NULL          = 1;
TRI_shape_pid_t const BasicShapes::TRI_SHAPE_SID_BOOLEAN       = 2;
TRI_shape_pid_t const BasicShapes::TRI_SHAPE_SID_NUMBER        = 3;
TRI_shape_pid_t const BasicShapes::TRI_SHAPE_SID_SHORT_STRING  = 4;
TRI_shape_pid_t const BasicShapes::TRI_SHAPE_SID_LONG_STRING   = 5;
TRI_shape_pid_t const BasicShapes::TRI_SHAPE_SID_LIST          = 6;
  
TRI_shape_t const BasicShapes::_shapeNull = { 
  BasicShapes::TRI_SHAPE_SID_NULL, 
  TRI_SHAPE_NULL, 
  sizeof(TRI_null_shape_t), 
  0 
};

TRI_shape_t const BasicShapes::_shapeBoolean = { 
  BasicShapes::TRI_SHAPE_SID_BOOLEAN, 
  TRI_SHAPE_BOOLEAN, 
  sizeof(TRI_boolean_shape_t), 
  sizeof(TRI_shape_boolean_t) 
};

TRI_shape_t const BasicShapes::_shapeNumber = { 
  BasicShapes::TRI_SHAPE_SID_NUMBER, 
  TRI_SHAPE_NUMBER, 
  sizeof(TRI_number_shape_t), 
  sizeof(TRI_shape_number_t) 
};

TRI_shape_t const BasicShapes::_shapeShortString = { 
  BasicShapes::TRI_SHAPE_SID_SHORT_STRING, 
  TRI_SHAPE_SHORT_STRING, 
  sizeof(TRI_short_string_shape_t), 
  sizeof(TRI_shape_length_short_string_t) + TRI_SHAPE_SHORT_STRING_CUT 
};

TRI_shape_t const BasicShapes::_shapeLongString = { 
  BasicShapes::TRI_SHAPE_SID_LONG_STRING, 
  TRI_SHAPE_LONG_STRING, 
  sizeof(TRI_long_string_shape_t), 
  TRI_SHAPE_SIZE_VARIABLE 
};

TRI_shape_t const BasicShapes::_shapeList = { 
  BasicShapes::TRI_SHAPE_SID_LIST, 
  TRI_SHAPE_LIST, 
  sizeof(TRI_list_shape_t), 
  TRI_SHAPE_SIZE_VARIABLE 
};

TRI_shape_t const* BasicShapes::ShapeAddresses[7] = {
  nullptr,
  &BasicShapes::_shapeNull,
  &BasicShapes::_shapeBoolean,
  &BasicShapes::_shapeNumber,
  &BasicShapes::_shapeShortString,
  &BasicShapes::_shapeLongString,
  &BasicShapes::_shapeList
};

////////////////////////////////////////////////////////////////////////////////
/// @brief static const length information for basic shape types
////////////////////////////////////////////////////////////////////////////////

uint32_t const BasicShapes::TypeLengths[5] = {
  0, // not used
  0, // null
  sizeof(TRI_shape_boolean_t),
  sizeof(TRI_shape_number_t),
  sizeof(TRI_shape_length_short_string_t) + TRI_SHAPE_SHORT_STRING_CUT
};

static_assert(BasicShapes::TRI_SHAPE_SID_NULL == 1, "invalid shape id for null shape");
static_assert(BasicShapes::TRI_SHAPE_SID_BOOLEAN == 2, "invalid shape id for boolean shape");
static_assert(BasicShapes::TRI_SHAPE_SID_NUMBER == 3, "invalid shape id for number shape");
static_assert(BasicShapes::TRI_SHAPE_SID_SHORT_STRING == 4, "invalid shape id for short string shape");

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the attribute path identifier
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashPidKeyAttributePath (TRI_associative_synced_t* array, 
                                         void const* key) {
  return TRI_FnvHashPointer(key, sizeof(TRI_shape_pid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the attribute path
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashPidElementAttributePath (TRI_associative_synced_t* array, 
                                             void const* element) {
  auto e = static_cast<TRI_shape_path_t const*>(element);

  return TRI_FnvHashPointer(&e->_pid, sizeof(TRI_shape_pid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an attribute path identifier and an attribute path
////////////////////////////////////////////////////////////////////////////////

static bool EqualPidKeyAttributePath (TRI_associative_synced_t* array, 
                                      void const* key, 
                                      void const* element) {
  auto k = static_cast<TRI_shape_pid_t const*>(key);
  auto e = static_cast<TRI_shape_path_t const*>(element);

  return *k == e->_pid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute path by identifier
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_path_t const* LookupAttributePathByPid (TRI_shaper_t* shaper, 
                                                         TRI_shape_pid_t pid) {
  return static_cast<TRI_shape_path_t const*>(TRI_LookupByKeyAssociativeSynced(&shaper->_attributePathsByPid, &pid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the attribute path name
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashNameKeyAttributePath (TRI_associative_synced_t* array, 
                                          void const* key) {
  return TRI_FnvHashString(static_cast<char const*>(key));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the attribute path
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashNameElementAttributePath (TRI_associative_synced_t* array, 
                                              void const* element) {
  char const* e = static_cast<char const*>(element);
  TRI_shape_path_t const* ee = static_cast<TRI_shape_path_t const*>(element);

  return TRI_FnvHashPointer(e + sizeof(TRI_shape_path_t) + ee->_aidLength * sizeof(TRI_shape_aid_t),
                            ee->_nameLength - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an attribute name and an attribute
////////////////////////////////////////////////////////////////////////////////

static bool EqualNameKeyAttributePath (TRI_associative_synced_t* array, 
                                       void const* key, 
                                       void const* element) {
  char const* k = static_cast<char const*>(key);
  char const* e = static_cast<char const*>(element);
  TRI_shape_path_t const* ee = static_cast<TRI_shape_path_t const*>(element);

  return TRI_EqualString(k, e + sizeof(TRI_shape_path_t) + ee->_aidLength * sizeof(TRI_shape_aid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a shape path by identifier
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_path_t const* FindShapePathByName (TRI_shaper_t* shaper,
                                                    char const* name,
                                                    bool create) {
  char* buffer;
  char* end;
  char* prev;
  char* ptr;

  TRI_ASSERT(name != nullptr);

  void const* p = TRI_LookupByKeyAssociativeSynced(&shaper->_attributePathsByName, name);

  if (p != nullptr) {
    return (TRI_shape_path_t const*) p;
  }

  // create an attribute path
  size_t len = strlen(name);

  // lock the index and check that the element is still missing
  TRI_LockMutex(&shaper->_attributePathLock);

  // if the element appeared, return the pid
  p = TRI_LookupByKeyAssociativeSynced(&shaper->_attributePathsByName, name);

  if (p != nullptr) {
    TRI_UnlockMutex(&shaper->_attributePathLock);
    return (TRI_shape_path_t const*) p;
  }

  // split path into attribute pieces
  size_t count = 0;
  TRI_shape_aid_t* aids = static_cast<TRI_shape_aid_t*>(TRI_Allocate(shaper->_memoryZone, len * sizeof(TRI_shape_aid_t), false));

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
  size_t total = sizeof(TRI_shape_path_t) + (len + 1) + (count * sizeof(TRI_shape_aid_t));
  TRI_shape_path_t* result = static_cast<TRI_shape_path_t*>(TRI_Allocate(shaper->_memoryZone, total, false));

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
                                                        char const* name) {
  TRI_shape_path_t const* path = FindShapePathByName(shaper, name, true);

  return path == nullptr ? 0 : path->_pid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute path by identifier
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_pid_t LookupAttributePathByName (TRI_shaper_t* shaper,
                                                  char const* name) {

  TRI_shape_path_t const* path = FindShapePathByName(shaper, name, false);

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
  TRI_shape_path_t const* path = shaper->lookupAttributePathByPid(shaper, pid);
  char const* e = (char const*) path;

  return e + sizeof(TRI_shape_path_t) + path->_aidLength * sizeof(TRI_shape_aid_t);
}

// -----------------------------------------------------------------------------
// --SECTION--                                               protected functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the shaper
////////////////////////////////////////////////////////////////////////////////

int TRI_InitShaper (TRI_shaper_t* shaper, TRI_memory_zone_t* zone) {
  shaper->_memoryZone = zone;

  int res = TRI_InitAssociativeSynced(&shaper->_attributePathsByName,
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
/// @brief checks whether a shape is of primitive type
////////////////////////////////////////////////////////////////////////////////

TRI_shape_t const* TRI_LookupSidBasicShapeShaper (TRI_shape_sid_t sid) {
  if (sid > BasicShapes::TRI_SHAPE_SID_LIST) {
    return nullptr;
  }

  return BasicShapes::ShapeAddresses[sid];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a shape is of primitive type
////////////////////////////////////////////////////////////////////////////////

TRI_shape_t const* TRI_LookupBasicShapeShaper (TRI_shape_t const* shape) {
  if (shape->_type == TRI_SHAPE_NULL) {
    return &BasicShapes::_shapeNull;
  }
  else if (shape->_type == TRI_SHAPE_BOOLEAN) {
    return &BasicShapes::_shapeBoolean;
  }
  else if (shape->_type == TRI_SHAPE_NUMBER) {
    return &BasicShapes::_shapeNumber;
  }
  else if (shape->_type == TRI_SHAPE_SHORT_STRING) {
    return &BasicShapes::_shapeShortString;
  }
  else if (shape->_type == TRI_SHAPE_LONG_STRING) {
    return &BasicShapes::_shapeLongString;
  }
  else if (shape->_type == TRI_SHAPE_LIST) {
    return &BasicShapes::_shapeList;
  }

  return nullptr;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
