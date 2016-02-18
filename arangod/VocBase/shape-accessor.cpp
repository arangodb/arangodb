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

#include "shape-accessor.h"
#include "Basics/Logger.h"
#include "Basics/vector.h"
#include "VocBase/shaped-json.h"
#include "VocBase/VocShaper.h"

// #define DEBUG_SHAPE_ACCESSOR 1

////////////////////////////////////////////////////////////////////////////////
/// @brief shape accessor bytecode operations
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_SHAPE_AC_DONE = 1,
  TRI_SHAPE_AC_OFFSET_FIX = 2,
  TRI_SHAPE_AC_OFFSET_VAR = 3
} TRI_shape_ac_bc_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief computes a byte-code sequence
////////////////////////////////////////////////////////////////////////////////

static bool BytecodeShapeAccessor(VocShaper* shaper,
                                  TRI_shape_access_t* accessor) {
  // find the shape
  TRI_shape_t const* shape = shaper->lookupShapeId(accessor->_sid);

  if (shape == nullptr) {
    LOG(ERR) << "unknown shape id " << accessor->_sid;
#ifdef TRI_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(false);
#endif
    return false;
  }

  // we need at least 3 or 4 entries in the vector to store an accessor
  TRI_vector_pointer_t ops;

  if (TRI_InitVectorPointer(&ops, TRI_UNKNOWN_MEM_ZONE, 4) !=
      TRI_ERROR_NO_ERROR) {
    return false;
  }

  // find the attribute path
  TRI_shape_path_t const* path =
      shaper->lookupAttributePathByPid(accessor->_pid);

  if (path == nullptr) {
    LOG(ERR) << "unknown attribute path " << accessor->_pid;
#ifdef TRI_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(false);
#endif
    return false;
  }

  TRI_shape_aid_t const* paids =
      (TRI_shape_aid_t*)(((char const*)path) + sizeof(TRI_shape_path_t));

  // and follow it
  for (size_t i = 0; i < path->_aidLength; ++i, ++paids) {
#ifdef DEBUG_SHAPE_ACCESSOR
    printf("%lu: aid: %lu, sid: %lu, type %lu\n", (unsigned long)i,
           (unsigned long)*paids, (unsigned long)shape->_sid,
           (unsigned long)shape->_type);
#endif

    if (shape->_type == TRI_SHAPE_ARRAY) {
      TRI_array_shape_t* s = (TRI_array_shape_t*)shape;
      TRI_shape_size_t const f = s->_fixedEntries;
      TRI_shape_size_t const v = s->_variableEntries;
      TRI_shape_size_t n = f + v;

      // find the aid within the shape
      char const* qtr = (char const*)shape;
      qtr += sizeof(TRI_array_shape_t);

      TRI_shape_sid_t const* sids = (TRI_shape_sid_t const*)qtr;
      qtr += n * sizeof(TRI_shape_sid_t);

      TRI_shape_aid_t const* aids = (TRI_shape_aid_t const*)qtr;
      qtr += n * sizeof(TRI_shape_aid_t);

      TRI_shape_size_t const* offsetsF = (TRI_shape_size_t const*)qtr;

      size_t j;

      // check for fixed size aid
      for (j = 0; j < f; ++j, ++sids, ++aids, ++offsetsF) {
        if (*paids == *aids) {
          TRI_shape_sid_t const sid = *sids;

          LOG(TRACE) << "found aid '" << *paids << "' as fixed entry with sid '" << sid << "' and offset '" << offsetsF[0] << "' - '" << offsetsF[1] << "'";

          shape = shaper->lookupShapeId(sid);

          if (shape == nullptr) {
            LOG(ERR) << "unknown shape id '" << accessor->_sid << "' for attribute id '" << *paids << "'";

            TRI_DestroyVectorPointer(&ops);
            return false;
          }

          // reserve a block big enough to hold the following 3 entries plus the
          // final AC_DONE entry
          int res = TRI_ReserveVectorPointer(&ops, 4);

          if (res != TRI_ERROR_NO_ERROR) {
            LOG(ERR) << "out of memory";
            TRI_DestroyVectorPointer(&ops);
            return false;
          }

          // this will always succeed as we reserve enough memory before
          TRI_PushBackVectorPointer(&ops, (void*)TRI_SHAPE_AC_OFFSET_FIX);
          TRI_PushBackVectorPointer(
              &ops, (void*)(uintptr_t)(
                        offsetsF[0]));  // offset is always smaller than 4 GByte
          TRI_PushBackVectorPointer(
              &ops, (void*)(uintptr_t)(
                        offsetsF[1]));  // offset is always smaller than 4 GByte
          break;
        }
      }

      if (j < f) {
        continue;
      }

      // check for variable size aid
      for (j = 0; j < v; ++j, ++sids, ++aids) {
        if (*paids == *aids) {
          TRI_shape_sid_t const sid = *sids;

          LOG(TRACE) << "found aid '" << *paids << "' as variable entry with sid '" << sid << "'";

          shape = shaper->lookupShapeId(sid);

          if (shape == nullptr) {
            LOG(ERR) << "unknown shape id '" << accessor->_sid << "' for attribute id '" << *paids << "'";

            TRI_DestroyVectorPointer(&ops);
            return false;
          }

          // reserve a block big enough to hold the following 2 entries plus the
          // final AC_DONE entry
          int res = TRI_ReserveVectorPointer(&ops, 3);

          if (res != TRI_ERROR_NO_ERROR) {
            LOG(ERR) << "out of memory";
            TRI_DestroyVectorPointer(&ops);
            return false;
          }

          // this will always succeed as we reserved enough memory in the vector
          // before
          TRI_PushBackVectorPointer(&ops, (void*)TRI_SHAPE_AC_OFFSET_VAR);
          TRI_PushBackVectorPointer(&ops, (void*)j);
          break;
        }
      }

      if (j < v) {
        continue;
      }

      LOG(TRACE) << "unknown attribute id '" << *paids << "'";
    }

    TRI_DestroyVectorPointer(&ops);

    accessor->_resultSid = TRI_SHAPE_ILLEGAL;
    accessor->_code = nullptr;

    return true;
  }

  // insert final "done" entry
  // note that this must always succeed as we reserved enough space before
  TRI_PushBackVectorPointer(&ops, (void*)TRI_SHAPE_AC_DONE);

  // remember resulting sid
  accessor->_resultSid = shape->_sid;

  // steal buffer from ops vector so we don't need to copy it
  accessor->_code = const_cast<void const**>(ops._buffer);

  // inform the vector that we took over ownership
  ops._buffer = nullptr;

  TRI_DestroyVectorPointer(&ops);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a byte-code sequence
////////////////////////////////////////////////////////////////////////////////

static bool ExecuteBytecodeShapeAccessor(TRI_shape_access_t const* accessor,
                                         void** begin, void** end) {
  if (accessor->_resultSid == TRI_SHAPE_ILLEGAL) {
    return false;
  }

  void const** ops = static_cast<void const**>(accessor->_code);

  while (true) {
    TRI_shape_ac_bc_e op = *(TRI_shape_ac_bc_e*)ops;
    ++ops;

    switch (op) {
      case TRI_SHAPE_AC_DONE:
        return true;

      case TRI_SHAPE_AC_OFFSET_FIX: {
        TRI_shape_size_t b = (TRI_shape_size_t)(
            uintptr_t)*ops++;  // offset is always smaller than 4 GByte
        TRI_shape_size_t e = (TRI_shape_size_t)(
            uintptr_t)*ops++;  // offset is always smaller than 4 GByte

        *end = ((char*)*begin) + e;
        *begin = ((char*)*begin) + b;
        break;
      }

      case TRI_SHAPE_AC_OFFSET_VAR: {
        TRI_shape_size_t pos = (TRI_shape_size_t)(
            uintptr_t)*ops++;  // offset is always smaller than 4 GByte
        TRI_shape_size_t* offsetsV = (TRI_shape_size_t*)*begin;

        *end = ((char*)*begin) + offsetsV[pos + 1];
        *begin = ((char*)*begin) + offsetsV[pos];
        break;
      }

      default:
        break;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a shape accessor
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShapeAccessor(TRI_shape_access_t* accessor) {
  TRI_ASSERT(accessor != nullptr);

  if (accessor->_code != nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, (void*)accessor->_code);
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, accessor);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a shape accessor
////////////////////////////////////////////////////////////////////////////////

TRI_shape_access_t* TRI_ShapeAccessor(VocShaper* shaper, TRI_shape_sid_t sid,
                                      TRI_shape_pid_t pid) {
  TRI_shape_access_t* accessor = static_cast<TRI_shape_access_t*>(
      TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shape_access_t), false));

  if (accessor == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  accessor->_sid = sid;
  accessor->_pid = pid;
  accessor->_code = nullptr;

  bool ok = BytecodeShapeAccessor(shaper, accessor);

  if (ok) {
    return accessor;
  }

  TRI_FreeShapeAccessor(accessor);
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a shape accessor
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteShapeAccessor(TRI_shape_access_t const* accessor,
                              TRI_shaped_json_t const* shaped,
                              TRI_shaped_json_t* result) {
  void* begin = shaped->_data.data;
  void* end = ((char*)begin) + shaped->_data.length;

  if (!ExecuteBytecodeShapeAccessor(accessor, &begin, &end)) {
    return false;
  }

  result->_sid = accessor->_resultSid;
  result->_data.data = (char*)begin;
  result->_data.length = (uint32_t)(((char const*)end) - ((char const*)begin));

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a TRI_shape_t for debugging
////////////////////////////////////////////////////////////////////////////////

void TRI_PrintShapeAccessor(TRI_shape_access_t* accessor) {
  TRI_shape_size_t b;
  TRI_shape_size_t e;
  TRI_shape_size_t pos;

  printf("shape accessor for sid: %lu, pid: %lu\n",
         (unsigned long)accessor->_sid, (unsigned long)accessor->_pid);

  if (accessor->_resultSid == TRI_SHAPE_ILLEGAL) {
    printf("  result shape: -\n");
    return;
  }

  printf("  result shape: %lu\n", (unsigned long)accessor->_resultSid);

  void const** ops = static_cast<void const**>(accessor->_code);

  while (true) {
    TRI_shape_ac_bc_e op =
        static_cast<TRI_shape_ac_bc_e>(*(TRI_shape_ac_bc_e*)ops);
    ops++;

    switch (op) {
      case TRI_SHAPE_AC_DONE:
        return;

      case TRI_SHAPE_AC_OFFSET_FIX:
        b = (TRI_shape_size_t)(
            uintptr_t)*ops++;  // offset is always smaller than 4 GByte
        e = (TRI_shape_size_t)(
            uintptr_t)*ops++;  // offset is always smaller than 4 GByte

        printf("  OP: fixed offset %lu - %lu\n", (unsigned long)b,
               (unsigned long)e);
        break;

      case TRI_SHAPE_AC_OFFSET_VAR:
        pos = (TRI_shape_size_t)(
            uintptr_t)*ops++;  // offset is always smaller than 4 GByte

        printf("  OP: variable offset at position %lu\n", (unsigned long)pos);
        break;

      default:
        printf("  OP: unknown op code\n");
        return;
    }
  }
}
