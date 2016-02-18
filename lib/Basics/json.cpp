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
////////////////////////////////////////////////////////////////////////////////

#include "json.h"
#include "Basics/files.h"
#include "Basics/Logger.h"
#include "Basics/StringBuffer.h"
#include "Basics/tri-strings.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a json object
////////////////////////////////////////////////////////////////////////////////

static int StringifyJson(TRI_memory_zone_t* zone, TRI_string_buffer_t* buffer,
                         TRI_json_t const* object, bool braces) {
  int res;

  switch (object->_type) {
    case TRI_JSON_UNUSED: {
      break;
    }

    case TRI_JSON_NULL: {
      res = TRI_AppendString2StringBuffer(buffer, "null", 4);  // strlen("null")

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }

      break;
    }

    case TRI_JSON_BOOLEAN: {
      if (object->_value._boolean) {
        res =
            TRI_AppendString2StringBuffer(buffer, "true", 4);  // strlen("true")
      } else {
        res = TRI_AppendString2StringBuffer(buffer, "false",
                                            5);  // strlen("false")
      }

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }

      break;
    }

    case TRI_JSON_NUMBER: {
      res = TRI_AppendDoubleStringBuffer(buffer, object->_value._number);

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }

      break;
    }

    case TRI_JSON_STRING:
    case TRI_JSON_STRING_REFERENCE: {
      res = TRI_AppendCharStringBuffer(buffer, '\"');

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }

      if (object->_value._string.length > 0) {
        // optimisation for the empty string
        res = TRI_AppendJsonEncodedStringStringBuffer(
            buffer, object->_value._string.data,
            object->_value._string.length - 1, false);

        if (res != TRI_ERROR_NO_ERROR) {
          return TRI_ERROR_OUT_OF_MEMORY;
        }
      }

      res = TRI_AppendCharStringBuffer(buffer, '\"');

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }

      break;
    }

    case TRI_JSON_OBJECT: {
      if (braces) {
        res = TRI_AppendCharStringBuffer(buffer, '{');

        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }
      }

      size_t const n = TRI_LengthVector(&object->_value._objects);

      for (size_t i = 0; i < n; i += 2) {
        if (0 < i) {
          res = TRI_AppendCharStringBuffer(buffer, ',');

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }
        }

        res = StringifyJson(zone, buffer,
                            static_cast<TRI_json_t const*>(
                                TRI_AtVector(&object->_value._objects, i)),
                            true);

        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }

        res = TRI_AppendCharStringBuffer(buffer, ':');

        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }

        res = StringifyJson(zone, buffer,
                            static_cast<TRI_json_t const*>(
                                TRI_AtVector(&object->_value._objects, i + 1)),
                            true);

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
    }

    case TRI_JSON_ARRAY: {
      if (braces) {
        res = TRI_AppendCharStringBuffer(buffer, '[');

        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }
      }

      size_t const n = TRI_LengthVector(&object->_value._objects);

      for (size_t i = 0; i < n; ++i) {
        if (0 < i) {
          res = TRI_AppendCharStringBuffer(buffer, ',');

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }
        }

        res = StringifyJson(zone, buffer,
                            static_cast<TRI_json_t const*>(
                                TRI_AtVector(&object->_value._objects, i)),
                            true);

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
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize a null object in place
////////////////////////////////////////////////////////////////////////////////

static inline void InitNull(TRI_json_t* result) {
  result->_type = TRI_JSON_NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize a boolean object in place
////////////////////////////////////////////////////////////////////////////////

static inline void InitBoolean(TRI_json_t* result, bool value) {
  result->_type = TRI_JSON_BOOLEAN;
  result->_value._boolean = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize a number object in place
////////////////////////////////////////////////////////////////////////////////

static inline void InitNumber(TRI_json_t* result, double value) {
  // check if the number can be represented in JSON
  if (std::isnan(value) || value == HUGE_VAL || value == -HUGE_VAL) {
    result->_type = TRI_JSON_NULL;
  } else {
    result->_type = TRI_JSON_NUMBER;
    result->_value._number = value;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize a string object in place
////////////////////////////////////////////////////////////////////////////////

static inline void InitString(TRI_json_t* result, char* value, size_t length) {
  TRI_ASSERT(value != nullptr);

  result->_type = TRI_JSON_STRING;
  result->_value._string.data = value;
  result->_value._string.length = (uint32_t)length + 1;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize a string reference object in place
////////////////////////////////////////////////////////////////////////////////

static inline void InitStringReference(TRI_json_t* result, char const* value,
                                       size_t length) {
  TRI_ASSERT(value != nullptr);

  result->_type = TRI_JSON_STRING_REFERENCE;
  result->_value._string.data = (char*)value;
  result->_value._string.length = (uint32_t)length + 1;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize an array in place
////////////////////////////////////////////////////////////////////////////////

static inline void InitArray(TRI_memory_zone_t* zone, TRI_json_t* result,
                             size_t initialSize) {
  result->_type = TRI_JSON_ARRAY;

  if (initialSize == 0) {
    TRI_InitVector(&result->_value._objects, zone, sizeof(TRI_json_t));
  } else {
    TRI_InitVector2(&result->_value._objects, zone, sizeof(TRI_json_t),
                    initialSize);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize an object in place
////////////////////////////////////////////////////////////////////////////////

static inline void InitObject(TRI_memory_zone_t* zone, TRI_json_t* result,
                              size_t initialSize) {
  result->_type = TRI_JSON_OBJECT;

  if (initialSize == 0) {
    TRI_InitVector(&result->_value._objects, zone, sizeof(TRI_json_t));
  } else {
    // need to allocate twice the space because for each array entry,
    // we need one object for the attribute key, and one for the attribute value
    TRI_InitVector2(&result->_value._objects, zone, sizeof(TRI_json_t),
                    2 * initialSize);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine whether a JSON value is of type string
////////////////////////////////////////////////////////////////////////////////

static inline bool IsString(TRI_json_t const* json) {
  return (json != nullptr && (json->_type == TRI_JSON_STRING ||
                              json->_type == TRI_JSON_STRING_REFERENCE) &&
          json->_value._string.data != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a null object
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CreateNullJson(TRI_memory_zone_t* zone) {
  TRI_json_t* result =
      static_cast<TRI_json_t*>(TRI_Allocate(zone, sizeof(TRI_json_t), false));

  if (result != nullptr) {
    InitNull(result);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a null object
////////////////////////////////////////////////////////////////////////////////

void TRI_InitNullJson(TRI_json_t* result) { InitNull(result); }

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a boolean object
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CreateBooleanJson(TRI_memory_zone_t* zone, bool value) {
  TRI_json_t* result =
      static_cast<TRI_json_t*>(TRI_Allocate(zone, sizeof(TRI_json_t), false));

  if (result != nullptr) {
    InitBoolean(result, value);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a boolean object
////////////////////////////////////////////////////////////////////////////////

void TRI_InitBooleanJson(TRI_json_t* result, bool value) {
  InitBoolean(result, value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a number object
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CreateNumberJson(TRI_memory_zone_t* zone, double value) {
  TRI_json_t* result =
      static_cast<TRI_json_t*>(TRI_Allocate(zone, sizeof(TRI_json_t), false));

  if (result != nullptr) {
    InitNumber(result, value);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a number object
////////////////////////////////////////////////////////////////////////////////

void TRI_InitNumberJson(TRI_json_t* result, double value) {
  InitNumber(result, value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a string object with given length
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CreateStringJson(TRI_memory_zone_t* zone, char* value,
                                 size_t length) {
  TRI_json_t* result =
      static_cast<TRI_json_t*>(TRI_Allocate(zone, sizeof(TRI_json_t), false));

  if (result != nullptr) {
    InitString(result, value, length);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a string object with given length, copying the string
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CreateStringCopyJson(TRI_memory_zone_t* zone, char const* value,
                                     size_t length) {
  if (value == nullptr) {
    // initial string should be valid...
    return nullptr;
  }
  TRI_json_t* result =
      static_cast<TRI_json_t*>(TRI_Allocate(zone, sizeof(TRI_json_t), false));

  if (result != nullptr) {
    char* copy = TRI_DuplicateString(zone, value, length);

    if (copy == nullptr) {
      TRI_Free(zone, result);
      return nullptr;
    }

    InitString(result, copy, length);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a string object
////////////////////////////////////////////////////////////////////////////////

void TRI_InitStringJson(TRI_json_t* result, char* value, size_t length) {
  InitString(result, value, length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a string object
////////////////////////////////////////////////////////////////////////////////

int TRI_InitStringCopyJson(TRI_memory_zone_t* zone, TRI_json_t* result,
                           char const* value, size_t length) {
  if (value == nullptr) {
    // initial string should be valid...
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  char* copy = TRI_DuplicateString(zone, value, length);

  if (copy == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  InitString(result, copy, length);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a string reference object with given length
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CreateStringReferenceJson(TRI_memory_zone_t* zone,
                                          char const* value, size_t length) {
  TRI_json_t* result =
      static_cast<TRI_json_t*>(TRI_Allocate(zone, sizeof(TRI_json_t), false));

  if (result != nullptr) {
    InitStringReference(result, value, length);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a string reference object
////////////////////////////////////////////////////////////////////////////////

void TRI_InitStringReferenceJson(TRI_json_t* result, char const* value,
                                 size_t length) {
  InitStringReference(result, value, length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an array object, with the specified initial size
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CreateArrayJson(TRI_memory_zone_t* zone, size_t initialSize) {
  TRI_json_t* result =
      static_cast<TRI_json_t*>(TRI_Allocate(zone, sizeof(TRI_json_t), false));

  if (result != nullptr) {
    InitArray(zone, result, initialSize);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes an array with a given size
////////////////////////////////////////////////////////////////////////////////

void TRI_InitArrayJson(TRI_memory_zone_t* zone, TRI_json_t* result,
                       size_t length) {
  InitArray(zone, result, length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an object
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CreateObjectJson(TRI_memory_zone_t* zone, size_t initialSize) {
  TRI_json_t* result =
      static_cast<TRI_json_t*>(TRI_Allocate(zone, sizeof(TRI_json_t), false));

  if (result != nullptr) {
    InitObject(zone, result, initialSize);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes an object, using a specific initial size
////////////////////////////////////////////////////////////////////////////////

void TRI_InitObjectJson(TRI_memory_zone_t* zone, TRI_json_t* result,
                        size_t initialSize) {
  InitObject(zone, result, initialSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a json object, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyJson(TRI_memory_zone_t* zone, TRI_json_t* object) {
  TRI_ASSERT(object != nullptr);

  switch (object->_type) {
    case TRI_JSON_UNUSED:
    case TRI_JSON_NULL:
    case TRI_JSON_BOOLEAN:
    case TRI_JSON_NUMBER:
    case TRI_JSON_STRING_REFERENCE:
      break;

    case TRI_JSON_STRING:
      TRI_DestroyBlob(zone, &object->_value._string);
      break;

    case TRI_JSON_OBJECT:
    case TRI_JSON_ARRAY: {
      size_t const n = TRI_LengthVector(&object->_value._objects);

      for (size_t i = 0; i < n; ++i) {
        auto v = static_cast<TRI_json_t*>(
            TRI_AddressVector(&object->_value._objects, i));
        TRI_DestroyJson(zone, v);
      }

      TRI_DestroyVector(&object->_value._objects);
      break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a json object and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeJson(TRI_memory_zone_t* zone, TRI_json_t* object) {
  TRI_ASSERT(object != nullptr);
  TRI_DestroyJson(zone, object);
  TRI_Free(zone, object);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a user printable string
////////////////////////////////////////////////////////////////////////////////

char const* TRI_GetTypeStringJson(TRI_json_t const* object) {
  switch (object->_type) {
    case TRI_JSON_UNUSED:
      return "unused";
    case TRI_JSON_NULL:
      return "null";
    case TRI_JSON_BOOLEAN:
      return "boolean";
    case TRI_JSON_NUMBER:
      return "number";
    case TRI_JSON_STRING:
      return "string";
    case TRI_JSON_STRING_REFERENCE:
      return "string-reference";
    case TRI_JSON_OBJECT:
      return "object";
    case TRI_JSON_ARRAY:
      return "array";
  }
  return "unknown";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines the length of an array json
////////////////////////////////////////////////////////////////////////////////

size_t TRI_LengthArrayJson(TRI_json_t const* json) {
  TRI_ASSERT(json != nullptr && json->_type == TRI_JSON_ARRAY);
  return TRI_LengthVector(&json->_value._objects);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines the length of the json's objects vector
////////////////////////////////////////////////////////////////////////////////

size_t TRI_LengthVectorJson(TRI_json_t const* json) {
  TRI_ASSERT(json != nullptr &&
             (json->_type == TRI_JSON_ARRAY || json->_type == TRI_JSON_OBJECT));
  return TRI_LengthVector(&json->_value._objects);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines whether the JSON passed is of type string
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsStringJson(TRI_json_t const* json) { return IsString(json); }

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new sub-object to an array, copying it
////////////////////////////////////////////////////////////////////////////////

void TRI_PushBackArrayJson(TRI_memory_zone_t* zone, TRI_json_t* array,
                           TRI_json_t const* object) {
  TRI_ASSERT(array->_type == TRI_JSON_ARRAY);

  TRI_json_t* dst =
      static_cast<TRI_json_t*>(TRI_NextVector(&array->_value._objects));

  if (dst == nullptr) {
    // out of memory
    return;
  }

  // directly copy value into the obtained address
  TRI_CopyToJson(zone, dst, object);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new sub-object to an array, not copying it
////////////////////////////////////////////////////////////////////////////////

int TRI_PushBack2ArrayJson(TRI_json_t* array, TRI_json_t const* object) {
  TRI_ASSERT(array != nullptr);
  TRI_ASSERT(array->_type == TRI_JSON_ARRAY);
  TRI_ASSERT(object != nullptr);

  return TRI_PushBackVector(&array->_value._objects, object);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new sub-object, not copying it but freeing the pointer
////////////////////////////////////////////////////////////////////////////////

int TRI_PushBack3ArrayJson(TRI_memory_zone_t* zone, TRI_json_t* array,
                           TRI_json_t* object) {
  if (object == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  TRI_ASSERT(array != nullptr);

  int res = TRI_PushBack2ArrayJson(array, object);
  TRI_Free(zone, object);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a value in a json array
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_LookupArrayJson(TRI_json_t const* array, size_t pos) {
  if (array == nullptr) {
    return nullptr;
  }
  TRI_ASSERT(array->_type == TRI_JSON_ARRAY);

  size_t const n = TRI_LengthVector(&array->_value._objects);

  if (pos >= n) {
    // out of bounds
    return nullptr;
  }

  return static_cast<TRI_json_t*>(TRI_AtVector(&array->_value._objects, pos));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an element from a json array
////////////////////////////////////////////////////////////////////////////////

bool TRI_DeleteArrayJson(TRI_memory_zone_t* zone, TRI_json_t* array,
                         size_t index) {
  TRI_ASSERT(TRI_IsArrayJson(array));

  size_t const n = TRI_LengthArrayJson(array);

  if (index >= n) {
    return false;
  }

  TRI_json_t* element =
      static_cast<TRI_json_t*>(TRI_AtVector(&array->_value._objects, index));
  TRI_ASSERT(element != nullptr);
  TRI_DestroyJson(zone, element);
  TRI_RemoveVector(&array->_value._objects, index);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new attribute to an object, using copy
////////////////////////////////////////////////////////////////////////////////

void TRI_InsertObjectJson(TRI_memory_zone_t* zone, TRI_json_t* object,
                          char const* name, TRI_json_t const* subobject) {
  TRI_ASSERT(object->_type == TRI_JSON_OBJECT);

  if (subobject == nullptr) {
    return;
  }

  if (TRI_ReserveVector(&object->_value._objects, 2) != TRI_ERROR_NO_ERROR) {
    return;
  }

  // attribute name
  size_t length = strlen(name);
  char* att = TRI_DuplicateString(zone, name, length);

  if (att == nullptr) {
    return;
  }

  // create attribute name in place
  TRI_json_t* next =
      static_cast<TRI_json_t*>(TRI_NextVector(&object->_value._objects));
  // we have made sure above with the reserve that the vector has enough
  // capacity
  TRI_ASSERT(next != nullptr);
  InitString(next, att, length);

  // attribute value
  next = static_cast<TRI_json_t*>(TRI_NextVector(&object->_value._objects));
  // we have made sure above with the reserve call that the vector has enough
  // capacity
  TRI_ASSERT(next != nullptr);

  TRI_CopyToJson(zone, next, subobject);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new attribute to an object, not copying it
////////////////////////////////////////////////////////////////////////////////

void TRI_Insert2ObjectJson(TRI_memory_zone_t* zone, TRI_json_t* object,
                           char const* name, TRI_json_t const* subobject) {
  TRI_ASSERT(object->_type == TRI_JSON_OBJECT);

  if (subobject == nullptr) {
    return;
  }

  if (TRI_ReserveVector(&object->_value._objects, 2) != TRI_ERROR_NO_ERROR) {
    return;
  }

  size_t length = strlen(name);
  char* att = TRI_DuplicateString(zone, name, length);

  if (att == nullptr) {
    return;
  }

  // create attribute name in place
  TRI_json_t* next =
      static_cast<TRI_json_t*>(TRI_NextVector(&object->_value._objects));
  // we have made sure above with the reserve call that the vector has enough
  // capacity
  TRI_ASSERT(next != nullptr);
  InitString(next, att, length);

  // attribute value
  TRI_PushBackVector(&object->_value._objects, subobject);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new attribute, not copying it but freeing the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_Insert3ObjectJson(TRI_memory_zone_t* zone, TRI_json_t* object,
                           char const* name, TRI_json_t* subobject) {
  if (object != nullptr && subobject != nullptr) {
    TRI_Insert2ObjectJson(zone, object, name, subobject);
    TRI_Free(zone, subobject);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute in an json object
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_LookupObjectJson(TRI_json_t const* object, char const* name) {
  if (object == nullptr) {
    return nullptr;
  }

  TRI_ASSERT(object->_type == TRI_JSON_OBJECT);
  TRI_ASSERT(name != nullptr);

  size_t const n = TRI_LengthVector(&object->_value._objects);

  if (n >= 16) {
    // if we have many attributes, try an algorithm that compares string length
    // first

    // calculate string length once
    size_t const length = strlen(name) + 1;

    for (size_t i = 0; i < n; i += 2) {
      auto key = static_cast<TRI_json_t const*>(
          TRI_AddressVector(&object->_value._objects, i));

      if (!IsString(key)) {
        continue;
      }

      // check string length first, and contents only if string lengths are
      // equal
      if (key->_value._string.length == length &&
          strcmp(key->_value._string.data, name) == 0) {
        return static_cast<TRI_json_t*>(
            TRI_AddressVector(&object->_value._objects, i + 1));
      }
    }
  } else {
    // simple case for smaller objects
    for (size_t i = 0; i < n; i += 2) {
      auto key = static_cast<TRI_json_t const*>(
          TRI_AddressVector(&object->_value._objects, i));

      if (!IsString(key)) {
        continue;
      }

      if (strcmp(key->_value._string.data, name) == 0) {
        return static_cast<TRI_json_t*>(
            TRI_AddressVector(&object->_value._objects, i + 1));
      }
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an element from a json object
////////////////////////////////////////////////////////////////////////////////

bool TRI_DeleteObjectJson(TRI_memory_zone_t* zone, TRI_json_t* object,
                          char const* name) {
  TRI_ASSERT(object->_type == TRI_JSON_OBJECT);
  TRI_ASSERT(name != nullptr);

  size_t const n = TRI_LengthVector(&object->_value._objects);

  for (size_t i = 0; i < n; i += 2) {
    TRI_json_t* key =
        static_cast<TRI_json_t*>(TRI_AtVector(&object->_value._objects, i));

    if (!IsString(key)) {
      continue;
    }

    if (TRI_EqualString(key->_value._string.data, name)) {
      // remove key
      TRI_json_t* old =
          static_cast<TRI_json_t*>(TRI_AtVector(&object->_value._objects, i));

      if (old != nullptr) {
        TRI_DestroyJson(zone, old);
      }

      TRI_RemoveVector(&object->_value._objects, i);

      // remove value
      old = static_cast<TRI_json_t*>(TRI_AtVector(&object->_value._objects, i));

      if (old != nullptr) {
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

bool TRI_ReplaceObjectJson(TRI_memory_zone_t* zone, TRI_json_t* object,
                           char const* name, TRI_json_t const* replacement) {
  TRI_ASSERT(replacement != nullptr);
  TRI_ASSERT(object != nullptr);
  TRI_ASSERT(object->_type == TRI_JSON_OBJECT);
  TRI_ASSERT(name != nullptr);

  size_t const n = TRI_LengthVector(&object->_value._objects);

  for (size_t i = 0; i < n; i += 2) {
    TRI_json_t* key =
        static_cast<TRI_json_t*>(TRI_AtVector(&object->_value._objects, i));

    if (!IsString(key)) {
      continue;
    }

    if (TRI_EqualString(key->_value._string.data, name)) {
      // retrieve the old element
      TRI_json_t* old = static_cast<TRI_json_t*>(
          TRI_AtVector(&object->_value._objects, i + 1));

      if (old != nullptr) {
        TRI_DestroyJson(zone, old);
      }

      TRI_json_t copy;
      TRI_CopyToJson(zone, &copy, replacement);
      TRI_SetVector(&object->_value._objects, i + 1, &copy);
      return true;
    }
  }

  // object not found in array, now simply add it
  TRI_Insert2ObjectJson(zone, object, name, replacement);

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringifies a json object
////////////////////////////////////////////////////////////////////////////////

int TRI_StringifyJson(TRI_string_buffer_t* buffer, TRI_json_t const* object) {
  return StringifyJson(buffer->_memoryZone, buffer, object, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringifies a json object skipping the outer braces
////////////////////////////////////////////////////////////////////////////////

int TRI_Stringify2Json(TRI_string_buffer_t* buffer, TRI_json_t const* object) {
  return StringifyJson(buffer->_memoryZone, buffer, object, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a json object
////////////////////////////////////////////////////////////////////////////////

bool TRI_PrintJson(int fd, TRI_json_t const* object, bool appendNewline) {
  if (object == nullptr) {
    // sanity check
    return false;
  }

  TRI_string_buffer_t buffer;
  TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);
  if (StringifyJson(buffer._memoryZone, &buffer, object, true) !=
      TRI_ERROR_NO_ERROR) {
    TRI_AnnihilateStringBuffer(&buffer);
    return false;
  }

  if (TRI_LengthStringBuffer(&buffer) == 0) {
    // should not happen
    return false;
  }

  if (appendNewline) {
    // add the newline here so we only need one write operation in the ideal
    // case
    TRI_AppendCharStringBuffer(&buffer, '\n');
  }

  char const* p = TRI_BeginStringBuffer(&buffer);
  size_t n = TRI_LengthStringBuffer(&buffer);

  while (0 < n) {
    ssize_t m = TRI_WRITE(fd, p, (TRI_write_t)n);

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

bool TRI_SaveJson(char const* filename, TRI_json_t const* object,
                  bool syncFile) {
  char* tmp = TRI_Concatenate2String(filename, ".tmp");

  if (tmp == nullptr) {
    return false;
  }

  // remove a potentially existing temporary file
  if (TRI_ExistsFile(tmp)) {
    TRI_UnlinkFile(tmp);
  }

  int fd = TRI_CREATE(tmp, O_CREAT | O_TRUNC | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

  if (fd < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG(ERR) << "cannot create json file '" << tmp << "': " << TRI_LAST_ERROR_STR;
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);
    return false;
  }

  if (!TRI_PrintJson(fd, object, true)) {
    TRI_CLOSE(fd);
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG(ERR) << "cannot write to json file '" << tmp << "': " << TRI_LAST_ERROR_STR;
    TRI_UnlinkFile(tmp);
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);
    return false;
  }

  if (syncFile) {
    LOG(TRACE) << "syncing tmp file '" << tmp << "'";

    if (!TRI_fsync(fd)) {
      TRI_CLOSE(fd);
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG(ERR) << "cannot sync saved json '" << tmp << "': " << TRI_LAST_ERROR_STR;
      TRI_UnlinkFile(tmp);
      TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);
      return false;
    }
  }

  int res = TRI_CLOSE(fd);

  if (res < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG(ERR) << "cannot close saved file '" << tmp << "': " << TRI_LAST_ERROR_STR;
    TRI_UnlinkFile(tmp);
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);
    return false;
  }

  res = TRI_RenameFile(tmp, filename);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);
    LOG(ERR) << "cannot rename saved file '" << tmp << "' to '" << filename << "': " << TRI_LAST_ERROR_STR;
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

int TRI_CopyToJson(TRI_memory_zone_t* zone, TRI_json_t* dst,
                   TRI_json_t const* src) {
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

    case TRI_JSON_STRING_REFERENCE:
      return TRI_AssignToBlob(zone, &dst->_value._string, &src->_value._string);

    case TRI_JSON_OBJECT:
    case TRI_JSON_ARRAY: {
      size_t const n = TRI_LengthVector(&src->_value._objects);

      TRI_InitVector(&dst->_value._objects, zone, sizeof(TRI_json_t));
      int res = TRI_ResizeVector(&dst->_value._objects, n);

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }

      for (size_t i = 0; i < n; ++i) {
        auto const* v = static_cast<TRI_json_t const*>(
            TRI_AtVector(&src->_value._objects, i));
        auto* w =
            static_cast<TRI_json_t*>(TRI_AtVector(&dst->_value._objects, i));

        res = TRI_CopyToJson(zone, w, v);

        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }
      }

      break;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a json object
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CopyJson(TRI_memory_zone_t* zone, TRI_json_t const* src) {
  TRI_ASSERT(src != nullptr);

  TRI_json_t* dst =
      static_cast<TRI_json_t*>(TRI_Allocate(zone, sizeof(TRI_json_t), false));

  if (dst != nullptr) {
    int res = TRI_CopyToJson(zone, dst, src);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_Free(zone, dst);
      return nullptr;
    }
  }

  return dst;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a json object into a number
////////////////////////////////////////////////////////////////////////////////

int64_t TRI_ToInt64Json(TRI_json_t const* json) {
  TRI_ASSERT(json != nullptr);

  switch (json->_type) {
    case TRI_JSON_UNUSED:
    case TRI_JSON_NULL:
      return 0;
    case TRI_JSON_BOOLEAN:
      return (json->_value._boolean ? 1 : 0);
    case TRI_JSON_NUMBER:
      return static_cast<int64_t>(json->_value._number);
    case TRI_JSON_STRING:
    case TRI_JSON_STRING_REFERENCE:
      try {
        // try converting string to number
        double v = std::stod(json->_value._string.data);
        return static_cast<int64_t>(v);
      } catch (...) {
        // conversion failed
      }
      break;
    case TRI_JSON_ARRAY: {
      size_t const n = TRI_LengthArrayJson(json);

      if (n == 0) {
        return 0;
      } else if (n == 1) {
        return TRI_ToInt64Json(TRI_LookupArrayJson(json, 0));
      }
      break;
    }
    case TRI_JSON_OBJECT:
      break;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a json object into a number
////////////////////////////////////////////////////////////////////////////////

double TRI_ToDoubleJson(TRI_json_t const* json, bool& failed) {
  TRI_ASSERT(json != nullptr);

  failed = false;
  switch (json->_type) {
    case TRI_JSON_UNUSED:
    case TRI_JSON_NULL:
      return 0.0;
    case TRI_JSON_BOOLEAN:
      return (json->_value._boolean ? 1.0 : 0.0);
    case TRI_JSON_NUMBER:
      return json->_value._number;
    case TRI_JSON_STRING:
    case TRI_JSON_STRING_REFERENCE:
      try {
        // try converting string to number
        double v = std::stod(json->_value._string.data);
        return v;
      } catch (...) {
        if (strlen(json->_value._string.data) == 0) {
          return 0.0;
        }
        // conversion failed
      }
      break;
    case TRI_JSON_ARRAY: {
      size_t const n = TRI_LengthArrayJson(json);

      if (n == 0) {
        return 0.0;
      } else if (n == 1) {
        return TRI_ToDoubleJson(TRI_LookupArrayJson(json, 0), failed);
      }
      break;
    }
    case TRI_JSON_OBJECT:
      break;
  }

  failed = true;
  return 0.0;
}
