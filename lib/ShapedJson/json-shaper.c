////////////////////////////////////////////////////////////////////////////////
/// @brief json shaper used to compute the shape of an json object
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Martin Schoenert
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "json-shaper.h"

#include "BasicsC/associative.h"
#include "BasicsC/hashes.h"
#include "BasicsC/logging.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/strings.h"
#include "BasicsC/vector.h"

// #define DEBUG_JSON_SHAPER 1

// -----------------------------------------------------------------------------
// --SECTION--                                                      ARRAY SHAPER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

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

static TRI_shape_path_t const* LookupPidAttributePath (TRI_shaper_t* shaper, TRI_shape_pid_t pid) {
  return TRI_LookupByKeyAssociativeSynced(&shaper->_attributePathsByPid, &pid);
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
  /*
  return TRI_EqualString2(k,
                          e + sizeof(TRI_shape_path_t) + ee->_aidLength * sizeof(TRI_shape_aid_t),
                          ee->_nameLength - 1);
  */
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute path by identifier
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_pid_t FindNameAttributePath (TRI_shaper_t* shaper, char const* name) {
  TRI_shape_aid_t* aids;
  TRI_shape_path_t* result;
  size_t count;
  size_t len;
  size_t total;
  char* buffer;
  char* end;
  char* prev;
  char* ptr;
  void const* f;
  void const* p;

  p = TRI_LookupByKeyAssociativeSynced(&shaper->_attributePathsByName, name);

  if (p != NULL) {
    return ((TRI_shape_path_t const*) p)->_pid;
  }

  // create a attribute path
  len = strlen(name);

  // lock the index and check that the element is still missing
  TRI_LockMutex(&shaper->_attributePathLock);

  // if the element appeared, return the pid
  p = TRI_LookupByKeyAssociativeSynced(&shaper->_attributePathsByName, name);

  if (p != NULL) {
    TRI_UnlockMutex(&shaper->_attributePathLock);
    return ((TRI_shape_path_t const*) p)->_pid;
  }

  // split path into attribute pieces
  count = 0;
  aids = TRI_Allocate(shaper->_memoryZone, len * sizeof(TRI_shape_aid_t), false);

  if (aids == NULL) {
    TRI_UnlockMutex(&shaper->_attributePathLock);
    LOG_ERROR("out of memory in shaper");
    return 0;
  }

  buffer = ptr = TRI_DuplicateString2Z(shaper->_memoryZone, name, len);

  if (buffer == NULL) {
    TRI_UnlockMutex(&shaper->_attributePathLock);
    TRI_Free(shaper->_memoryZone, aids);
    LOG_ERROR("out of memory in shaper");
    return 0;
  }

  end = buffer + len + 1;
  prev = buffer;

  for (;  ptr < end;  ++ptr) {
    if (*ptr == '.' || *ptr == '\0') {
      *ptr = '\0';

      if (ptr != prev) {
        aids[count++] = shaper->findAttributeName(shaper, prev);
      }

      prev = ptr + 1;
    }
  }

  TRI_FreeString(shaper->_memoryZone, buffer);

  // create element
  total = sizeof(TRI_shape_path_t) + (len + 1) + (count * sizeof(TRI_shape_aid_t));
  result = TRI_Allocate(shaper->_memoryZone, total, false);

  if (result == NULL) {
    TRI_UnlockMutex(&shaper->_attributePathLock);
    TRI_Free(shaper->_memoryZone, aids);
    LOG_ERROR("out of memory in shaper");
    return 0;
  }

  result->_pid = shaper->_nextPid++;
  result->_nameLength = len + 1;
  result->_aidLength = count;

  memcpy(((char*) result) + sizeof(TRI_shape_path_t), aids, count * sizeof(TRI_shape_aid_t));
  memcpy(((char*) result) + sizeof(TRI_shape_path_t) + count * sizeof(TRI_shape_aid_t), name, len + 1);

  TRI_Free(shaper->_memoryZone, aids);

  f = TRI_InsertKeyAssociativeSynced(&shaper->_attributePathsByName, name, result);
  assert(f == NULL);

  f = TRI_InsertKeyAssociativeSynced(&shaper->_attributePathsByPid, &result->_pid, result);
  assert(f == NULL);

  // return pid
  TRI_UnlockMutex(&shaper->_attributePathLock);
  return result->_pid;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the attribute name
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyAttributeName (TRI_associative_pointer_t* array, void const* key) {
  char const* k;

  k = (char const*) key;

  return TRI_FnvHashString(k);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the attribute
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementAttributeName (TRI_associative_pointer_t* array, void const* element) {
  char const* e;
  attribute_2_id_t const* ee;

  e = (char const*) element;
  ee = (attribute_2_id_t const*) element;

  return TRI_FnvHashPointer(e + sizeof(attribute_2_id_t), (size_t)(ee->_size - 1));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an attribute name and an attribute
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyAttributeName (TRI_associative_pointer_t* array, void const* key, void const* element) {
  char const* k;
  char const* e;

  k = (char const*) key;
  e = (char const*) element;

  return strcmp(k, e + sizeof(attribute_2_id_t)) == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an attribute identifier by name
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_aid_t FindAttributeNameArrayShaper (TRI_shaper_t* shaper, char const* name) {
  array_shaper_t* s;
  void const* p;

  s = (array_shaper_t*) shaper;
  p = TRI_LookupByKeyAssociativePointer(&s->_attributeNames, name);

  if (p == NULL) {
    size_t n;
    attribute_2_id_t* a2i;
    void* f;
    int res;

    n = strlen(name) + 1;
    a2i = TRI_Allocate(shaper->_memoryZone, sizeof(attribute_2_id_t) + n, false);

    if (a2i == NULL) {
      return 0;
    }

    a2i->_aid = 1 + s->_attributes._length;
    a2i->_size = n;
    memcpy(((char*) a2i) + sizeof(attribute_2_id_t), name, n);

    f = TRI_InsertKeyAssociativePointer(&s->_attributeNames, name, a2i, false);

    if (f == NULL) {
      TRI_Free(shaper->_memoryZone, a2i);
      return 0;
    }

    res = TRI_PushBackVectorPointer(&s->_attributes, a2i);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_RemoveKeyAssociativePointer(&s->_attributeNames, name);
      TRI_Free(shaper->_memoryZone, a2i);
      return 0;
    }

    return a2i->_aid;
  }
  else {
    attribute_2_id_t const* a2i = (attribute_2_id_t const*) p;

    return a2i->_aid;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute name by identifier
////////////////////////////////////////////////////////////////////////////////

static char const* LookupAttributeIdArrayShaper (TRI_shaper_t* shaper, TRI_shape_aid_t aid) {
  array_shaper_t* s;

  s = (array_shaper_t*) shaper;

  if (0 < aid && aid <= s->_attributes._length) {
    char const* a2i;

    a2i = s->_attributes._buffer[aid - 1];

    return a2i + sizeof(attribute_2_id_t);
  }

  return NULL;
}

static int64_t LookupAttributeWeight (TRI_shaper_t* shaper, TRI_shape_aid_t aid) {
  // todo: add support for an attribute weight
  assert(0);
  return -1;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the shapes
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementShape (TRI_associative_pointer_t* array, void const* element) {
  char const* e;
  TRI_shape_t const* ee;

  e = element;
  ee = element;

  return TRI_FnvHashPointer(e + sizeof(TRI_shape_sid_t), (size_t)(ee->_size - sizeof(TRI_shape_sid_t)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares shapes
////////////////////////////////////////////////////////////////////////////////

static bool EqualElementShape (TRI_associative_pointer_t* array, void const* left, void const* right) {
  TRI_shape_t const* ll;
  TRI_shape_t const* rr;
  char const* l;
  char const* r;

  l = left;
  ll = left;

  r = right;
  rr = right;

  return (ll->_size == rr->_size)
    && memcmp(l + sizeof(TRI_shape_sid_t), r + sizeof(TRI_shape_sid_t), (size_t)(ll->_size - sizeof(TRI_shape_sid_t))) == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a shape
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_t const* FindShapeShape (TRI_shaper_t* shaper, TRI_shape_t* shape) {
  TRI_shape_t const* l;
  array_shaper_t* s;
  int res;

  s = (array_shaper_t*) shaper;
  l = TRI_LookupByElementAssociativePointer(&s->_shapeDictionary, shape);

  if (l != NULL) {
    TRI_Free(shaper->_memoryZone, shape);
    return l;
  }

  shape->_sid = s->_shapes._length + 1;
  assert(shape->_sid > 0);

  TRI_InsertElementAssociativePointer(&s->_shapeDictionary, shape, false);

  if (s->base._memoryZone->_failed) {
    TRI_Free(shaper->_memoryZone, shape);
    return NULL;
  }

  res = TRI_PushBackVectorPointer(&s->_shapes, shape);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_RemoveElementAssociativePointer(&s->_shapeDictionary, shape);
    TRI_Free(shaper->_memoryZone, shape);
    return NULL;
  }

  return shape;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a shape by identifier
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_t const* LookupShapeId (TRI_shaper_t* shaper, TRI_shape_sid_t sid) {
  array_shaper_t* s;

  s = (array_shaper_t*) shaper;

  if (0 < sid && sid <= s->_shapes._length) {
    return s->_shapes._buffer[sid - 1];
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a simple, array-based shaper
////////////////////////////////////////////////////////////////////////////////

TRI_shaper_t* TRI_CreateArrayShaper (TRI_memory_zone_t* zone) {
  array_shaper_t* shaper;
  bool ok;

  // create the shaper
  shaper = TRI_Allocate(zone, sizeof(array_shaper_t), false);

  if (shaper == NULL) {
    return NULL;
  }

  TRI_InitShaper(&shaper->base, zone);

  // create the attribute dictionary
  TRI_InitAssociativePointer(&shaper->_attributeNames,
                             zone,
                             HashKeyAttributeName,
                             HashElementAttributeName,
                             EqualKeyAttributeName,
                             0);

  // create the attributes vector
  TRI_InitVectorPointer(&shaper->_attributes, zone);

  // create the shape dictionary
  TRI_InitAssociativePointer(&shaper->_shapeDictionary,
                             zone,
                             0,
                             HashElementShape,
                             0,
                             EqualElementShape);

  // create the shapes vector
  TRI_InitVectorPointer(&shaper->_shapes, zone);

  // set the find and lookup functions
  shaper->base.findAttributeName = FindAttributeNameArrayShaper;
  shaper->base.lookupAttributeId = LookupAttributeIdArrayShaper;
  shaper->base.findShape = FindShapeShape;
  shaper->base.lookupShapeId = LookupShapeId;
  shaper->base.lookupAttributeWeight = LookupAttributeWeight;

  // handle basics
  ok = TRI_InsertBasicTypesShaper(&shaper->base);

  if (! ok || zone->_failed) {
    TRI_FreeArrayShaper(zone, &shaper->base);
    return NULL;
  }

  // and return
  return &shaper->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array-based shaper, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyArrayShaper (TRI_shaper_t* shaper) {
  array_shaper_t* s;
  size_t i;
  size_t n;

  s = (array_shaper_t*) shaper;

  for (i = 0, n = s->_attributes._length;  i < n;  ++i) {
    attribute_2_id_t* a2i;

    a2i = s->_attributes._buffer[i];
    TRI_Free(shaper->_memoryZone, a2i);
  }

  TRI_DestroyAssociativePointer(&s->_attributeNames);
  TRI_DestroyVectorPointer(&s->_attributes);

  for (i = 0, n = s->_shapes._length;  i < n;  ++i) {
    TRI_shape_t* shape;

    shape = s->_shapes._buffer[i];
    TRI_Free(shaper->_memoryZone, shape);
  }

  TRI_DestroyAssociativePointer(&s->_shapeDictionary);
  TRI_DestroyVectorPointer(&s->_shapes);

  TRI_DestroyShaper(shaper);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array-based shaper and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeArrayShaper (TRI_memory_zone_t* zone, TRI_shaper_t* shaper) {
  TRI_DestroyArrayShaper(shaper);
  TRI_Free(zone, shaper);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            SHAPER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the attribute path
////////////////////////////////////////////////////////////////////////////////

char const* TRI_AttributeNameShapePid (TRI_shaper_t* shaper, TRI_shape_pid_t pid) {
  TRI_shape_path_t const* path;
  char const* e;

  path = shaper->lookupAttributePathByPid(shaper, pid);
  e = (char const*) path;

  return e + sizeof(TRI_shape_path_t) + path->_aidLength * sizeof(TRI_shape_aid_t);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               protected functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the shaper
////////////////////////////////////////////////////////////////////////////////

void TRI_InitShaper (TRI_shaper_t* shaper, TRI_memory_zone_t* zone) {
  shaper->_memoryZone = zone;

  TRI_InitAssociativeSynced(&shaper->_attributePathsByName,
                            zone,
                            HashNameKeyAttributePath,
                            HashNameElementAttributePath,
                            EqualNameKeyAttributePath,
                            0);

  TRI_InitAssociativeSynced(&shaper->_attributePathsByPid,
                            zone,
                            HashPidKeyAttributePath,
                            HashPidElementAttributePath,
                            EqualPidKeyAttributePath,
                            0);

  TRI_InitMutex(&shaper->_attributePathLock);

  shaper->_nextPid = 1;

  shaper->lookupAttributePathByPid = LookupPidAttributePath;
  shaper->findAttributePathByName = FindNameAttributePath;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the shaper
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyShaper (TRI_shaper_t* shaper) {
  size_t n = shaper->_attributePathsByName._nrAlloc;
  size_t i;

  // only free pointers in attributePathsByName
  // (attributePathsByPid contains the same pointers!)
  for (i = 0; i < n; ++i) {
    void* data = shaper->_attributePathsByName._table[i];

    if (data) {
      TRI_Free(shaper->_memoryZone, data);
    }
  }

  TRI_DestroyAssociativeSynced(&shaper->_attributePathsByName);
  TRI_DestroyAssociativeSynced(&shaper->_attributePathsByPid);

  TRI_DestroyMutex(&shaper->_attributePathLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates or finds the basic types
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertBasicTypesShaper (TRI_shaper_t* shaper) {
  TRI_shape_t const* l;
  TRI_shape_t* shape;

  // NULL
  shape = TRI_Allocate(shaper->_memoryZone, sizeof(TRI_null_shape_t), true);

  if (shape == NULL) {
    return false;
  }

  shape->_size = sizeof(TRI_null_shape_t);
  shape->_type = TRI_SHAPE_NULL;
  shape->_dataSize = 0;

  l = shaper->findShape(shaper, shape);

  if (l == NULL) {
    return false;
  }

  shaper->_sidNull = l->_sid;

  // BOOLEAN
  shape = TRI_Allocate(shaper->_memoryZone, sizeof(TRI_boolean_shape_t), true);

  if (shape == NULL) {
    return false;
  }

  shape->_size = sizeof(TRI_boolean_shape_t);
  shape->_type = TRI_SHAPE_BOOLEAN;
  shape->_dataSize = sizeof(TRI_shape_boolean_t);

  l = shaper->findShape(shaper, shape);

  if (l == NULL) {
    return false;
  }

  shaper->_sidBoolean = l->_sid;

  // NUMBER
  shape = TRI_Allocate(shaper->_memoryZone, sizeof(TRI_number_shape_t), true);

  if (shape == NULL) {
    return false;
  }

  shape->_size = sizeof(TRI_number_shape_t);
  shape->_type = TRI_SHAPE_NUMBER;
  shape->_dataSize = sizeof(TRI_shape_number_t);

  l = shaper->findShape(shaper, shape);

  if (l == NULL) {
    return false;
  }

  shaper->_sidNumber = l->_sid;

  // SHORT STRING
  shape = TRI_Allocate(shaper->_memoryZone, sizeof(TRI_short_string_shape_t), true);

  if (shape == NULL) {
    return false;
  }

  shape->_size = sizeof(TRI_short_string_shape_t);
  shape->_type = TRI_SHAPE_SHORT_STRING;
  shape->_dataSize = sizeof(TRI_shape_length_short_string_t) + TRI_SHAPE_SHORT_STRING_CUT;

  l = shaper->findShape(shaper, shape);

  if (l == NULL) {
    return false;
  }

  shaper->_sidShortString = l->_sid;

  // LONG STRING
  shape = TRI_Allocate(shaper->_memoryZone, sizeof(TRI_long_string_shape_t), true);

  if (shape == NULL) {
    return false;
  }

  shape->_size = sizeof(TRI_long_string_shape_t);
  shape->_type = TRI_SHAPE_LONG_STRING;
  shape->_dataSize = TRI_SHAPE_SIZE_VARIABLE;

  l = shaper->findShape(shaper, shape);

  if (l == NULL) {
    return false;
  }

  shaper->_sidLongString = l->_sid;

  // LIST
  shape = TRI_Allocate(shaper->_memoryZone, sizeof(TRI_list_shape_t), true);

  if (shape == NULL) {
    return false;
  }

  shape->_size = sizeof(TRI_list_shape_t);
  shape->_type = TRI_SHAPE_LIST;
  shape->_dataSize = TRI_SHAPE_SIZE_VARIABLE;

  l = shaper->findShape(shaper, shape);

  if (l == NULL) {
    return false;
  }

  shaper->_sidList = l->_sid;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
