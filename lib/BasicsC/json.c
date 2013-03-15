////////////////////////////////////////////////////////////////////////////////
/// @brief json objects
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "json.h"
#include "BasicsC/files.h"
#include "BasicsC/logging.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/strings.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                              JSON
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a json object
////////////////////////////////////////////////////////////////////////////////

static int StringifyJson (TRI_memory_zone_t* zone,
                          TRI_string_buffer_t* buffer,
                          TRI_json_t const* object,
                          bool braces) {
  size_t n;
  size_t i;
  size_t outLength;
  char* ptr;
  int res;

  switch (object->_type) {
    case TRI_JSON_UNUSED:
      break;

    case TRI_JSON_NULL:
      res = TRI_AppendString2StringBuffer(buffer, "null", 4); // strlen("null")

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }

      break;

    case TRI_JSON_BOOLEAN:
      if (object->_value._boolean) {
        res = TRI_AppendString2StringBuffer(buffer, "true", 4); // strlen("true")
      }
      else {
        res = TRI_AppendString2StringBuffer(buffer, "false", 5); // strlen("false")
      }

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }

      break;

    case TRI_JSON_NUMBER:
      res = TRI_AppendDoubleStringBuffer(buffer, object->_value._number);

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }

      break;

    case TRI_JSON_STRING:
      res = TRI_AppendCharStringBuffer(buffer, '\"');

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }

      if (object->_value._string.length > 0) {
        // optimisation for the empty string
        ptr = TRI_EscapeUtf8StringZ(zone,
                                    object->_value._string.data,
                                    object->_value._string.length - 1,
                                    false,
                                    &outLength);

        if (ptr == NULL) {
          return TRI_ERROR_OUT_OF_MEMORY;
        }

        res = TRI_AppendString2StringBuffer(buffer, ptr, outLength);
  
        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }

        TRI_Free(zone, ptr);
      }

      res = TRI_AppendCharStringBuffer(buffer, '\"');

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }

      break;

    case TRI_JSON_ARRAY:
      if (braces) {
        res = TRI_AppendCharStringBuffer(buffer, '{');

        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }
      }

      n = object->_value._objects._length;

      for (i = 0;  i < n;  i += 2) {
        if (0 < i) {
          res = TRI_AppendCharStringBuffer(buffer, ',');

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }
        }

        res = StringifyJson(zone, buffer, TRI_AtVector(&object->_value._objects, i), true);

        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }

        res = TRI_AppendCharStringBuffer(buffer, ':');

        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }

        res = StringifyJson(zone, buffer, TRI_AtVector(&object->_value._objects, i + 1), true);

        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }
      }

      if (braces) {
        res = TRI_AppendCharStringBuffer(buffer, '}');

        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }
      }

      break;

    case TRI_JSON_LIST:
      if (braces) {
        res = TRI_AppendCharStringBuffer(buffer, '[');

        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }
      }

      n = object->_value._objects._length;

      for (i = 0;  i < n;  ++i) {
        if (0 < i) {
          res = TRI_AppendCharStringBuffer(buffer, ',');

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }
        }

        res = StringifyJson(zone, buffer, TRI_AtVector(&object->_value._objects, i), true);

        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }
      }

      if (braces) {
        res = TRI_AppendCharStringBuffer(buffer, ']');

        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }
      }

      break;
  }

  return TRI_ERROR_NO_ERROR;
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
/// @brief creates a null object
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CreateNullJson (TRI_memory_zone_t* zone) {
  TRI_json_t* result;

  result = (TRI_json_t*) TRI_Allocate(zone, sizeof(TRI_json_t), false);

  if (result == NULL) {
    return NULL;
  }

  result->_type = TRI_JSON_NULL;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a boolean object
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CreateBooleanJson (TRI_memory_zone_t* zone, bool value) {
  TRI_json_t* result;

  result = (TRI_json_t*) TRI_Allocate(zone, sizeof(TRI_json_t), false);

  if (result == NULL) {
    return NULL;
  }

  result->_type = TRI_JSON_BOOLEAN;
  result->_value._boolean = value;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a number object
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CreateNumberJson (TRI_memory_zone_t* zone, double value) {
  TRI_json_t* result;

  result = (TRI_json_t*) TRI_Allocate(zone, sizeof(TRI_json_t), false);

  if (result == NULL) {
    return NULL;
  }

  result->_type = TRI_JSON_NUMBER;
  result->_value._number = value;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a number object
////////////////////////////////////////////////////////////////////////////////

void TRI_InitNumberJson (TRI_json_t* result, double value) {
  result->_type = TRI_JSON_NUMBER;
  result->_value._number = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a string object
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CreateStringJson (TRI_memory_zone_t* zone, char* value) {
  TRI_json_t* result;
  size_t length;

  length = strlen(value);

  result = (TRI_json_t*) TRI_Allocate(zone, sizeof(TRI_json_t), false);

  if (result == NULL) {
    return NULL;
  }

  result->_type = TRI_JSON_STRING;
  result->_value._string.length = length + 1;
  result->_value._string.data = value;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a string object
////////////////////////////////////////////////////////////////////////////////

void TRI_InitStringJson (TRI_memory_zone_t* zone, TRI_json_t* result, char* value) {
  size_t length;

  length = strlen(value);

  result->_type = TRI_JSON_STRING;
  result->_value._string.length = length + 1;
  result->_value._string.data = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a string object, copying the string
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CreateStringCopyJson (TRI_memory_zone_t* zone, char const* value) {
  TRI_json_t* result;
  size_t length;

  length = strlen(value);

  result = (TRI_json_t*) TRI_Allocate(zone, sizeof(TRI_json_t), false);

  if (result == NULL) {
    return NULL;
  }

  result->_type = TRI_JSON_STRING;
  result->_value._string.length = length + 1;
  result->_value._string.data = TRI_DuplicateString2Z(zone, value, length);

  if (result->_value._string.data == NULL) {
    TRI_Free(zone, result);
    return NULL;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a string object with given length
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CreateString2Json (TRI_memory_zone_t* zone, char* value, size_t length) {
  TRI_json_t* result;

  result = (TRI_json_t*) TRI_Allocate(zone, sizeof(TRI_json_t), false);

  if (result == NULL) {
    return NULL;
  }

  result->_type = TRI_JSON_STRING;
  result->_value._string.length = length + 1;
  result->_value._string.data = value;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a string object with given length, copying the string
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CreateString2CopyJson (TRI_memory_zone_t* zone, char const* value, size_t length) {
  TRI_json_t* result;

  result = (TRI_json_t*) TRI_Allocate(zone, sizeof(TRI_json_t), false);

  if (result == NULL) {
    return NULL;
  }

  result->_type = TRI_JSON_STRING;
  result->_value._string.length = length + 1;
  result->_value._string.data = TRI_DuplicateString2Z(zone, value, length);

  if (result->_value._string.data == NULL) {
    TRI_Free(zone, result);
    return NULL;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a list object
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CreateListJson (TRI_memory_zone_t* zone) {
  TRI_json_t* result;

  result = (TRI_json_t*) TRI_Allocate(zone, sizeof(TRI_json_t), false);

  if (result == NULL) {
    return NULL;
  }

  result->_type = TRI_JSON_LIST;
  TRI_InitVector(&result->_value._objects, zone, sizeof(TRI_json_t));

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a list object, with the specified initial size
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CreateList2Json (TRI_memory_zone_t* zone,
                                 const size_t initialSize) {
  TRI_json_t* result;

  result = (TRI_json_t*) TRI_Allocate(zone, sizeof(TRI_json_t), false);

  if (result == NULL) {
    return NULL;
  }

  result->_type = TRI_JSON_LIST;
  TRI_InitVector2(&result->_value._objects, zone, sizeof(TRI_json_t), initialSize);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an object
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CreateArrayJson (TRI_memory_zone_t* zone) {
  TRI_json_t* result;

  result = (TRI_json_t*) TRI_Allocate(zone, sizeof(TRI_json_t), false);

  if (result == NULL) {
    return NULL;
  }

  result->_type = TRI_JSON_ARRAY;
  TRI_InitVector(&result->_value._objects, zone, sizeof(TRI_json_t));

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an object
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CreateArray2Json (TRI_memory_zone_t* zone,
                                  const size_t initialSize) {
  TRI_json_t* result;

  result = (TRI_json_t*) TRI_Allocate(zone, sizeof(TRI_json_t), false);

  if (result == NULL) {
    return NULL;
  }

  result->_type = TRI_JSON_ARRAY;
  TRI_InitVector2(&result->_value._objects, zone, sizeof(TRI_json_t), initialSize);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an array
////////////////////////////////////////////////////////////////////////////////

void TRI_InitArrayJson (TRI_memory_zone_t* zone, TRI_json_t* result) {
  result->_type = TRI_JSON_ARRAY;
  TRI_InitVector(&result->_value._objects, zone, sizeof(TRI_json_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an array, using a specific initial size
////////////////////////////////////////////////////////////////////////////////

void TRI_Init2ArrayJson (TRI_memory_zone_t* zone,
                         TRI_json_t* result,
                         size_t initialSize) {
  result->_type = TRI_JSON_ARRAY;
  TRI_InitVector2(&result->_value._objects, zone, sizeof(TRI_json_t), initialSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a json object, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyJson (TRI_memory_zone_t* zone, TRI_json_t* object) {
  size_t n;
  size_t i;

  switch (object->_type) {
    case TRI_JSON_UNUSED:
    case TRI_JSON_NULL:
    case TRI_JSON_BOOLEAN:
    case TRI_JSON_NUMBER:
      break;

    case TRI_JSON_STRING:
      TRI_DestroyBlob(zone, &object->_value._string);
      break;

    case TRI_JSON_ARRAY:
    case TRI_JSON_LIST:
      n = object->_value._objects._length;

      for (i = 0;  i < n;  ++i) {
        TRI_json_t* v = TRI_AtVector(&object->_value._objects, i);
        TRI_DestroyJson(zone, v);
      }

      TRI_DestroyVector(&object->_value._objects);
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a json object and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeJson (TRI_memory_zone_t* zone, TRI_json_t* object) {
  TRI_DestroyJson(zone, object);
  TRI_Free(zone, object);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new sub-object to a list object, copying it
////////////////////////////////////////////////////////////////////////////////

void TRI_PushBackListJson (TRI_memory_zone_t* zone, TRI_json_t* list, TRI_json_t* object) {
  TRI_json_t copy;

  assert(list->_type == TRI_JSON_LIST);

  TRI_CopyToJson(zone, &copy, object);

  TRI_PushBackVector(&list->_value._objects, &copy);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new sub-object to a list object, not copying it
////////////////////////////////////////////////////////////////////////////////

int TRI_PushBack2ListJson (TRI_json_t* list, TRI_json_t* object) {
  assert(list->_type == TRI_JSON_LIST);
  assert(object);

  return TRI_PushBackVector(&list->_value._objects, object);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new sub-object, not copying it but freeing the pointer
////////////////////////////////////////////////////////////////////////////////

int TRI_PushBack3ListJson (TRI_memory_zone_t* zone, TRI_json_t* list, TRI_json_t* object) {
  int res;

  if (object == NULL) {
    return TRI_ERROR_INTERNAL;
  }

  res = TRI_PushBack2ListJson(list, object);
  TRI_Free(zone, object);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a value in a json list
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_LookupListJson (const TRI_json_t* const object, const size_t pos) {
  size_t n;

  assert(object->_type == TRI_JSON_LIST);

  n = object->_value._objects._length;

  if (pos >= n) {
    // out of bounds
    return NULL;
  }

  return TRI_AtVector(&object->_value._objects, pos);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new attribute to an object, using copy
////////////////////////////////////////////////////////////////////////////////

void TRI_InsertArrayJson (TRI_memory_zone_t* zone,
                          TRI_json_t* object,
                          char const* name,
                          TRI_json_t* subobject) {
  TRI_json_t copy;
  size_t length;

  assert(object->_type == TRI_JSON_ARRAY);

  if (subobject == NULL) {
    return;
  }

  length = strlen(name);

  copy._type = TRI_JSON_STRING;
  copy._value._string.length = length + 1;
  copy._value._string.data = TRI_DuplicateString2Z(zone, name, length); // including '\0'

  TRI_PushBackVector(&object->_value._objects, &copy);

  TRI_CopyToJson(zone, &copy, subobject);
  TRI_PushBackVector(&object->_value._objects, &copy);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new attribute to an object, not copying it
////////////////////////////////////////////////////////////////////////////////

void TRI_Insert2ArrayJson (TRI_memory_zone_t* zone,
                           TRI_json_t* object,
                           char const* name,
                           TRI_json_t* subobject) {
  TRI_json_t copy;
  size_t length;

  assert(object->_type == TRI_JSON_ARRAY);

  if (subobject == NULL) {
    return;
  }

  length = strlen(name);

  copy._type = TRI_JSON_STRING;
  copy._value._string.length = length + 1;
  copy._value._string.data = TRI_DuplicateString2Z(zone, name, length); // including '\0'

  TRI_PushBackVector(&object->_value._objects, &copy);
  TRI_PushBackVector(&object->_value._objects, subobject);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new attribute, not copying it but freeing the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_Insert3ArrayJson (TRI_memory_zone_t* zone, TRI_json_t* object, char const* name, TRI_json_t* subobject) {
  TRI_Insert2ArrayJson(zone, object, name, subobject);
  TRI_Free(zone, subobject);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new attribute, not copying it but freeing the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_Insert4ArrayJson (TRI_memory_zone_t* zone, TRI_json_t* object, char* name, size_t nameLength, TRI_json_t* subobject) {
  TRI_json_t copy;

  copy._type = TRI_JSON_STRING;
  copy._value._string.length = nameLength + 1;
  copy._value._string.data = name;

  TRI_PushBackVector(&object->_value._objects, &copy);
  TRI_PushBackVector(&object->_value._objects, subobject);
  TRI_Free(zone, subobject);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute in an json array
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_LookupArrayJson (const TRI_json_t* const object, char const* name) {
  size_t n;
  size_t i;

  assert(object->_type == TRI_JSON_ARRAY);
  assert(name);

  n = object->_value._objects._length;

  for (i = 0;  i < n;  i += 2) {
    TRI_json_t* key;

    key = TRI_AtVector(&object->_value._objects, i);

    if (key->_type != TRI_JSON_STRING) {
      continue;
    }

    if (TRI_EqualString(key->_value._string.data, name)) {
      return TRI_AtVector(&object->_value._objects, i + 1);
    }
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an element from a json array
////////////////////////////////////////////////////////////////////////////////

bool TRI_DeleteArrayJson (TRI_memory_zone_t* zone, TRI_json_t* object, char const* name) {
  size_t n;
  size_t i;

  assert(object->_type == TRI_JSON_ARRAY);
  assert(name);

  n = object->_value._objects._length;

  for (i = 0;  i < n;  i += 2) {
    TRI_json_t* key;

    key = TRI_AtVector(&object->_value._objects, i);

    if (key->_type != TRI_JSON_STRING) {
      continue;
    }

    if (TRI_EqualString(key->_value._string.data, name)) {
      TRI_json_t* old;

      // remove key
      old = TRI_AtVector(&object->_value._objects, i);
      if (old != NULL) {
        TRI_DestroyJson(zone, old);
      }
      TRI_RemoveVector(&object->_value._objects, i);

      // remove value
      old = TRI_AtVector(&object->_value._objects, i);
      if (old != NULL) {
        TRI_DestroyJson(zone, old);
      }
      TRI_RemoveVector(&object->_value._objects, i);

      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces an element in a json array
////////////////////////////////////////////////////////////////////////////////

bool TRI_ReplaceArrayJson (TRI_memory_zone_t* zone, TRI_json_t* object, char const* name, TRI_json_t* replacement) {
  size_t n;
  size_t i;

  assert(object->_type == TRI_JSON_ARRAY);
  assert(name);

  n = object->_value._objects._length;

  for (i = 0;  i < n;  i += 2) {
    TRI_json_t* key;

    key = TRI_AtVector(&object->_value._objects, i);

    if (key->_type != TRI_JSON_STRING) {
      continue;
    }

    if (TRI_EqualString(key->_value._string.data, name)) {
      TRI_json_t copy;

      // retrieve the old element
      TRI_json_t* old = TRI_AtVector(&object->_value._objects, i + 1);
      if (old != NULL) {
        TRI_DestroyJson(zone, old);
      }

      TRI_CopyToJson(zone, &copy, replacement);
      TRI_SetVector(&object->_value._objects, i + 1, &copy);
      return true;
    }
  }

  // object not found in array, now simply add it
  TRI_Insert2ArrayJson(zone, object, name, replacement);

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringifies a json object
////////////////////////////////////////////////////////////////////////////////

int TRI_StringifyJson (TRI_string_buffer_t* buffer, TRI_json_t const* object) {
  return StringifyJson(buffer->_memoryZone, buffer, object, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringifies a json object skipping the outer braces
////////////////////////////////////////////////////////////////////////////////

int TRI_Stringify2Json (TRI_string_buffer_t* buffer, TRI_json_t const* object) {
  return StringifyJson(buffer->_memoryZone, buffer, object, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a json object
////////////////////////////////////////////////////////////////////////////////

bool TRI_PrintJson (int fd, TRI_json_t const* object) {
  TRI_string_buffer_t buffer;
  char const* p;
  size_t n;

  TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);
  StringifyJson(buffer._memoryZone, &buffer, object, true);

  p = TRI_BeginStringBuffer(&buffer);
  n = TRI_LengthStringBuffer(&buffer);

  while (0 < n) {
    ssize_t m = TRI_WRITE(fd, p, n);

    if (m <= 0) {
      TRI_AnnihilateStringBuffer(&buffer);
      return false;
    }

    n -= m;
    p += m;
  }

  TRI_AnnihilateStringBuffer(&buffer);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a json object
////////////////////////////////////////////////////////////////////////////////

bool TRI_SaveJson (char const* filename, 
                   TRI_json_t const* object, 
                   const bool syncFile) {
  char* tmp;
  int fd;
  int res;
  ssize_t m;

  tmp = TRI_Concatenate2String(filename, ".tmp");

  if (tmp == NULL) {
    return false;
  }

  fd = TRI_CREATE(tmp, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);

  if (fd < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG_ERROR("cannot create json file '%s': '%s'", tmp, TRI_LAST_ERROR_STR);
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);
    return false;
  }

  if (! TRI_PrintJson(fd, object)) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG_ERROR("cannot write to json file '%s': '%s'", tmp, TRI_LAST_ERROR_STR);
    TRI_UnlinkFile(tmp);
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);
    return false;
  }

  m = TRI_WRITE(fd, "\n", 1);

  if (m <= 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG_ERROR("cannot write to json file '%s': '%s'", tmp, TRI_LAST_ERROR_STR);
    TRI_UnlinkFile(tmp);
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);
    return false;
  }

  if (syncFile) {
    if (! TRI_fsync(fd)) {
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG_ERROR("cannot sync saved json '%s': '%s'", tmp, TRI_LAST_ERROR_STR);
      TRI_UnlinkFile(tmp);
      TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);
      return false;
    }
  }

  res = TRI_CLOSE(fd);

  if (res < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG_ERROR("cannot close saved file '%s': '%s'", tmp, TRI_LAST_ERROR_STR);
    TRI_UnlinkFile(tmp);
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);
    return false;
  }

  res = TRI_RenameFile(tmp, filename);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);
    LOG_ERROR("cannot rename saved file '%s' to '%s': '%s'", tmp, filename, TRI_LAST_ERROR_STR);
    TRI_UnlinkFile(tmp);
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);

    return false;
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a json object into a given buffer
////////////////////////////////////////////////////////////////////////////////

int TRI_CopyToJson (TRI_memory_zone_t* zone,
                    TRI_json_t* dst,
                    TRI_json_t const* src) {
  int res;
  size_t n;
  size_t i;

  dst->_type = src->_type;

  switch (src->_type) {
    case TRI_JSON_UNUSED:
    case TRI_JSON_NULL:
      break;

    case TRI_JSON_BOOLEAN:
      dst->_value._boolean = src->_value._boolean;
      break;

    case TRI_JSON_NUMBER:
      dst->_value._number = src->_value._number;
      break;

    case TRI_JSON_STRING:
      return TRI_CopyToBlob(zone, &dst->_value._string, &src->_value._string);

    case TRI_JSON_ARRAY:
    case TRI_JSON_LIST:
      n = src->_value._objects._length;

      TRI_InitVector(&dst->_value._objects, zone, sizeof(TRI_json_t));
      res = TRI_ResizeVector(&dst->_value._objects, n);

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }

      for (i = 0;  i < n;  ++i) {
        TRI_json_t* v = TRI_AtVector(&src->_value._objects, i);
        TRI_json_t* w = TRI_AtVector(&dst->_value._objects, i);

        res = TRI_CopyToJson(zone, w, v);

        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }
      }

      break;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a json object
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CopyJson (TRI_memory_zone_t* zone, const TRI_json_t* const src) {
  TRI_json_t* dst;
  int res;

  dst = (TRI_json_t*) TRI_Allocate(zone, sizeof(TRI_json_t), false);

  if (dst == NULL) {
    return NULL;
  }

  res = TRI_CopyToJson(zone, dst, src);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_Free(zone, dst);
    return NULL;
  }

  return dst;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief determines whether or not two json objects are equal
////////////////////////////////////////////////////////////////////////////////

bool TRI_EqualJsonJson (TRI_json_t* left, TRI_json_t* right) {
  if (left == NULL && right == NULL) {
    return true;
  }

  if (left == NULL || right == NULL) {
    return false;
  }

  if (left->_type != right->_type) {
    return false;
  }

  switch (left->_type) {
    case TRI_JSON_UNUSED: {
      return true;
    }

    case TRI_JSON_NULL: {
      return true;
    }

    case TRI_JSON_BOOLEAN: {
      return (left->_value._boolean == right->_value._boolean);
    }

    case TRI_JSON_NUMBER: {
      return (left->_value._number == right->_value._number);
    }

    case TRI_JSON_STRING: {
      return (strcmp(left->_value._string.data, right->_value._string.data) == 0);
    }

    case TRI_JSON_ARRAY: {
      size_t j;

      if (left->_value._objects._length != right->_value._objects._length) {
        return false;
      }

      for (j = 0;  j < (left->_value._objects._length / 2);  ++j) {
        TRI_json_t* leftName;
        TRI_json_t* leftValue;
        TRI_json_t* rightValue;

        leftName = (TRI_json_t*)(TRI_AtVector(&(left->_value._objects),2*j));
        if (leftName == NULL) {
          return false;
        }

        leftValue  = (TRI_json_t*)(TRI_AtVector(&(left->_value._objects),(2*j) + 1));
        rightValue = TRI_LookupArrayJson(right, leftName->_value._string.data);

        if (TRI_EqualJsonJson(leftValue, rightValue)) {
          continue;
        }
        return false;
      }

      return true;
    }

    case TRI_JSON_LIST: {
      size_t j;

      if (left->_value._objects._length != right->_value._objects._length) {
        return false;
      }

      for (j = 0; j < left->_value._objects._length; ++j) {
        TRI_json_t* subLeft;
        TRI_json_t* subRight;
        subLeft  = (TRI_json_t*)(TRI_AtVector(&(left->_value._objects),j));
        subRight = (TRI_json_t*)(TRI_AtVector(&(right->_value._objects),j));
        if (TRI_EqualJsonJson(subLeft, subRight)) {
          continue;
        }
        return false;
      }

      return true;
    }

    default: {
      assert(false);
    }
  }
  return false; // stops the vc++ compiler from complaining
}


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
