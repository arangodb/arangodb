////////////////////////////////////////////////////////////////////////////////
/// @brief vector implementation
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

#include "vector.h"

#include "BasicsC/tri-strings.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief grow rate
////////////////////////////////////////////////////////////////////////////////

#define GROW_FACTOR (1.2)

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       POD VECTORS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a vector
////////////////////////////////////////////////////////////////////////////////

void TRI_InitVector (TRI_vector_t* vector, TRI_memory_zone_t* zone, size_t elementSize) {
  vector->_memoryZone      = zone;
  vector->_elementSize     = elementSize;
  vector->_buffer          = NULL;
  vector->_length          = 0;
  vector->_capacity        = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a vector, with user-definable settings
////////////////////////////////////////////////////////////////////////////////

int TRI_InitVector2 (TRI_vector_t* vector,
                     TRI_memory_zone_t* zone,
                     size_t elementSize,
                     size_t initialCapacity) {
  // init vector as usual
  TRI_InitVector(vector, zone, elementSize);

  if (initialCapacity != 0) {
    vector->_buffer = (char*) TRI_Allocate(vector->_memoryZone, (initialCapacity * vector->_elementSize), false);

    if (vector->_buffer == NULL) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }
  }

  vector->_capacity = initialCapacity;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVector (TRI_vector_t* vector) {
  if (vector->_buffer != NULL) {
    TRI_Free(vector->_memoryZone, vector->_buffer);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVector (TRI_memory_zone_t* zone, TRI_vector_t* vector) {
  TRI_DestroyVector(vector);
  TRI_Free(zone, vector);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a vector
////////////////////////////////////////////////////////////////////////////////

TRI_vector_t* TRI_CopyVector (TRI_memory_zone_t* zone, TRI_vector_t* vector) {
  TRI_vector_t* copy;

  copy = TRI_Allocate(zone, sizeof(TRI_vector_t), false);

  if (copy == NULL) {
    return NULL;
  }

  copy->_elementSize = vector->_elementSize;
  copy->_memoryZone = zone;

  if (vector->_capacity == 0) {
    copy->_buffer = NULL;
    copy->_length = 0;
    copy->_capacity = 0;
  }
  else {
    copy->_buffer = TRI_Allocate(zone, vector->_length * vector->_elementSize, false);

    if (copy->_buffer == NULL) {
      TRI_Free(zone, copy);
      return NULL;
    }

    copy->_capacity = vector->_length;
    copy->_length = vector->_length;

    memcpy(copy->_buffer, vector->_buffer, vector->_length * vector->_elementSize);
  }

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy data from one vector into another
////////////////////////////////////////////////////////////////////////////////

int TRI_CopyDataVector (TRI_vector_t* dest, const TRI_vector_t* const source) {
  if (dest->_elementSize != source->_elementSize) {
    return TRI_ERROR_INTERNAL;
  }

  if (dest->_buffer != NULL) {
    TRI_Free(dest->_memoryZone, dest->_buffer);
    dest->_buffer = NULL;
  }

  dest->_capacity = 0;
  dest->_length = 0;

  if (source->_length > 0) {
    dest->_buffer = TRI_Allocate(dest->_memoryZone, source->_length * source->_elementSize, false);
    if (dest->_buffer == NULL) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    memcpy(dest->_buffer, source->_buffer, source->_length * source->_elementSize);
    dest->_capacity = source->_length;
    dest->_length = source->_length;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if the vector is empty
////////////////////////////////////////////////////////////////////////////////

bool TRI_EmptyVector (TRI_vector_t const* vector) {
  return vector->_length == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the vector
////////////////////////////////////////////////////////////////////////////////

void TRI_ClearVector (TRI_vector_t* vector) {
  vector->_length = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the vector
////////////////////////////////////////////////////////////////////////////////

int TRI_ResizeVector (TRI_vector_t* vector, size_t n) {
  if (vector->_length == n) {
    return TRI_ERROR_NO_ERROR;
  }

  if (vector->_capacity < n) {
    char* newBuffer;
    size_t newSize = n;

    newBuffer = (char*) TRI_Reallocate(vector->_memoryZone, vector->_buffer, newSize * vector->_elementSize);

    if (newBuffer == NULL) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    vector->_capacity = n;
    vector->_buffer = newBuffer;
  }

  vector->_length = n;
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures there is spare capacity at the end of the vector
////////////////////////////////////////////////////////////////////////////////

int TRI_EnsureSpareCapacityVector (TRI_vector_t* vector, size_t spare) {
  int res;

  if (vector->_length + spare <= vector->_capacity) {
    // vector is already big enough
    res = TRI_ERROR_NO_ERROR;
  }
  else {
    // vector is too small. resize
    res = TRI_ResizeVector(vector, vector->_length + spare);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element at the end
////////////////////////////////////////////////////////////////////////////////

int TRI_PushBackVector (TRI_vector_t* vector, void const* element) {
  if (vector->_length == vector->_capacity) {
    char* newBuffer;
    size_t newSize = (size_t) (1 + (GROW_FACTOR * vector->_capacity));

    newBuffer = (char*) TRI_Reallocate(vector->_memoryZone, vector->_buffer, newSize * vector->_elementSize);

    if (newBuffer == NULL) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    vector->_capacity = newSize;
    vector->_buffer = newBuffer;
  }

  memcpy(vector->_buffer + vector->_length * vector->_elementSize, element, vector->_elementSize);

  vector->_length++;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveVector (TRI_vector_t* vector, size_t n) {
  if (n < vector->_length) {
    if (n + 1 < vector->_length) {
      memmove(vector->_buffer + n * vector->_elementSize,
              vector->_buffer + (n + 1) * vector->_elementSize,
              (vector->_length - n - 1) * vector->_elementSize);
    }

    --vector->_length;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the element at a given position
////////////////////////////////////////////////////////////////////////////////

void* TRI_AtVector (TRI_vector_t const* vector, size_t pos) {
  if (vector->_buffer == NULL || pos >= vector->_length) {
    return 0;
  }

  return (void*) (vector->_buffer + pos * vector->_elementSize);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief inserts an element at a given position
////////////////////////////////////////////////////////////////////////////////

void TRI_InsertVector (TRI_vector_t* vector, void const* element, size_t position) {
  char* newBuffer;

  // ...........................................................................
  // Check and see if we need to extend the vector
  // ...........................................................................

  if (vector->_length >= vector->_capacity || position >= vector->_length) {
    size_t newSize = (size_t) (1 + (GROW_FACTOR * vector->_capacity));

    if (position >= newSize) {
      newSize = position + 1;
    }

    newBuffer = (char*) TRI_Allocate(vector->_memoryZone, newSize * vector->_elementSize, false);

    if (newBuffer == NULL) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return;
    }

    vector->_capacity = newSize;

    if (vector->_buffer != NULL) {
      memcpy(newBuffer, vector->_buffer, vector->_length * vector->_elementSize);
      TRI_Free(vector->_memoryZone, vector->_buffer);
    }

    vector->_buffer = newBuffer;
  }

  if (position < vector->_length) {
    memmove(vector->_buffer + (vector->_elementSize * (position + 1)),
            vector->_buffer + (vector->_elementSize * position),
            vector->_elementSize * (vector->_length - position)
           );
    vector->_length += 1;
  }
  else {
    vector->_length = position + 1;
  }

  memcpy(vector->_buffer + (vector->_elementSize * position), element, vector->_elementSize);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief sets an element at a given position
////////////////////////////////////////////////////////////////////////////////

void TRI_SetVector (TRI_vector_t* vector, size_t pos, void const* element) {
  if (pos < vector->_length) {
    memcpy((void*)(vector->_buffer + pos * vector->_elementSize), element, vector->_elementSize);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the beginning
////////////////////////////////////////////////////////////////////////////////

void* TRI_BeginVector (TRI_vector_t* vector) {
  return vector->_buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the end (pointer after the last element)
////////////////////////////////////////////////////////////////////////////////

void* TRI_EndVector (TRI_vector_t* vector) {
  return vector->_buffer + vector->_length * vector->_elementSize;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   POINTER VECTORS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a vector
////////////////////////////////////////////////////////////////////////////////

void TRI_InitVectorPointer (TRI_vector_pointer_t* vector,
                            TRI_memory_zone_t* zone) {
  vector->_memoryZone = zone;
  vector->_buffer = NULL;
  vector->_length = 0;
  vector->_capacity = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a vector, with user-definable settings
////////////////////////////////////////////////////////////////////////////////

int TRI_InitVectorPointer2 (TRI_vector_pointer_t* vector,
                            TRI_memory_zone_t* zone,
                            size_t initialCapacity) {
  TRI_InitVectorPointer(vector, zone);

  if (initialCapacity != 0) {
    vector->_buffer = (void*) TRI_Allocate(vector->_memoryZone, (initialCapacity * sizeof(void*)), true);
    if (vector->_buffer == NULL) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }
  }

  vector->_capacity = initialCapacity;
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVectorPointer (TRI_vector_pointer_t* vector) {
  if (vector->_buffer != NULL) {
    TRI_Free(vector->_memoryZone, vector->_buffer);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVectorPointer (TRI_memory_zone_t* zone, TRI_vector_pointer_t* vector) {
  TRI_DestroyVectorPointer(vector);
  TRI_Free(zone, vector);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector and frees the pointer and the content
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeContentVectorPointer (TRI_memory_zone_t* zone,
                                   TRI_vector_pointer_t* vector) {
  size_t i;
  void* ptr;

  for (i = 0;  i < vector->_length;  ++i) {
    ptr = vector->_buffer[i];

    if (ptr != NULL) {
      TRI_Free(zone, ptr);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a vector
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_CopyVectorPointer (TRI_memory_zone_t* zone,
                                             TRI_vector_pointer_t* vector) {
  TRI_vector_pointer_t* copy = TRI_Allocate(zone, sizeof(TRI_vector_pointer_t), false);

  if (copy == NULL) {
    return NULL;
  }

  copy->_memoryZone = zone;

  if (vector->_capacity == 0) {
    copy->_buffer = NULL;
    copy->_length = 0;
    copy->_capacity = 0;
  }
  else {
    copy->_buffer = TRI_Allocate(zone, vector->_length * sizeof(void*), false);

    if (copy->_buffer == NULL) {
      TRI_Free(zone, copy);
      return NULL;
    }

    copy->_length = vector->_length;
    copy->_capacity = vector->_length;

    memcpy(copy->_buffer, vector->_buffer, vector->_length * sizeof(void*));
  }

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies all pointers from a vector
////////////////////////////////////////////////////////////////////////////////

int TRI_CopyDataVectorPointer (TRI_vector_pointer_t* dst, TRI_vector_pointer_t* src) {
  if (src->_length == 0) {
    dst->_length = 0;
  }
  else {
    int res = TRI_ResizeVectorPointer(dst, src->_length);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    memcpy(dst->_buffer, src->_buffer, src->_length * sizeof(void*));
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if the vector is empty
////////////////////////////////////////////////////////////////////////////////

bool TRI_EmptyVectorPointer (TRI_vector_pointer_t* vector) {
  return vector->_length == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the vector
////////////////////////////////////////////////////////////////////////////////

void TRI_ClearVectorPointer (TRI_vector_pointer_t* vector) {
  vector->_length = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the vector
////////////////////////////////////////////////////////////////////////////////

int TRI_ResizeVectorPointer (TRI_vector_pointer_t* vector, size_t n) {
  if (vector->_length == n) {
    return TRI_ERROR_NO_ERROR;
  }

  if (vector->_capacity < n) {
    void* newBuffer;
    size_t newSize = n;

    newBuffer = TRI_Reallocate(vector->_memoryZone, vector->_buffer, newSize * sizeof(void*));

    if (newBuffer == NULL) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    vector->_capacity = newSize;
    vector->_buffer = newBuffer;
  }

  vector->_length = n;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element at the end
////////////////////////////////////////////////////////////////////////////////

int TRI_PushBackVectorPointer (TRI_vector_pointer_t* vector, void* element) {
  if (vector->_length == vector->_capacity) {
    void* newBuffer;
    size_t newSize = (size_t) (1 + GROW_FACTOR * vector->_capacity);

    newBuffer = TRI_Reallocate(vector->_memoryZone, vector->_buffer, newSize * sizeof(void*));

    if (newBuffer == NULL) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    vector->_buffer = newBuffer;
    // prefill the new vector elements with 0's
    memset(&vector->_buffer[vector->_capacity], 0, (newSize - vector->_capacity) * sizeof(void*));
    vector->_capacity = newSize;
  }

  vector->_buffer[vector->_length++] = element;
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts an element at position n, shifting the following elements
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertVectorPointer (TRI_vector_pointer_t* vector, void* element, size_t n) {

  // ...........................................................................
  // Check and see if we need to extend the vector
  // ...........................................................................

  if (vector->_length >= vector->_capacity || n >= vector->_length) {
    void* newBuffer;
    size_t newSize = (size_t) (1 + GROW_FACTOR * vector->_capacity);

    if (n >= newSize) {
      newSize = n + 1;
    }

    newBuffer = TRI_Reallocate(vector->_memoryZone, vector->_buffer, newSize * sizeof(void*));

    if (newBuffer == NULL) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    vector->_buffer = newBuffer;
    // prefill the new vector elements with 0's
    memset(&vector->_buffer[vector->_capacity], 0, (newSize - vector->_capacity) * sizeof(void*));
    vector->_capacity = newSize;
  }

  if (n < vector->_length) {
    memmove(vector->_buffer + n + 1,
            vector->_buffer + n,
            sizeof(void*) * (vector->_length - n));
  }

  vector->_buffer[n] = element;

  if (n > vector->_length) {
    vector->_length = n + 1;
  }
  else {
    ++vector->_length;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element, returns this element
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveVectorPointer (TRI_vector_pointer_t* vector, size_t n) {
  void* old;

  old = 0;

  if (n < vector->_length) {
    old = vector->_buffer[n];

    if (n + 1 < vector->_length) {
      memmove(vector->_buffer + n,
              vector->_buffer + (n + 1),
              (vector->_length - n - 1) * sizeof(void*));
    }

    --vector->_length;
  }

  return old;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the element at a given position
////////////////////////////////////////////////////////////////////////////////

void* TRI_AtVectorPointer (TRI_vector_pointer_t const* vector, size_t pos) {
  if (vector->_buffer == NULL || pos >= vector->_length) {
    return 0;
  }

  return vector->_buffer[pos];
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    STRING VECTORS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a string vector
////////////////////////////////////////////////////////////////////////////////

void TRI_InitVectorString (TRI_vector_string_t* vector, TRI_memory_zone_t* zone) {
  vector->_memoryZone = zone;
  vector->_buffer = NULL;
  vector->_length = 0;
  vector->_capacity = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a string vector, with user-definable settings
////////////////////////////////////////////////////////////////////////////////

int TRI_InitVectorString2 (TRI_vector_string_t* vector,
                           TRI_memory_zone_t* zone,
                           size_t initialCapacity) {
  TRI_InitVectorString(vector, zone);

  if (initialCapacity != 0) {
    vector->_buffer = (char**) TRI_Allocate(vector->_memoryZone, initialCapacity * sizeof(char*), false);
    if (vector->_buffer == NULL) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }
  }

  vector->_capacity = initialCapacity;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector and all strings, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVectorString (TRI_vector_string_t* vector) {
  if (vector->_buffer != NULL) {
    size_t i;

    for (i = 0;  i < vector->_length;  ++i) {
      if (vector->_buffer[i] != 0) {
        TRI_Free(vector->_memoryZone, vector->_buffer[i]);
      }
    }

    TRI_Free(vector->_memoryZone, vector->_buffer);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector and frees the string
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVectorString (TRI_memory_zone_t* zone, TRI_vector_string_t* vector) {
  TRI_DestroyVectorString(vector);
  TRI_Free(zone, vector);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a vector and all its strings
////////////////////////////////////////////////////////////////////////////////

TRI_vector_string_t* TRI_CopyVectorString (TRI_memory_zone_t* zone,
                                           TRI_vector_string_t* vector) {
  TRI_vector_string_t* copy;

  copy = TRI_Allocate(zone, sizeof(TRI_vector_t), false);

  if (copy == NULL) {
    return NULL;
  }

  copy->_memoryZone = zone;

  if (vector->_capacity == 0) {
    copy->_buffer = NULL;
    copy->_length = 0;
    copy->_capacity = 0;
  }
  else {
    char** ptr;
    char** end;
    char** qtr;

    copy->_buffer = TRI_Allocate(zone, vector->_length * sizeof(char*), false);

    if (copy->_buffer == NULL) {
      TRI_Free(zone, copy);
      return NULL;
    }

    copy->_capacity = vector->_length;
    copy->_length = vector->_length;

    ptr = vector->_buffer;
    end = vector->_buffer + vector->_length;
    qtr = copy->_buffer;

    for (;  ptr < end;  ++ptr, ++qtr) {
      *qtr = TRI_DuplicateStringZ(zone, *ptr);

      if (*qtr == NULL) {
        char** xtr = copy->_buffer;

        for (;  xtr < qtr;  ++xtr) {
          TRI_Free(zone, *xtr);
        }

        TRI_Free(zone, copy->_buffer);
        TRI_Free(zone, copy);
        return NULL;
      }
    }
  }

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies all pointers from a vector
////////////////////////////////////////////////////////////////////////////////

int TRI_CopyDataVectorString (TRI_memory_zone_t* zone,
                              TRI_vector_string_t* dst,
                              TRI_vector_string_t* src) {
  TRI_ClearVectorString(dst);

  if (0 < src->_length) {
    char** ptr;
    char** end;
    char** qtr;
    int res;

    res = TRI_ResizeVectorString (dst, src->_length);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    ptr = src->_buffer;
    end = src->_buffer + src->_length;
    qtr = dst->_buffer;

    for (;  ptr < end;  ++ptr, ++qtr) {
      *qtr = TRI_DuplicateStringZ(zone, *ptr);

      if (*qtr == NULL) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies all pointers from a vector
////////////////////////////////////////////////////////////////////////////////

int TRI_CopyDataVectorStringFromVectorPointer (TRI_memory_zone_t* zone,
                                               TRI_vector_string_t* dst,
                                               TRI_vector_pointer_t* src) {
  TRI_ClearVectorString(dst);

  if (0 < src->_length) {
    void** ptr;
    void** end;
    char** qtr;
    int res;

    res = TRI_ResizeVectorString (dst, src->_length);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    ptr = src->_buffer;
    end = src->_buffer + src->_length;
    qtr = dst->_buffer;

    for (;  ptr < end;  ++ptr, ++qtr) {
      *qtr = TRI_DuplicateStringZ(zone, *ptr);

      if (*qtr == NULL) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if the vector is empty
////////////////////////////////////////////////////////////////////////////////

bool TRI_EmptyVectorString (TRI_vector_string_t* vector) {
  return vector->_length == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the vector
////////////////////////////////////////////////////////////////////////////////

void TRI_ClearVectorString (TRI_vector_string_t* vector) {
  if (vector->_buffer != NULL) {
    size_t i;

    for (i = 0;  i < vector->_length;  ++i) {
      if (vector->_buffer[i] != 0) {
        TRI_Free(vector->_memoryZone, vector->_buffer[i]);
      }
    }
  }

  vector->_length = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the vector
////////////////////////////////////////////////////////////////////////////////

int TRI_ResizeVectorString (TRI_vector_string_t* vector, size_t n) {
  if (vector->_length == n) {
    return TRI_ERROR_NO_ERROR;
  }

  if (vector->_capacity < n) {
    void* newBuffer;
    size_t newSize = n;

    newBuffer = TRI_Reallocate(vector->_memoryZone, vector->_buffer, newSize * sizeof(char*));

    if (newBuffer == NULL) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    vector->_capacity = n;
    vector->_buffer = newBuffer;
  }

  vector->_length = n;
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element at the end
////////////////////////////////////////////////////////////////////////////////

int TRI_PushBackVectorString (TRI_vector_string_t* vector, char* element) {
  if (vector->_length == vector->_capacity) {
    char** newBuffer;
    size_t newSize = (size_t) (1 + GROW_FACTOR * vector->_capacity);

    newBuffer = (char**) TRI_Reallocate(vector->_memoryZone, vector->_buffer, newSize * sizeof(char*));

    if (newBuffer == NULL) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    vector->_capacity = newSize;
    vector->_buffer = newBuffer;
  }

  vector->_buffer[vector->_length++] = element;
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts an element at position n
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertVectorString (TRI_vector_string_t* vector, char* element, size_t n) {

  // ...........................................................................
  // Check and see if we need to extend the vector
  // ...........................................................................

  if (n >= vector->_capacity || n >= vector->_length) {
    char** newBuffer;
    size_t newSize = (size_t) (1 + GROW_FACTOR * vector->_capacity);

    if (n >= newSize) {
      newSize = n + 1;
    }

    newBuffer = (char**) TRI_Reallocate(vector->_memoryZone, vector->_buffer, newSize * sizeof(char*));

    if (newBuffer == NULL) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    vector->_capacity = newSize;
    vector->_buffer = newBuffer;
  }

  if (n != vector->_length) {
    memmove(vector->_buffer + n + 1,
            vector->_buffer + n,
            sizeof(char**) * (vector->_length - n));
  }

  vector->_length++;
  vector->_buffer[n] = element;
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element, returns this element
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveVectorString (TRI_vector_string_t* vector, size_t n) {
  if (n < vector->_length) {
    if (vector->_buffer[n] != NULL) {
      TRI_Free(vector->_memoryZone, vector->_buffer[n]);
    }

    if (n + 1 < vector->_length) {
      memmove(vector->_buffer + n,
              vector->_buffer + (n + 1),
              (vector->_length - n - 1) * sizeof(void*));
    }

    --vector->_length;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the element at a given position
////////////////////////////////////////////////////////////////////////////////

char* TRI_AtVectorString (TRI_vector_string_t const* vector, size_t pos) {
  if (vector->_buffer == NULL || pos >= vector->_length) {
    return NULL;
  }

  return (char*) vector->_buffer[pos];
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
