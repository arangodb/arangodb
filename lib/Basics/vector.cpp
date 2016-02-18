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

#include "vector.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief grow rate
////////////////////////////////////////////////////////////////////////////////

#define GROW_FACTOR (1.2)

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a vector
////////////////////////////////////////////////////////////////////////////////

void TRI_InitVector(TRI_vector_t* vector, TRI_memory_zone_t* zone,
                    size_t elementSize) {
  vector->_buffer = nullptr;
  vector->_memoryZoneX = TRI_MemoryZoneId(zone);
  vector->_lengthX = 0;
  vector->_capacityX = 0;
  vector->_elementSizeX = static_cast<uint32_t>(elementSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a vector, with user-definable settings
////////////////////////////////////////////////////////////////////////////////

int TRI_InitVector2(TRI_vector_t* vector, TRI_memory_zone_t* zone,
                    size_t elementSize, size_t initialCapacity) {
  // init vector as usual
  TRI_InitVector(vector, zone, elementSize);

  if (initialCapacity != 0) {
    vector->_buffer = static_cast<char*>(TRI_Allocate(
        TRI_MemoryZone(vector->_memoryZoneX),
        (initialCapacity * static_cast<size_t>(vector->_elementSizeX)), false));

    if (vector->_buffer == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }
  }

  vector->_capacityX = static_cast<uint32_t>(initialCapacity);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVector(TRI_vector_t* vector) {
  if (vector->_buffer != nullptr) {
    TRI_Free(TRI_MemoryZone(vector->_memoryZoneX), vector->_buffer);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVector(TRI_memory_zone_t* zone, TRI_vector_t* vector) {
  TRI_DestroyVector(vector);
  TRI_Free(zone, vector);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures a vector has space for extraCapacity more items
////////////////////////////////////////////////////////////////////////////////

int TRI_ReserveVector(TRI_vector_t* vector, size_t extraCapacity) {
  size_t oldLength = static_cast<size_t>(vector->_lengthX);
  size_t minLength = oldLength + extraCapacity;

  if (static_cast<size_t>(vector->_capacityX) >= minLength) {
    return TRI_ERROR_NO_ERROR;
  }

  size_t newSize = static_cast<size_t>(vector->_capacityX);
  while (newSize < minLength) {
    newSize = (size_t)(1 + GROW_FACTOR * newSize);
  }

  auto newBuffer = static_cast<char*>(
      TRI_Reallocate(TRI_MemoryZone(vector->_memoryZoneX), vector->_buffer,
                     newSize * static_cast<size_t>(vector->_elementSizeX)));

  if (newBuffer == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  vector->_buffer = newBuffer;
  vector->_capacityX = static_cast<uint32_t>(newSize);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adjusts the length of the vector
////////////////////////////////////////////////////////////////////////////////

void TRI_SetLengthVector(TRI_vector_t* vector, size_t n) {
  vector->_lengthX = static_cast<uint32_t>(n);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the vector
////////////////////////////////////////////////////////////////////////////////

void TRI_ClearVector(TRI_vector_t* vector) { vector->_lengthX = 0; }

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the vector
////////////////////////////////////////////////////////////////////////////////

int TRI_ResizeVector(TRI_vector_t* vector, size_t n) {
  if (static_cast<size_t>(vector->_lengthX) == n) {
    return TRI_ERROR_NO_ERROR;
  }

  if (static_cast<size_t>(vector->_capacityX) < n) {
    size_t newSize = n;
    auto newBuffer = static_cast<char*>(
        TRI_Reallocate(TRI_MemoryZone(vector->_memoryZoneX), vector->_buffer,
                       newSize * static_cast<size_t>(vector->_elementSizeX)));

    if (newBuffer == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    vector->_capacityX = static_cast<uint32_t>(n);
    vector->_buffer = newBuffer;
  }

  vector->_lengthX = static_cast<uint32_t>(n);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element at the end
////////////////////////////////////////////////////////////////////////////////

int TRI_PushBackVector(TRI_vector_t* vector, void const* element) {
  size_t const elementSize = static_cast<size_t>(vector->_elementSizeX);

  if (vector->_lengthX == vector->_capacityX) {
    size_t newSize =
        (size_t)(1 + (GROW_FACTOR * static_cast<size_t>(vector->_capacityX)));

    auto newBuffer = static_cast<char*>(
        TRI_Reallocate(TRI_MemoryZone(vector->_memoryZoneX), vector->_buffer,
                       newSize * elementSize));

    if (newBuffer == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    vector->_capacityX = static_cast<uint32_t>(newSize);
    vector->_buffer = newBuffer;
  }

  memcpy(vector->_buffer + static_cast<size_t>(vector->_lengthX) * elementSize,
         element, elementSize);

  vector->_lengthX++;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveVector(TRI_vector_t* vector, size_t n) {
  if (n < static_cast<size_t>(vector->_lengthX)) {
    if (n + 1 < static_cast<size_t>(vector->_lengthX)) {
      size_t const elementSize = static_cast<size_t>(vector->_elementSizeX);

      memmove(vector->_buffer + n * elementSize,
              vector->_buffer + (n + 1) * elementSize,
              (static_cast<size_t>(vector->_lengthX) - n - 1) * elementSize);
    }

    --vector->_lengthX;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an element to the vector after borrowing it via
/// TRI_NextVector. This will also decrease the vector length by one.
/// The caller must ensure that the element has been fetched from the vector
/// before.
////////////////////////////////////////////////////////////////////////////////

void TRI_ReturnVector(TRI_vector_t* vector) {
  TRI_ASSERT(vector->_lengthX > 0);

  vector->_lengthX--;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increases vector length by one and returns the address of the
/// uninitialized element at the new position
////////////////////////////////////////////////////////////////////////////////

void* TRI_NextVector(TRI_vector_t* vector) {
  // ensure the vector has enough capacity for another element
  int res = TRI_ReserveVector(vector, 1);

  if (res != TRI_ERROR_NO_ERROR) {
    // out of memory
    return nullptr;
  }

  ++vector->_lengthX;
  TRI_ASSERT(vector->_lengthX <= vector->_capacityX);
  TRI_ASSERT(vector->_buffer != nullptr);

  return TRI_AtVector(vector, static_cast<size_t>(vector->_lengthX - 1));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the element at a given position
////////////////////////////////////////////////////////////////////////////////

void* TRI_AtVector(TRI_vector_t const* vector, size_t pos) {
  if (vector->_buffer == nullptr ||
      pos >= static_cast<size_t>(vector->_lengthX)) {
    return nullptr;
  }

  return static_cast<void*>(vector->_buffer +
                            pos * static_cast<size_t>(vector->_elementSizeX));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts an element at a given position
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertVector(TRI_vector_t* vector, void const* element, size_t n) {
  size_t const elementSize = static_cast<size_t>(vector->_elementSizeX);

  // ...........................................................................
  // Check and see if we need to extend the vector
  // ...........................................................................

  if (vector->_lengthX >= vector->_capacityX ||
      n >= static_cast<size_t>(vector->_capacityX)) {
    size_t newSize =
        (size_t)(1 + (GROW_FACTOR * static_cast<size_t>(vector->_capacityX)));

    if (n >= newSize) {
      newSize = n + 1;
    }

    TRI_ASSERT(newSize > n);

    auto newBuffer = static_cast<char*>(TRI_Allocate(
        TRI_MemoryZone(vector->_memoryZoneX), newSize * elementSize, false));

    if (newBuffer == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    vector->_capacityX = static_cast<uint32_t>(newSize);

    if (vector->_buffer != nullptr) {
      memcpy(newBuffer, vector->_buffer,
             static_cast<size_t>(vector->_lengthX) * elementSize);
      TRI_Free(TRI_MemoryZone(vector->_memoryZoneX), vector->_buffer);
    }

    vector->_buffer = newBuffer;
  }

  if (n < static_cast<size_t>(vector->_lengthX)) {
    memmove(vector->_buffer + (elementSize * (n + 1)),
            vector->_buffer + (elementSize * n),
            elementSize * (static_cast<size_t>(vector->_lengthX) - n));
    ++vector->_lengthX;
  } else {
    vector->_lengthX = static_cast<uint32_t>(n + 1);
  }

  memcpy(vector->_buffer + (elementSize * n), element, elementSize);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets an element at a given position
////////////////////////////////////////////////////////////////////////////////

void TRI_SetVector(TRI_vector_t* vector, size_t pos, void const* element) {
  if (pos < static_cast<size_t>(vector->_lengthX)) {
    size_t const elementSize = static_cast<size_t>(vector->_elementSizeX);
    memcpy((void*)(vector->_buffer + pos * elementSize), element, elementSize);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the beginning
////////////////////////////////////////////////////////////////////////////////

void* TRI_BeginVector(TRI_vector_t const* vector) { return vector->_buffer; }

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a vector
////////////////////////////////////////////////////////////////////////////////

void TRI_InitVectorPointer(TRI_vector_pointer_t* vector,
                           TRI_memory_zone_t* zone) {
  vector->_memoryZone = zone;
  vector->_buffer = nullptr;
  vector->_length = 0;
  vector->_capacity = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a vector, with user-definable settings
////////////////////////////////////////////////////////////////////////////////

int TRI_InitVectorPointer(TRI_vector_pointer_t* vector, TRI_memory_zone_t* zone,
                          size_t initialCapacity) {
  TRI_InitVectorPointer(vector, zone);

  if (initialCapacity != 0) {
    vector->_buffer = static_cast<void**>(TRI_Allocate(
        vector->_memoryZone, (initialCapacity * sizeof(void*)), true));

    if (vector->_buffer == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }
  }

  vector->_capacity = initialCapacity;
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVectorPointer(TRI_vector_pointer_t* vector) {
  if (vector->_buffer != nullptr) {
    TRI_Free(vector->_memoryZone, vector->_buffer);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVectorPointer(TRI_memory_zone_t* zone,
                           TRI_vector_pointer_t* vector) {
  TRI_DestroyVectorPointer(vector);
  TRI_Free(zone, vector);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures a vector has space for extraCapacity more items
////////////////////////////////////////////////////////////////////////////////

int TRI_ReserveVectorPointer(TRI_vector_pointer_t* vector,
                             size_t extraCapacity) {
  size_t oldLength = vector->_length;
  size_t minLength = oldLength + extraCapacity;
  size_t newSize;
  void* newBuffer;

  if (vector->_capacity >= minLength) {
    return TRI_ERROR_NO_ERROR;
  }

  newSize = vector->_capacity;
  while (newSize < minLength) {
    newSize = (size_t)(1 + GROW_FACTOR * newSize);
  }

  newBuffer = TRI_Reallocate(vector->_memoryZone, vector->_buffer,
                             newSize * sizeof(void*));

  if (newBuffer == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  vector->_buffer = static_cast<void**>(newBuffer);
  vector->_capacity = newSize;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a vector
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_CopyVectorPointer(
    TRI_memory_zone_t* zone, TRI_vector_pointer_t const* vector) {
  TRI_vector_pointer_t* copy = static_cast<TRI_vector_pointer_t*>(
      TRI_Allocate(zone, sizeof(TRI_vector_pointer_t), false));

  if (copy == nullptr) {
    return nullptr;
  }

  copy->_memoryZone = zone;

  if (vector->_capacity == 0) {
    copy->_buffer = nullptr;
    copy->_length = 0;
    copy->_capacity = 0;
  } else {
    copy->_buffer = static_cast<void**>(
        TRI_Allocate(zone, vector->_length * sizeof(void*), false));

    if (copy->_buffer == nullptr) {
      TRI_Free(zone, copy);

      return nullptr;
    }

    copy->_length = vector->_length;
    copy->_capacity = vector->_length;

    memcpy(copy->_buffer, vector->_buffer, vector->_length * sizeof(void*));
  }

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element at the end
////////////////////////////////////////////////////////////////////////////////

int TRI_PushBackVectorPointer(TRI_vector_pointer_t* vector, void* element) {
  if (vector->_length == vector->_capacity) {
    void* newBuffer;
    size_t newSize = (size_t)(1 + GROW_FACTOR * vector->_capacity);

    newBuffer = TRI_Reallocate(vector->_memoryZone, vector->_buffer,
                               newSize * sizeof(void*));

    if (newBuffer == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    vector->_buffer = static_cast<void**>(newBuffer);
    // prefill the new vector elements with 0's
    memset(&vector->_buffer[vector->_capacity], 0,
           (newSize - vector->_capacity) * sizeof(void*));
    vector->_capacity = newSize;
  }

  vector->_buffer[vector->_length++] = element;
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts an element at position n, shifting the following elements
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertVectorPointer(TRI_vector_pointer_t* vector, void* element,
                            size_t n) {
  // ...........................................................................
  // Check and see if we need to extend the vector
  // ...........................................................................

  if (vector->_length >= vector->_capacity || n >= vector->_capacity) {
    void* newBuffer;
    size_t newSize = (size_t)(1 + GROW_FACTOR * vector->_capacity);

    if (n >= newSize) {
      newSize = n + 1;
    }

    TRI_ASSERT(newSize > n);

    newBuffer = TRI_Reallocate(vector->_memoryZone, vector->_buffer,
                               newSize * sizeof(void*));

    if (newBuffer == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    vector->_buffer = static_cast<void**>(newBuffer);
    // prefill the new vector elements with 0's
    memset(&vector->_buffer[vector->_capacity], 0,
           (newSize - vector->_capacity) * sizeof(void*));
    vector->_capacity = newSize;
  }

  if (n < vector->_length) {
    memmove(vector->_buffer + n + 1, vector->_buffer + n,
            sizeof(void*) * (vector->_length - n));
  }

  TRI_ASSERT(vector->_capacity >= vector->_length);

  vector->_buffer[n] = element;

  if (n > vector->_length) {
    vector->_length = n + 1;
  } else {
    ++vector->_length;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element, returns this element
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveVectorPointer(TRI_vector_pointer_t* vector, size_t n) {
  void* old;

  old = 0;

  if (n < vector->_length) {
    old = vector->_buffer[n];

    if (n + 1 < vector->_length) {
      memmove(vector->_buffer + n, vector->_buffer + (n + 1),
              (vector->_length - n - 1) * sizeof(void*));
    }

    --vector->_length;
  }

  return old;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the element at a given position
////////////////////////////////////////////////////////////////////////////////

void* TRI_AtVectorPointer(TRI_vector_pointer_t const* vector, size_t pos) {
  if (vector->_buffer == nullptr || pos >= vector->_length) {
    return nullptr;
  }

  return vector->_buffer[pos];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a string vector
////////////////////////////////////////////////////////////////////////////////

void TRI_InitVectorString(TRI_vector_string_t* vector,
                          TRI_memory_zone_t* zone) {
  vector->_memoryZone = zone;
  vector->_buffer = nullptr;
  vector->_length = 0;
  vector->_capacity = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a string vector, with user-definable settings
////////////////////////////////////////////////////////////////////////////////

int TRI_InitVectorString2(TRI_vector_string_t* vector, TRI_memory_zone_t* zone,
                          size_t initialCapacity) {
  TRI_InitVectorString(vector, zone);

  if (initialCapacity != 0) {
    vector->_buffer = (char**)TRI_Allocate(
        vector->_memoryZone, initialCapacity * sizeof(char*), false);
    if (vector->_buffer == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }
  }

  vector->_capacity = initialCapacity;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector and all strings, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVectorString(TRI_vector_string_t* vector) {
  if (vector->_buffer != nullptr) {
    for (size_t i = 0; i < vector->_length; ++i) {
      if (vector->_buffer[i] != nullptr) {
        TRI_Free(vector->_memoryZone, vector->_buffer[i]);
      }
    }

    TRI_Free(vector->_memoryZone, vector->_buffer);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector and frees the string
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVectorString(TRI_memory_zone_t* zone,
                          TRI_vector_string_t* vector) {
  TRI_DestroyVectorString(vector);
  TRI_Free(zone, vector);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element at the end
////////////////////////////////////////////////////////////////////////////////

int TRI_PushBackVectorString(TRI_vector_string_t* vector, char* element) {
  if (vector->_length == vector->_capacity) {
    char** newBuffer;
    size_t newSize = (size_t)(1 + GROW_FACTOR * vector->_capacity);

    newBuffer = (char**)TRI_Reallocate(vector->_memoryZone, vector->_buffer,
                                       newSize * sizeof(char*));

    if (newBuffer == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    vector->_capacity = newSize;
    vector->_buffer = newBuffer;
  }

  vector->_buffer[vector->_length++] = element;
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element, returns this element
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveVectorString(TRI_vector_string_t* vector, size_t n) {
  if (n < vector->_length) {
    if (vector->_buffer[n] != nullptr) {
      TRI_Free(vector->_memoryZone, vector->_buffer[n]);
    }

    if (n + 1 < vector->_length) {
      memmove(vector->_buffer + n, vector->_buffer + (n + 1),
              (vector->_length - n - 1) * sizeof(void*));
    }

    --vector->_length;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the element at a given position
////////////////////////////////////////////////////////////////////////////////

char* TRI_AtVectorString(TRI_vector_string_t const* vector, size_t pos) {
  if (vector->_buffer == nullptr || pos >= vector->_length) {
    return nullptr;
  }

  return (char*)vector->_buffer[pos];
}
