////////////////////////////////////////////////////////////////////////////////
/// @brief vector implementation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "vector.h"

#include "BasicsC/strings.h"

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

void TRI_InitVector (TRI_vector_t* vector, size_t elementSize) {
  vector->_elementSize = elementSize;
  vector->_buffer = NULL;
  vector->_length = 0;
  vector->_capacity = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVector (TRI_vector_t* vector) {
  if (vector->_buffer != NULL) {
    TRI_Free(vector->_buffer);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVector (TRI_vector_t* vector) {
  TRI_DestroyVector(vector);
  TRI_Free(vector);
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

TRI_vector_t* TRI_CopyVector (TRI_vector_t* vector) {
  TRI_vector_t* copy;

  copy = TRI_Allocate(sizeof(TRI_vector_t));
  copy->_elementSize = vector->_elementSize;

  if (vector->_capacity == 0) {
    copy->_buffer = NULL;
    copy->_length = 0;
    copy->_capacity = 0;
  }
  else {
    copy->_buffer = TRI_Allocate(vector->_length * vector->_elementSize);
    copy->_capacity = vector->_length;
    copy->_length = vector->_length;

    memcpy(copy->_buffer, vector->_buffer, vector->_length * vector->_elementSize);
  }

  return copy;
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

void TRI_ResizeVector (TRI_vector_t* vector, size_t n) {
  if (vector->_length == n) {
    return;
  }

  if (vector->_capacity < n) {
    char * newBuffer;

    vector->_capacity = n;
    newBuffer = (char*) TRI_Allocate(vector->_capacity * vector->_elementSize);

    if (vector->_buffer != NULL) {
      memcpy(newBuffer, vector->_buffer, vector->_length * vector->_elementSize);
      TRI_Free(vector->_buffer);
    }

    vector->_buffer = newBuffer;
  }

  vector->_length = n;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds and element at the end
////////////////////////////////////////////////////////////////////////////////

void TRI_PushBackVector (TRI_vector_t* vector, void const* element) {
  if (vector->_length == vector->_capacity) {
    char * newBuffer;

    vector->_capacity = (size_t)(1 + GROW_FACTOR * vector->_capacity);
    newBuffer = (char*) TRI_Allocate(vector->_capacity * vector->_elementSize);

    if (vector->_buffer != NULL) {
      memcpy(newBuffer, vector->_buffer, vector->_length * vector->_elementSize);
      TRI_Free(vector->_buffer);
    }

    vector->_buffer = newBuffer;
  }

  memcpy(vector->_buffer + vector->_length * vector->_elementSize, element, vector->_elementSize);
  vector->_length++;
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
  return (void*)(vector->_buffer + pos * vector->_elementSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets an element at a given position
////////////////////////////////////////////////////////////////////////////////

void TRI_SetVector (TRI_vector_t* vector, size_t pos, void const* element) {
  memcpy((void*)(vector->_buffer + pos * vector->_elementSize), element, vector->_elementSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the begining
////////////////////////////////////////////////////////////////////////////////

void* TRI_BeginVector (TRI_vector_t* vector) {
  return vector->_buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the end
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

void TRI_InitVectorPointer (TRI_vector_pointer_t* vector) {
  vector->_buffer = NULL;
  vector->_length = 0;
  vector->_capacity = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVectorPointer (TRI_vector_pointer_t* vector) {
  if (vector->_buffer != NULL) {
    TRI_Free(vector->_buffer);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVectorPointer (TRI_vector_pointer_t* vector) {
  TRI_DestroyVectorPointer(vector);
  TRI_Free(vector);
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

TRI_vector_pointer_t* TRI_CopyVectorPointer (TRI_vector_pointer_t* vector) {
  TRI_vector_pointer_t* copy = TRI_Allocate(sizeof(TRI_vector_pointer_t));

  if (!copy) {
    return NULL;
  }

  if (vector->_capacity == 0) {
    copy->_buffer = NULL;
    copy->_length = 0;
    copy->_capacity = 0;
  }
  else {
    copy->_buffer = TRI_Allocate(vector->_length * sizeof(void*));
    if (!copy->_buffer) {
      TRI_Free(copy);
      return NULL;
    }
    copy->_capacity = vector->_length;
    copy->_length = vector->_length;

    memcpy(copy->_buffer, vector->_buffer, vector->_length * sizeof(void*));
  }

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies all pointers from a vector
////////////////////////////////////////////////////////////////////////////////

void TRI_CopyDataVectorPointer (TRI_vector_pointer_t* dst, TRI_vector_pointer_t* src) {
  if (src->_length == 0) {
    dst->_length = 0;
  }
  else {
    TRI_ResizeVectorPointer(dst, src->_length);

    memcpy(dst->_buffer, src->_buffer, src->_length * sizeof(void*));
  }
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

void TRI_ResizeVectorPointer (TRI_vector_pointer_t* vector, size_t n) {
  assert(n >= 0);

  if (vector->_length == n) {
    return;
  }

  if (vector->_capacity < n) {
    void* newBuffer;

    vector->_capacity = n;
    newBuffer = TRI_Allocate(vector->_capacity * sizeof(void*));

    if (vector->_buffer != NULL) {
      memcpy(newBuffer, vector->_buffer, vector->_length * sizeof(void*));
      TRI_Free(vector->_buffer);
    }

    vector->_buffer = newBuffer;
  }

  vector->_length = n;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element at the end
////////////////////////////////////////////////////////////////////////////////

void TRI_PushBackVectorPointer (TRI_vector_pointer_t* vector, void* element) {
  if (vector->_length == vector->_capacity) {
    void* newBuffer;

    vector->_capacity = (size_t)(1 + GROW_FACTOR * vector->_capacity);
    newBuffer = (char*) TRI_Allocate(vector->_capacity * sizeof(void*));

    if (vector->_buffer != NULL) {
      memcpy(newBuffer, vector->_buffer, vector->_length * sizeof(void*));
      TRI_Free(vector->_buffer);
    }

    vector->_buffer = newBuffer;
  }

  vector->_buffer[vector->_length++] = element;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts an element at position n, shifting the following elements
////////////////////////////////////////////////////////////////////////////////

void TRI_InsertVectorPointer (TRI_vector_pointer_t* vector, void* element, size_t n) {
  assert(n >= 0);

  if (vector->_length >= vector->_capacity) {
    void* newBuffer;

    vector->_capacity = (size_t)(1 + GROW_FACTOR * vector->_capacity);
    if (n >= vector->_capacity) {
      vector->_capacity = n + 1;
    }
    newBuffer = (char**) TRI_Allocate(vector->_capacity * sizeof(void*));

    if (vector->_buffer != NULL) {
      memcpy(newBuffer, vector->_buffer, vector->_length * sizeof(void*));
      TRI_Free(vector->_buffer);
    }

    vector->_buffer = newBuffer;
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
  if (pos >= vector->_length) {
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
/// @brief initialises a vector
////////////////////////////////////////////////////////////////////////////////

void TRI_InitVectorString (TRI_vector_string_t* vector) {
  vector->_buffer = NULL;
  vector->_length = 0;
  vector->_capacity = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector and all strings, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVectorString (TRI_vector_string_t* vector) {
  size_t i;

  if (vector->_buffer != NULL) {
    for (i = 0;  i < vector->_capacity;  ++i) {
      if (vector->_buffer[i] != 0) {
        TRI_Free(vector->_buffer[i]);
      }
    }

    TRI_Free(vector->_buffer);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector and frees the string
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVectorString (TRI_vector_string_t* vector) {
  TRI_DestroyVectorString(vector);
  TRI_Free(vector);
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

TRI_vector_string_t* TRI_CopyVectorString (TRI_vector_string_t* vector) {
  TRI_vector_string_t* copy;
  char** ptr;
  char** end;
  char** qtr;

  copy = TRI_Allocate(sizeof(TRI_vector_t));

  if (vector->_capacity == 0) {
    copy->_buffer = NULL;
    copy->_length = 0;
    copy->_capacity = 0;
  }
  else {
    copy->_buffer = TRI_Allocate(vector->_length * sizeof(char*));
    copy->_capacity = vector->_length;
    copy->_length = vector->_length;

    ptr = vector->_buffer;
    end = vector->_buffer + vector->_length;
    qtr = copy->_buffer;

    for (;  ptr < end;  ++ptr, ++qtr) {
      *qtr = TRI_DuplicateString(*ptr);
    }
  }

  return copy;
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
  vector->_length = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the vector
////////////////////////////////////////////////////////////////////////////////

void TRI_ResizeVectorString (TRI_vector_string_t* vector, size_t n) {
  if (vector->_length == n) {
    return;
  }

  if (vector->_capacity < n) {
    void* newBuffer;

    vector->_capacity = n;
    newBuffer = TRI_Allocate(vector->_capacity * sizeof(char*));

    if (vector->_buffer != NULL) {
      memcpy(newBuffer, vector->_buffer, vector->_length * sizeof(char*));
      TRI_Free(vector->_buffer);
    }

    vector->_buffer = newBuffer;
  }

  vector->_length = n;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element at the end
////////////////////////////////////////////////////////////////////////////////

void TRI_PushBackVectorString (TRI_vector_string_t* vector, char* element) {
  if (vector->_length == vector->_capacity) {
    char** newBuffer;

    vector->_capacity = (size_t)(1 + GROW_FACTOR * vector->_capacity);
    newBuffer = (char**) TRI_Allocate(vector->_capacity * sizeof(char*));

    if (vector->_buffer != NULL) {
      memcpy(newBuffer, vector->_buffer, vector->_length * sizeof(char*));
      TRI_Free(vector->_buffer);
    }

    vector->_buffer = newBuffer;
  }

  vector->_buffer[vector->_length++] = element;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts an element at position n
////////////////////////////////////////////////////////////////////////////////

void TRI_InsertVectorString (TRI_vector_string_t* vector, char* element, size_t n) {
  if (n >= vector->_capacity) {
    char** newBuffer;

    vector->_capacity = (size_t)(1 + GROW_FACTOR * vector->_capacity);
    if (n >= vector->_capacity) {
      vector->_capacity = n + 1;
    }
    newBuffer = (char**) TRI_Allocate(vector->_capacity * sizeof(char*));

    if (vector->_buffer != NULL) {
      memcpy(newBuffer, vector->_buffer, vector->_length * sizeof(char*));
      TRI_Free(vector->_buffer);
    }

    vector->_buffer = newBuffer;
  }

  if (n != vector->_length) {
    memmove(vector->_buffer + n + 1, 
            vector->_buffer + n, 
            sizeof(char**) * (vector->_length - n));
  }
  vector->_length++;
  vector->_buffer[n] = element;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element, returns this element
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveVectorString (TRI_vector_string_t* vector, size_t n) {
  if (n < vector->_length) {
    if (vector->_buffer[n] != NULL) {
      TRI_Free(vector->_buffer[n]);
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
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
