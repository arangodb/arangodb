////////////////////////////////////////////////////////////////////////////////
/// @brief shape accessor
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "shape-accessor.h"

#include <BasicsC/logging.h>
#include <BasicsC/vector.h>

#include "ShapedJson/shaped-json.h"

// #define DEBUG_SHAPE_ACCESSOR 1

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief computes a byte-code sequence
////////////////////////////////////////////////////////////////////////////////

static bool BytecodeShapeAccessor (TRI_shaper_t* shaper, TRI_shape_access_t* accessor) {
  union { void const* c;  void* v; } cv;
  TRI_shape_aid_t const* paids;
  TRI_shape_path_t const* path;
  TRI_shape_t const* shape;
  TRI_vector_pointer_t ops;
  size_t i;
  size_t j;

  // find the shape
  shape = shaper->lookupShapeId(shaper, accessor->_sid);

  if (shape == NULL) {
    LOG_ERROR("unknown shape id '%ld'", (unsigned long) accessor->_sid);
    return false;
  }

  // find the attribute path
  path = shaper->lookupAttributePathByPid(shaper, accessor->_pid);

  if (path == NULL) {
    LOG_ERROR("unknown attribute path '%ld'", (unsigned long) accessor->_pid);
    return false;
  }

  paids = (TRI_shape_aid_t*) (((char const*) path) + sizeof(TRI_shape_path_t));

  // collect the bytecode
  TRI_InitVectorPointer(&ops);

  // start with the shape
  TRI_PushBackVectorPointer(&ops, (void*) TRI_SHAPE_AC_SHAPE_PTR);
  cv.c = shape;
  TRI_PushBackVectorPointer(&ops, cv.v);

  // and follow it
  for (i = 0;  i < path->_aidLength;  ++i, ++paids) {
#ifdef DEBUG_SHAPE_ACCESSOR
    printf("%lu: aid: %lu, sid: %lu, type %lu\n",
           (unsigned long) i,
           (unsigned long) *paids,
           (unsigned long) shape->_sid,
           (unsigned long) shape->_type);
#endif

    if (shape->_type == TRI_SHAPE_ARRAY) {
      TRI_array_shape_t* s;
      TRI_shape_aid_t const* aids;
      TRI_shape_sid_t const* sids;
      TRI_shape_sid_t sid;
      TRI_shape_size_t const* offsetsF;
      TRI_shape_size_t f;
      TRI_shape_size_t n;
      TRI_shape_size_t v;
      char const* qtr;

      s = (TRI_array_shape_t*) shape;
      f = s->_fixedEntries;
      v = s->_variableEntries;
      n = f + v;

      // find the aid within the shape
      qtr = (char const*) shape;
      qtr += sizeof(TRI_array_shape_t);

      sids = (TRI_shape_sid_t const*) qtr;
      qtr += n * sizeof(TRI_shape_sid_t);

      aids = (TRI_shape_aid_t const*) qtr;
      qtr += n * sizeof(TRI_shape_aid_t);

      offsetsF = (TRI_shape_size_t const*) qtr;

      // check for fixed size aid
      for (j = 0;  j < f;  ++j, ++sids, ++aids, ++offsetsF) {
        if (*paids == *aids) {
          sid = *sids;

          LOG_TRACE("found aid '%ld' as fixed entry with sid '%ld' and offset '%ld' - '%ld'",
                    (unsigned long) *paids,
                    (unsigned long) sid,
                    (unsigned long) offsetsF[0],
                    (unsigned long) offsetsF[1]);

          shape = shaper->lookupShapeId(shaper, sid);

          if (shape == NULL) {
            LOG_ERROR("unknown shape id '%ld' for attribute id '%ld'",
                      (unsigned long) accessor->_sid,
                      (unsigned long) *paids);

            TRI_DestroyVectorPointer(&ops);
            return false;
          }

          TRI_PushBackVectorPointer(&ops, (void*) TRI_SHAPE_AC_OFFSET_FIX);
          TRI_PushBackVectorPointer(&ops, (void*) (offsetsF[0]));
          TRI_PushBackVectorPointer(&ops, (void*) (offsetsF[1]));

          TRI_PushBackVectorPointer(&ops, (void*) TRI_SHAPE_AC_SHAPE_PTR);
          cv.c = shape;
          TRI_PushBackVectorPointer(&ops, cv.v);

          break;
        }
      }

      if (j < f) {
        continue;
      }

      // check for variable size aid
      for (j = 0;  j < v;  ++j, ++sids, ++aids) {
        if (*paids == *aids) {
          sid = *sids;

          LOG_TRACE("found aid '%ld' as variable entry with sid '%ld'",
                    (unsigned long) *paids,
                    (unsigned long) sid);

          shape = shaper->lookupShapeId(shaper, sid);

          if (shape == NULL) {
            LOG_ERROR("unknown shape id '%ld' for attribute id '%ld'",
                      (unsigned long) accessor->_sid,
                      (unsigned long) *paids);

            TRI_DestroyVectorPointer(&ops);
            return false;
          }

          TRI_PushBackVectorPointer(&ops, (void*) TRI_SHAPE_AC_OFFSET_VAR);
          TRI_PushBackVectorPointer(&ops, (void*) j);

          TRI_PushBackVectorPointer(&ops, (void*) TRI_SHAPE_AC_SHAPE_PTR);
          cv.c = shape;
          TRI_PushBackVectorPointer(&ops, cv.v);

          break;
        }
      }

      if (j < v) {
        continue;
      }

      LOG_TRACE("unknown attribute id '%ld'", (unsigned long) *paids);

      TRI_DestroyVectorPointer(&ops);

      accessor->_shape = NULL;
      accessor->_code = NULL;

      return true;
    }
    else {
      TRI_DestroyVectorPointer(&ops);

      accessor->_shape = NULL;
      accessor->_code = NULL;

      return true;
    }
  }

  // travel attribute path to the end
  TRI_PushBackVectorPointer(&ops, (void*) TRI_SHAPE_AC_DONE);

  accessor->_shape = shape;
  cv.c = accessor->_code = TRI_Allocate(ops._length * sizeof(void*));

  memcpy(cv.v, ops._buffer, ops._length * sizeof(void*));

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a byte-code sequence
////////////////////////////////////////////////////////////////////////////////

static bool ExecuteBytecodeShapeAccessor (TRI_shape_access_t* accessor,
                                          void** begin,
                                          void** end) {
  TRI_shape_size_t b;
  TRI_shape_size_t e;
  TRI_shape_size_t pos;
  TRI_shape_size_t* offsetsV;
  TRI_shape_t const* shape;
  void* const* ops;

  if (accessor->_shape == NULL) {
    return false;
  }

  ops = accessor->_code;
  shape = NULL;

  while (true) {
    TRI_shape_ac_bc_e op;

    op = * (TRI_shape_ac_bc_e*) ops;
    ops++;

    switch (op) {
      case TRI_SHAPE_AC_DONE:
        return true;

      case TRI_SHAPE_AC_SHAPE_PTR:
        shape = *ops++;
        break;

      case TRI_SHAPE_AC_OFFSET_FIX:
        b = (TRI_shape_size_t) *ops++;
        e = (TRI_shape_size_t) *ops++;

        *end = ((char*) *begin) + e;
        *begin = ((char*) *begin) + b;

        break;

      case TRI_SHAPE_AC_OFFSET_VAR:
        pos = (TRI_shape_size_t) *ops++;

        offsetsV = (TRI_shape_size_t*) *begin;

        *end = ((char*) *begin) + offsetsV[pos + 1];
        *begin = ((char*) *begin) + offsetsV[pos];

        break;

      default:
        return false;
    }
  }

  return false;
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
/// @brief creates a shape accessor
////////////////////////////////////////////////////////////////////////////////

TRI_shape_access_t* TRI_ShapeAccessor (TRI_shaper_t* shaper,
                                       TRI_shape_sid_t sid,
                                       TRI_shape_pid_t pid) {
  TRI_shape_access_t* accessor;
  bool ok;

  accessor = TRI_Allocate(sizeof(TRI_shape_access_t));
  accessor->_sid = sid;
  accessor->_pid = pid;
  ok = BytecodeShapeAccessor(shaper, accessor);

  if (ok) {
    return accessor;
  }

  TRI_Free(accessor);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a shape accessor
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteShapeAccessor (TRI_shape_access_t* accessor,
                               TRI_shaped_json_t const* shaped,
                               TRI_shaped_json_t* result) {
  void* begin;
  void* end;
  bool ok;

  begin = shaped->_data.data;
  end = ((char*) begin) + shaped->_data.length;

  ok = ExecuteBytecodeShapeAccessor(accessor, &begin, &end);

  if (! ok) {
    return false;
  }

  result->_sid = accessor->_shape->_sid;
  result->_data.data = begin;
  result->_data.length = ((char const*) end) - ((char const*) begin);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a TRI_shape_t for debugging
////////////////////////////////////////////////////////////////////////////////

void TRI_PrintShapeAccessor (TRI_shape_access_t* accessor) {
  TRI_shape_size_t b;
  TRI_shape_size_t e;
  TRI_shape_size_t pos;
  TRI_shape_t const* shape;
  void* const* ops;

  printf("shape accessor for sid: %lu, pid: %lu\n",
         (unsigned long) accessor->_sid,
         (unsigned long) accessor->_pid);

  if (accessor->_shape == NULL) {
    printf("  result shape: -\n");
    return;
  }

  printf("  result shape: %lu\n", (unsigned long) accessor->_shape->_sid);

  ops = accessor->_code;
  shape = accessor->_shape;

  while (true) {
    TRI_shape_ac_bc_e op;

    op = * (TRI_shape_ac_bc_e*) ops;
    ops++;

    switch (op) {
      case TRI_SHAPE_AC_DONE:
        return;

      case TRI_SHAPE_AC_SHAPE_PTR:
        shape = *ops++;

        printf("  OP: shape %lu\n",
               (unsigned long) shape->_sid);
        break;

      case TRI_SHAPE_AC_OFFSET_FIX:
        b = (TRI_shape_size_t) *ops++;
        e = (TRI_shape_size_t) *ops++;

        printf("  OP: fixed offset %lu - %lu\n",
               (unsigned long) b,
               (unsigned long) e);
        break;

      case TRI_SHAPE_AC_OFFSET_VAR:
        pos = (TRI_shape_size_t) *ops++;

        printf("  OP: variable offset at position %lu\n",
               (unsigned long) pos);
        break;

      default:
        printf("  OP: unknown op code\n");
        return;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
