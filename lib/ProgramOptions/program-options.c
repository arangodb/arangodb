////////////////////////////////////////////////////////////////////////////////
/// @brief program options
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
/// @author Esteban Lombeyda
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "program-options.h"

#include <getopt.h>
#include <regex.h>

#include "BasicsC/conversions.h"
#include "BasicsC/logging.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/tri-strings.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ProgramOptions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief description of a double
////////////////////////////////////////////////////////////////////////////////

typedef struct po_double_s {
  TRI_PO_desc_t base;

  double * _value;
} po_double_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief description of an attribute (a flag) without arguments
////////////////////////////////////////////////////////////////////////////////

typedef struct po_flag_s {
  TRI_PO_desc_t base;

  bool *_value;
} po_flag_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief description of a signed 16-bit integer
////////////////////////////////////////////////////////////////////////////////

typedef struct po_int16_s {
  TRI_PO_desc_t base;

  int16_t * _value;
} po_int16_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief description of a signed 32-bit integer
////////////////////////////////////////////////////////////////////////////////

typedef struct po_int32_s {
  TRI_PO_desc_t base;

  int32_t * _value;
} po_int32_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief description of a signed 64-bit integer
////////////////////////////////////////////////////////////////////////////////

typedef struct po_int64_s {
  TRI_PO_desc_t base;

  int64_t * _value;
} po_int64_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief description of an unsigned 16-bit integer
////////////////////////////////////////////////////////////////////////////////

typedef struct po_uint16_s {
  TRI_PO_desc_t base;

  uint16_t * _value;
} po_uint16_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief description of an unsigned 32-bit integer
////////////////////////////////////////////////////////////////////////////////

typedef struct po_uint32_s {
  TRI_PO_desc_t base;

  uint32_t * _value;
} po_uint32_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief description of an unsigned 64-bit integer
////////////////////////////////////////////////////////////////////////////////

typedef struct po_uint64_s {
  TRI_PO_desc_t base;

  uint64_t * _value;
} po_uint64_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief set of evaluation functions
////////////////////////////////////////////////////////////////////////////////

typedef struct po_visit_functions_s {
  void (*visitDoubleNode) (po_double_t *, void const * input, void * output);
  void (*visitFlagNode) (po_flag_t *, void const * input, void * output);
  void (*visitInt16Node) (po_int16_t *, void const * input, void * output);
  void (*visitInt32Node) (po_int32_t *, void const * input, void * output);
  void (*visitInt64Node) (po_int64_t *, void const * input, void * output);
  void (*visitSectionNodeBefore) (TRI_PO_section_t *, void const * input, void * output);
  void (*visitSectionNodeAfter) (TRI_PO_section_t *, void const * input, void * output);
  void (*visitStringNode) (TRI_PO_string_t *, void const * input, void * output);
  void (*visitUInt16Node) (po_uint16_t *, void const * input, void * output);
  void (*visitUInt32Node) (po_uint32_t *, void const * input, void * output);
  void (*visitUInt64Node) (po_uint64_t *, void const * input, void * output);
  void (*visitVectorStringNode) (TRI_PO_vector_string_t *, void const * input, void * output);
} po_visit_functions_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ProgramOptions
/// @{
////////////////////////////////////////////////////////////////////////////////

struct option * InitOptionStructure (struct option * option,
                                     char const * name,
                                     int hasArg,
                                     int * flag,
                                     int val) {
  option->name = name;
  option->has_arg = hasArg;
  option->flag = flag;
  option->val = 256 + val;

  return option;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a atomic option
////////////////////////////////////////////////////////////////////////////////

static void FreeOption (TRI_PO_desc_t* desc, void const * input, void * output) {
  TRI_FreeString(TRI_CORE_MEM_ZONE, desc->_name);

  if (desc->_desc != NULL) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, desc->_desc);
  }

  TRI_Free(TRI_CORE_MEM_ZONE, desc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a double argument
////////////////////////////////////////////////////////////////////////////////

static void ParseDoubleArg (char const * userarg, void * value) {
  po_double_t * desc;
  double tmp;

  assert(value != NULL);

  desc = value;
  tmp = TRI_DoubleString(userarg);

  if (TRI_errno() == TRI_ERROR_NO_ERROR) {
    *desc->_value = tmp;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a double option
////////////////////////////////////////////////////////////////////////////////

static void CreateDoubleOption (po_double_t * desc, void const * input, void * output) {
  TRI_PO_item_t item;
  TRI_program_options_t * po;
  struct option doubleOpt;

  po = (TRI_program_options_t*) (output);

  InitOptionStructure(&doubleOpt, desc->base._name, 1, 0, (int)(po->_longopts._length));

  memset(&item, 0, sizeof(item));

  item._desc = &desc->base;
  item.parse = ParseDoubleArg;

  TRI_PushBackVector(&po->_longopts, &doubleOpt);
  TRI_PushBackVector(&po->_items, &item);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a flag argument
////////////////////////////////////////////////////////////////////////////////

static void ParseFlagArg (char const * userarg, void * value) {
  po_flag_t * flag;

  assert(value != NULL);

  flag = (po_flag_t*) (value);

  if (flag->_value != NULL) {
    if (userarg == NULL) {
      *flag->_value = true;
    }
    else if (TRI_CaseEqualString(userarg, "yes")) {
      *flag->_value = true;
    }
    else if (TRI_CaseEqualString(userarg, "no")) {
      *flag->_value = false;
    }
    else if (TRI_CaseEqualString(userarg, "true")) {
      *flag->_value = true;
    }
    else if (TRI_CaseEqualString(userarg, "false")) {
      *flag->_value = false;
    }
    else if (TRI_CaseEqualString(userarg, "1")) {
      *flag->_value = true;
    }
    else if (TRI_CaseEqualString(userarg, "0")) {
      *flag->_value = false;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a flag option
////////////////////////////////////////////////////////////////////////////////

static void CreateFlagOption (po_flag_t * desc, void const * input, void * output) {
  TRI_PO_item_t item;
  TRI_program_options_t * po;
  struct option flagOpt;

  po = (TRI_program_options_t*) (output);

  if (desc->_value == 0) {
    InitOptionStructure(&flagOpt, desc->base._name, 0, 0, po->_longopts._length);
  }
  else {
    InitOptionStructure(&flagOpt, desc->base._name, 1, 0, po->_longopts._length);
  }

  memset(&item, 0, sizeof(item));

  item._desc = &desc->base;
  item.parse = ParseFlagArg;

  TRI_PushBackVector(&po->_longopts, &flagOpt);
  TRI_PushBackVector(&po->_items, &item);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a 16-bit integer argument
////////////////////////////////////////////////////////////////////////////////

static void ParseInt16Arg (char const * userarg, void * value) {
  po_int16_t * desc;
  int32_t tmp;

  assert(value != NULL);

  desc = (po_int16_t*) (value);
  tmp = TRI_Int32String(userarg);

  if (TRI_errno() == TRI_ERROR_NO_ERROR) {
    if (INT16_MIN <= tmp && tmp <= INT16_MAX) {
      *desc->_value = tmp;
    }
    else {
      TRI_set_errno(TRI_ERROR_NUMERIC_OVERFLOW);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a 16-bit integer option
////////////////////////////////////////////////////////////////////////////////

void CreateInt16Option (po_int16_t * desc, void const * input, void * output) {
  TRI_PO_item_t item;
  TRI_program_options_t * po;
  struct option intOpt;

  po = (TRI_program_options_t*) (output);

  InitOptionStructure(&intOpt, desc->base._name, 1, 0, po->_longopts._length);

  memset(&item, 0, sizeof(item));

  item._desc = &desc->base;
  item.parse = ParseInt16Arg;

  TRI_PushBackVector(&po->_longopts, &intOpt);
  TRI_PushBackVector(&po->_items, &item);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a 32-bit integer argument
////////////////////////////////////////////////////////////////////////////////

static void ParseInt32Arg (char const * userarg, void * value) {
  po_int32_t * desc;
  int32_t tmp;

  assert(value != NULL);

  desc = (po_int32_t*) (value);
  tmp = TRI_Int32String(userarg);

  if (TRI_errno() == TRI_ERROR_NO_ERROR) {
    *desc->_value = tmp;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a 32-bit integer option
////////////////////////////////////////////////////////////////////////////////

void CreateInt32Option (po_int32_t * desc, void const * input, void * output) {
  TRI_PO_item_t item;
  TRI_program_options_t * po;
  struct option intOpt;

  po = (TRI_program_options_t*) (output);

  InitOptionStructure(&intOpt, desc->base._name, 1, 0, po->_longopts._length);

  memset(&item, 0, sizeof(item));

  item._desc = &desc->base;
  item.parse = ParseInt32Arg;

  TRI_PushBackVector(&po->_longopts, &intOpt);
  TRI_PushBackVector(&po->_items, &item);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a 64-bit integer argument
////////////////////////////////////////////////////////////////////////////////

static void ParseInt64Arg (char const * userarg, void * value) {
  po_int64_t * desc;
  int64_t tmp;

  assert(value != NULL);

  desc = (po_int64_t*) (value);
  tmp = TRI_Int64String(userarg);

  if (TRI_errno() == TRI_ERROR_NO_ERROR) {
    *desc->_value = tmp;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a 64-bit integer option
////////////////////////////////////////////////////////////////////////////////

static void CreateInt64Option (po_int64_t * desc, void const * input, void * output) {
  TRI_PO_item_t item;
  TRI_program_options_t * po;
  struct option intOpt;

  po = (TRI_program_options_t*) (output);

  InitOptionStructure(&intOpt, desc->base._name, 1, 0, po->_longopts._length);

  memset(&item, 0, sizeof(item));

  item._desc = &desc->base;
  item.parse = ParseInt64Arg;

  TRI_PushBackVector(&po->_longopts, &intOpt);
  TRI_PushBackVector(&po->_items, &item);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a section
////////////////////////////////////////////////////////////////////////////////

static void FreeSectionOption (TRI_PO_section_t* desc, void const * input, void * output) {
  TRI_DestroyVectorPointer(&desc->_children);
  FreeOption(&desc->base, NULL, NULL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a section option
////////////////////////////////////////////////////////////////////////////////

static void CreateSectionOption (TRI_PO_section_t * section, void const * input, void * output) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a string
////////////////////////////////////////////////////////////////////////////////

static void ParseStringArg (char const * userarg, void * value) {
  TRI_PO_string_t * desc;

  assert(value != NULL);
  assert(userarg != NULL);

  desc = (TRI_PO_string_t*) value;

  if (*desc->_value != NULL) {
    TRI_Free(TRI_CORE_MEM_ZONE, *desc->_value);
  }

  *desc->_value = TRI_DuplicateString(userarg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a string option
////////////////////////////////////////////////////////////////////////////////

static void CreateStringOption (TRI_PO_string_t * desc, void const * input, void * output) {
  TRI_PO_item_t item;
  TRI_program_options_t * po;
  struct option stringOpt;

  po = (TRI_program_options_t *) output;

  InitOptionStructure(&stringOpt, desc->base._name, 1, 0, po->_longopts._length);

  memset(&item, 0, sizeof(item));

  item._desc = &desc->base;
  item.parse = ParseStringArg;

  TRI_PushBackVector(&po->_longopts, &stringOpt);
  TRI_PushBackVector(&po->_items, &item);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses an unsigned 16-bit integer argument
////////////////////////////////////////////////////////////////////////////////

static void ParseUInt16Arg (char const * userarg, void * value) {
  po_uint16_t * desc;
  uint32_t tmp;

  assert(value != NULL);

  desc = value;
  tmp = TRI_UInt32String(userarg);

  if (TRI_errno() == TRI_ERROR_NO_ERROR) {
    if (tmp <= UINT16_MAX) {
      *desc->_value = tmp;
    }
    else {
      TRI_set_errno(TRI_ERROR_NUMERIC_OVERFLOW);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an unsigned 16-bit integer option
////////////////////////////////////////////////////////////////////////////////

static void CreateUInt16Option (po_uint16_t * desc, void const * input, void * output) {
  TRI_PO_item_t item;
  TRI_program_options_t * po;
  struct option intOpt;

  po = (TRI_program_options_t*) (output);

  InitOptionStructure(&intOpt, desc->base._name, 1, 0, po->_longopts._length);

  memset(&item, 0, sizeof(item));

  item._desc = &desc->base;
  item.parse = ParseUInt16Arg;

  TRI_PushBackVector(&po->_longopts, &intOpt);
  TRI_PushBackVector(&po->_items, &item);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a 32-bit integer argument
////////////////////////////////////////////////////////////////////////////////

static void ParseUInt32Arg (char const * userarg, void * value) {
  po_uint32_t * desc;
  uint32_t tmp;

  assert(value != NULL);

  desc = value;
  tmp = TRI_UInt32String(userarg);

  if (TRI_errno() == TRI_ERROR_NO_ERROR) {
    *desc->_value = tmp;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a 32-bit integer option
////////////////////////////////////////////////////////////////////////////////

static void CreateUInt32Option (po_uint32_t * desc, void const * input, void * output) {
  TRI_PO_item_t item;
  TRI_program_options_t * po;
  struct option intOpt;

  po = (TRI_program_options_t*) (output);

  InitOptionStructure(&intOpt, desc->base._name, 1, 0, po->_longopts._length);

  memset(&item, 0, sizeof(item));

  item._desc = &desc->base;
  item.parse = ParseUInt32Arg;

  TRI_PushBackVector(&po->_longopts, &intOpt);
  TRI_PushBackVector(&po->_items, &item);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a 64-bit integer argument
////////////////////////////////////////////////////////////////////////////////

static void ParseUInt64Arg (char const * userarg, void * value) {
  po_uint64_t * desc;
  uint64_t tmp;

  assert(value != NULL);

  desc = value;
  tmp = TRI_UInt64String(userarg);

  if (TRI_errno() == TRI_ERROR_NO_ERROR) {
    *desc->_value = tmp;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a 64-bit integer option
////////////////////////////////////////////////////////////////////////////////

static void CreateUInt64Option (po_uint64_t * desc, void const * input, void * output) {
  TRI_PO_item_t item;
  TRI_program_options_t * po;
  struct option intOpt;

  po = (TRI_program_options_t*) (output);

  InitOptionStructure(&intOpt, desc->base._name, 1, 0, po->_longopts._length);

  memset(&item, 0, sizeof(item));

  item._desc = &desc->base;
  item.parse = ParseUInt64Arg;

  TRI_PushBackVector(&po->_longopts, &intOpt);
  TRI_PushBackVector(&po->_items, &item);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a vector of strings
////////////////////////////////////////////////////////////////////////////////

static void ParseVectorStringArg (char const * userarg, void * value) {
  TRI_PO_vector_string_t * desc;

  assert(value != NULL);
  assert(userarg != NULL);

  desc = value;

  TRI_PushBackVectorString(desc->_value, TRI_DuplicateString(userarg));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a vector string option
////////////////////////////////////////////////////////////////////////////////

static void CreateVectorStringOption (TRI_PO_vector_string_t * desc, void const * input, void * output) {
  TRI_PO_item_t item;
  TRI_program_options_t * po;
  struct option vectorOpt;

  po = (TRI_program_options_t*) (output);

  InitOptionStructure(&vectorOpt, desc->base._name, 1, 0, po->_longopts._length);

  memset(&item, 0, sizeof(item));

  item._desc = &desc->base;
  item.parse = ParseVectorStringArg;

  TRI_PushBackVector(&po->_longopts, &vectorOpt);
  TRI_PushBackVector(&po->_items, &item);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a vector string option
////////////////////////////////////////////////////////////////////////////////

static void VisitProgramOptions (TRI_PO_desc_t * ptr,
                                 po_visit_functions_t * functions,
                                 void const * input,
                                 void * output) {
  TRI_PO_section_t* section;
  TRI_PO_desc_t* child;
  size_t n;
  size_t i;

  switch (ptr->_type) {
    case TRI_PO_DOUBLE:
      functions->visitDoubleNode((po_double_t*) ptr, input, output);
      break;

    case TRI_PO_FLAG:
      functions->visitFlagNode((po_flag_t *) ptr, input, output);
      break;

    case TRI_PO_INT16:
      functions->visitInt16Node((po_int16_t *) ptr, input, output);
      break;

    case TRI_PO_INT32:
      functions->visitInt32Node((po_int32_t *) ptr, input, output);
      break;

    case TRI_PO_INT64:
      functions->visitInt64Node((po_int64_t *) ptr, input, output);
      break;

    case TRI_PO_SECTION:
      section = (TRI_PO_section_t*) ptr;

      if (functions->visitSectionNodeBefore != NULL) {
        functions->visitSectionNodeBefore(section, input, output);
      }

      n = section->_children._length;

      for (i = 0;  i < n;  ++i) {
        child = section->_children._buffer[i];

        VisitProgramOptions(child, functions, input, output);
      }

      if (functions->visitSectionNodeAfter != NULL) {
        functions->visitSectionNodeAfter(section, input, output);
      }

      break;

    case TRI_PO_STRING:
      functions->visitStringNode((TRI_PO_string_t*) ptr, input, output);
      break;

    case TRI_PO_UINT16:
      functions->visitUInt16Node((po_uint16_t*) ptr, input, output);
      break;

    case TRI_PO_UINT32:
      functions->visitUInt32Node((po_uint32_t*) ptr, input, output);
      break;

    case TRI_PO_UINT64:
      functions->visitUInt64Node((po_uint64_t*) ptr, input, output);
      break;

    case TRI_PO_VECTOR_STRING:
      functions->visitVectorStringNode((TRI_PO_vector_string_t *) ptr, input, output);
      break;

    default:
      printf("type unknown");
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles an option entry
////////////////////////////////////////////////////////////////////////////////

static bool HandleOption (TRI_program_options_t * options,
                          char const* section,
                          char const* option,
                          char const* value) {
  size_t i;
  char* full;

  if (*section == '\0') {
    full = TRI_DuplicateString(option);
  }
  else {
    full = TRI_Concatenate3String(section, ".", option);
  }


  for (i = 0;  i < options->_items._length;  ++i) {
    TRI_PO_item_t * item;

    item = TRI_AtVector(&options->_items, i);

    if (TRI_EqualString(full, item->_desc->_name)) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, full);

      item->parse(value, item->_desc);
      item->_used = true;

      return true;
    }
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, full);

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     USAGE VISTORS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ProgramOptions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief usage for any option
////////////////////////////////////////////////////////////////////////////////

static void UsageNode (TRI_PO_desc_t * desc, char const * arg, void * output) {
  TRI_string_buffer_t * buffer;

  buffer = output;

  TRI_AppendStringStringBuffer(buffer, "  --");
  TRI_AppendStringStringBuffer(buffer, desc->_name);

  if (desc->_short != '\0') {
    TRI_AppendStringStringBuffer(buffer, " [-");
    TRI_AppendCharStringBuffer(buffer, desc->_short);
    TRI_AppendStringStringBuffer(buffer, "] ");
  }

  if (*arg != '\0') {
    TRI_AppendStringStringBuffer(buffer, " <");
    TRI_AppendStringStringBuffer(buffer, arg);
    TRI_AppendStringStringBuffer(buffer, "> ");
  }
  else {
    TRI_AppendStringStringBuffer(buffer, " ");
  }

  TRI_AppendStringStringBuffer(buffer, desc->_desc);
  TRI_AppendStringStringBuffer(buffer, "\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief usage for double option
////////////////////////////////////////////////////////////////////////////////

static void UsageDoubleNode (po_double_t * desc, void const * input, void * output) {
  UsageNode(&desc->base, "double", output);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief usage for flag option
////////////////////////////////////////////////////////////////////////////////

static void UsageFlagNode (po_flag_t * desc, void const * input, void * output) {
  UsageNode(&desc->base, "", output);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief usage for 16-bit integer option
////////////////////////////////////////////////////////////////////////////////

static void UsageInt16Node (po_int16_t * desc, void const * input, void * output) {
  UsageNode(&desc->base, "int16", output);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief usage for 32-bit integer option
////////////////////////////////////////////////////////////////////////////////

static void UsageInt32Node (po_int32_t * desc, void const * input, void * output) {
  UsageNode(&desc->base, "int32", output);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief usage for 64-bit integer option
////////////////////////////////////////////////////////////////////////////////

static void UsageInt64Node (po_int64_t * desc, void const * input, void * output) {
  UsageNode(&desc->base, "int64", output);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief usage for string option
////////////////////////////////////////////////////////////////////////////////

static void UsageStringNode (TRI_PO_string_t * desc, void const * input, void * output) {
  UsageNode(&desc->base, "string", output);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief usage for section option
////////////////////////////////////////////////////////////////////////////////

static void UsageSectionNode (TRI_PO_section_t * section, void const * input, void * output) {
  TRI_string_buffer_t * buffer;
  char const* description;

  buffer = output;

  if (section->base._desc != NULL) {
    description = section->base._desc;
  }
  else if (section->base._name != NULL) {
    description = section->base._name;
  }
  else {
    description = NULL;
  }

  if (description != NULL) {
    TRI_AppendStringStringBuffer(buffer, "\n");
    TRI_AppendStringStringBuffer(buffer, description);
    TRI_AppendStringStringBuffer(buffer, " options: ");
    TRI_AppendStringStringBuffer(buffer, "\n");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief usage for unsigned 16-bit integer option
////////////////////////////////////////////////////////////////////////////////

static void UsageUInt16Node (po_uint16_t * desc, void const * input, void * output) {
  UsageNode(&desc->base, "uint16", output);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief usage for unsigned 32-bit integer option
////////////////////////////////////////////////////////////////////////////////

static void UsageUInt32Node (po_uint32_t * desc, void const * input, void * output) {
  UsageNode(&desc->base, "uint32", output);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief usage for unsigned 64-bit integer option
////////////////////////////////////////////////////////////////////////////////

static void UsageUInt64Node (po_uint64_t * desc, void const * input, void * output) {
  UsageNode(&desc->base, "uint64", output);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief usage for vector string option
////////////////////////////////////////////////////////////////////////////////

static void UsageVectorStringNode (TRI_PO_vector_string_t * desc, void const * input, void * output) {
  UsageNode(&desc->base, "string", output);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ProgramOptions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new program options description
////////////////////////////////////////////////////////////////////////////////

TRI_PO_section_t* TRI_CreatePODescription (char const *description) {
  TRI_PO_section_t * desc;

  desc = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_PO_section_t), false);

  desc->base._type = TRI_PO_SECTION;
  desc->base._name = TRI_DuplicateString("Program Options");
  desc->base._desc = (description != NULL) ? TRI_DuplicateString(description) : NULL;

  TRI_InitVectorPointer(&desc->_children, TRI_CORE_MEM_ZONE);

  return desc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees all pointers for a program options description
////////////////////////////////////////////////////////////////////////////////

void TRI_FreePODescription (TRI_PO_section_t* desc) {
  po_visit_functions_t freeFunc;

  freeFunc.visitDoubleNode = (void(*)(po_double_t *, void const * input, void * output)) FreeOption;
  freeFunc.visitFlagNode = (void(*)(po_flag_t *, void const * input, void * output)) FreeOption;
  freeFunc.visitInt16Node = (void(*)(po_int16_t *, void const * input, void * output)) FreeOption;
  freeFunc.visitInt32Node = (void(*)(po_int32_t *, void const * input, void * output)) FreeOption;
  freeFunc.visitInt64Node = (void(*)(po_int64_t *, void const * input, void * output)) FreeOption;
  freeFunc.visitSectionNodeBefore = NULL;
  freeFunc.visitSectionNodeAfter = FreeSectionOption;
  freeFunc.visitStringNode = (void(*)(TRI_PO_string_t *, void const * input, void * output)) FreeOption;
  freeFunc.visitUInt16Node = (void(*)(po_uint16_t *, void const * input, void * output)) FreeOption;
  freeFunc.visitUInt32Node = (void(*)(po_uint32_t *, void const * input, void * output)) FreeOption;
  freeFunc.visitUInt64Node = (void(*)(po_uint64_t *, void const * input, void * output)) FreeOption;
  freeFunc.visitVectorStringNode = (void(*)(TRI_PO_vector_string_t *, void const * input, void * output)) FreeOption;

  VisitProgramOptions(&desc->base, &freeFunc, NULL, NULL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the runtime structures which handles the user options
////////////////////////////////////////////////////////////////////////////////

TRI_program_options_t * TRI_CreateProgramOptions (TRI_PO_section_t * desc) {
  TRI_program_options_t * po;
  po_visit_functions_t optionBuilders;
  struct option nullOpt;

  po = (TRI_program_options_t*) (TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_program_options_t), false));

  // TODO: not safe since po could be 0

  TRI_InitVector(&po->_longopts, TRI_CORE_MEM_ZONE, sizeof(struct option));
  TRI_InitVector(&po->_items, TRI_CORE_MEM_ZONE, sizeof(TRI_PO_item_t));

  optionBuilders.visitDoubleNode = CreateDoubleOption;
  optionBuilders.visitFlagNode = CreateFlagOption;
  optionBuilders.visitInt16Node = CreateInt16Option;
  optionBuilders.visitInt32Node = CreateInt32Option;
  optionBuilders.visitInt64Node = CreateInt64Option;
  optionBuilders.visitSectionNodeBefore = CreateSectionOption;
  optionBuilders.visitSectionNodeAfter = NULL;
  optionBuilders.visitStringNode = CreateStringOption;
  optionBuilders.visitUInt16Node = CreateUInt16Option;
  optionBuilders.visitUInt32Node = CreateUInt32Option;
  optionBuilders.visitUInt64Node = CreateUInt64Option;
  optionBuilders.visitVectorStringNode = CreateVectorStringOption;

  VisitProgramOptions(&desc->base, &optionBuilders, NULL, po);

  // last element in vector has to be the null option
  InitOptionStructure(&nullOpt, 0, 0, 0, 0);
  TRI_PushBackVector(&po->_longopts, &nullOpt);

  // setup argument vector
  TRI_InitVectorString(&po->_arguments, TRI_CORE_MEM_ZONE);

  return po;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroyes alls allocated memory, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyProgramOptions (TRI_program_options_t * options) {
  TRI_DestroyVector(&options->_longopts);
  TRI_DestroyVector(&options->_items);
  TRI_DestroyVectorString(&options->_arguments);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroyes alls allocated memory and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeProgramOptions (TRI_program_options_t * options) {
  TRI_DestroyProgramOptions(options);
  TRI_Free(TRI_CORE_MEM_ZONE, options);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ProgramOptions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a double option
////////////////////////////////////////////////////////////////////////////////

void TRI_AddDoublePODescription (TRI_PO_section_t * desc,
                                 char const * name,
                                 char shortName,
                                 char const* description,
                                 double *variable) {
  po_double_t * res;

  assert(name != NULL);
  assert(variable != NULL);

  res = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(po_double_t), false);

  res->base._type  = TRI_PO_DOUBLE;
  res->base._name  = TRI_DuplicateString(name);
  res->base._short = shortName;
  res->base._desc  = (description != NULL) ? TRI_DuplicateString(description) : NULL;

  res->_value = variable;

  TRI_PushBackVectorPointer(&desc->_children, res);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a flag option
////////////////////////////////////////////////////////////////////////////////

void TRI_AddFlagPODescription (TRI_PO_section_t * desc,
                               char const * name,
                               char shortName,
                               char const * description,
                               bool * variable) {
  po_flag_t * res;

  assert(name != NULL);

  res = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(po_flag_t), false);

  res->base._type  = TRI_PO_FLAG;
  res->base._name  = TRI_DuplicateString(name);
  res->base._short = shortName;
  res->base._desc  = (description != NULL) ? TRI_DuplicateString(description) : NULL;

  res->_value = variable;

  TRI_PushBackVectorPointer(&desc->_children, res);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a 16-bit integer option
////////////////////////////////////////////////////////////////////////////////

void TRI_AddInt16PODescription (TRI_PO_section_t * desc,
                                char const * name,
                                char shortName,
                                char const * description,
                                int16_t * variable) {

  po_int16_t * res;

  assert(variable != NULL);
  assert(name != NULL);

  res = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(po_int16_t), false);

  res->base._type  = TRI_PO_INT16;
  res->base._name  = TRI_DuplicateString(name);
  res->base._short = shortName;
  res->base._desc  = (description != NULL) ? TRI_DuplicateString(description) : NULL;

  res->_value = variable;

  TRI_PushBackVectorPointer(&desc->_children, res);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a 32-bit integer option
////////////////////////////////////////////////////////////////////////////////

void TRI_AddInt32PODescription (TRI_PO_section_t * desc,
                                char const * name,
                                char shortName,
                                char const * description,
                                int32_t * variable) {

  po_int32_t * res;

  assert(variable != NULL);
  assert(name != NULL);

  res = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(po_int32_t), false);

  res->base._type  = TRI_PO_INT32;
  res->base._name  = TRI_DuplicateString(name);
  res->base._short = shortName;
  res->base._desc  = (description != NULL) ? TRI_DuplicateString(description) : NULL;

  res->_value = variable;

  TRI_PushBackVectorPointer(&desc->_children, res);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a 64-bit integer option
////////////////////////////////////////////////////////////////////////////////

void TRI_AddInt64PODescription (TRI_PO_section_t * desc,
                                char const * name,
                                char shortName,
                                char const * description,
                                int64_t * variable) {

  po_int64_t * res;

  assert(variable != NULL);
  assert(name != NULL);

  res = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(po_int64_t), false);

  res->base._type  = TRI_PO_INT64;
  res->base._name  = TRI_DuplicateString(name);
  res->base._short = shortName;
  res->base._desc  = (description != NULL) ? TRI_DuplicateString(description) : NULL;

  res->_value = variable;

  TRI_PushBackVectorPointer(&desc->_children, res);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a string option
////////////////////////////////////////////////////////////////////////////////

void TRI_AddStringPODescription (TRI_PO_section_t * desc,
                                 char const * name,
                                 char shortName,
                                 char const * description,
                                 char ** variable) {
  TRI_PO_string_t * res;

  assert(variable != NULL);
  assert(name != NULL);

  res = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_PO_string_t), false);

  res->base._type  = TRI_PO_STRING;
  res->base._name  = TRI_DuplicateString(name);
  res->base._short = shortName;
  res->base._desc  = (description != NULL) ? TRI_DuplicateString(description) : NULL;

  res->_value = variable;

  TRI_PushBackVectorPointer(&desc->_children, res);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an unsigned 16-bit integer option
////////////////////////////////////////////////////////////////////////////////

void TRI_AddUInt16PODescription (TRI_PO_section_t * desc,
                                 char const * name,
                                 char shortName,
                                 char const * description,
                                 uint16_t * variable) {

  po_uint16_t * res;

  assert(variable != NULL);
  assert(name != NULL);

  res = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(po_uint16_t), false);

  res->base._type  = TRI_PO_UINT16;
  res->base._name  = TRI_DuplicateString(name);
  res->base._short = shortName;
  res->base._desc  = (description != NULL) ? TRI_DuplicateString(description) : NULL;

  res->_value = variable;

  TRI_PushBackVectorPointer(&desc->_children, res);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an unsigned 32-bit integer option
////////////////////////////////////////////////////////////////////////////////

void TRI_AddUInt32PODescription (TRI_PO_section_t * desc,
                                 char const * name,
                                 char shortName,
                                 char const * description,
                                 uint32_t * variable) {

  po_uint32_t * res;

  assert(variable != NULL);
  assert(name != NULL);

  res = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(po_uint32_t), false);

  res->base._type  = TRI_PO_UINT32;
  res->base._name  = TRI_DuplicateString(name);
  res->base._short = shortName;
  res->base._desc  = (description != NULL) ? TRI_DuplicateString(description) : NULL;

  res->_value = variable;

  TRI_PushBackVectorPointer(&desc->_children, res);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an unsigned 64-bit integer option
////////////////////////////////////////////////////////////////////////////////

void TRI_AddUInt64PODescription (TRI_PO_section_t * desc,
                                 char const * name,
                                 char shortName,
                                 char const * description,
                                 uint64_t * variable) {

  po_uint64_t * res;

  assert(variable != NULL);
  assert(name != NULL);

  res = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(po_uint64_t), false);

  res->base._type  = TRI_PO_UINT64;
  res->base._name  = TRI_DuplicateString(name);
  res->base._short = shortName;
  res->base._desc  = (description != NULL) ? TRI_DuplicateString(description) : NULL;

  res->_value = variable;

  TRI_PushBackVectorPointer(&desc->_children, res);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a vector string option
////////////////////////////////////////////////////////////////////////////////

void TRI_AddVectorStringPODescription (TRI_PO_section_t * desc,
                                       char const * name,
                                       char shortName,
                                       char const * description,
                                       TRI_vector_string_t * variable) {

  TRI_PO_vector_string_t * res;

  assert(variable != NULL);
  assert(name != NULL);

  res = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_PO_vector_string_t), false);

  res->base._type  = TRI_PO_VECTOR_STRING;
  res->base._name  = TRI_DuplicateString(name);
  res->base._short = shortName;
  res->base._desc  = (description != NULL) ? TRI_DuplicateString(description) : NULL;

  res->_value = variable;

  TRI_PushBackVectorPointer(&desc->_children, res);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new section
////////////////////////////////////////////////////////////////////////////////

void TRI_AddOptionsPODescription (TRI_PO_section_t * parent, TRI_PO_section_t * child) {
  assert(parent != NULL);
  assert(child != NULL);

  TRI_PushBackVectorPointer(&parent->_children, child);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds a usage string
////////////////////////////////////////////////////////////////////////////////

char * TRI_UsagePODescription (TRI_PO_section_t * desc) {
  po_visit_functions_t visitors;
  TRI_string_buffer_t buffer;

  visitors.visitDoubleNode = UsageDoubleNode;
  visitors.visitFlagNode = UsageFlagNode;
  visitors.visitInt16Node = UsageInt16Node;
  visitors.visitInt32Node = UsageInt32Node;
  visitors.visitInt64Node = UsageInt64Node;
  visitors.visitSectionNodeBefore = UsageSectionNode;
  visitors.visitSectionNodeAfter = NULL;
  visitors.visitStringNode = UsageStringNode;
  visitors.visitUInt16Node = UsageUInt16Node;
  visitors.visitUInt32Node = UsageUInt32Node;
  visitors.visitUInt64Node = UsageUInt64Node;
  visitors.visitVectorStringNode = UsageVectorStringNode;

  TRI_InitStringBuffer(&buffer, TRI_CORE_MEM_ZONE);
  VisitProgramOptions(&desc->base, &visitors, NULL, &buffer);

  // the caller has to free this buffer
  return buffer._buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a command line of arguments
////////////////////////////////////////////////////////////////////////////////

bool TRI_ParseArgumentsProgramOptions (TRI_program_options_t * options,
                                       int argc,
                                       char ** argv) {
  extern char *optarg;
  extern int optind;

  TRI_string_buffer_t buffer;
  TRI_PO_item_t * item;
  char const* shortOptions;
  size_t i;
  int c;
  int idx;
  int maxIdx;

  TRI_set_errno(TRI_ERROR_NO_ERROR);

  TRI_InitStringBuffer(&buffer, TRI_CORE_MEM_ZONE);

  for (i = 0;  i < options->_items._length;  ++i) {
    item = TRI_AtVector(&options->_items, i);

    if (item->_desc->_short != '\0') {
      TRI_AppendCharStringBuffer(&buffer, item->_desc->_short);

      if (item->_desc->_type == TRI_PO_FLAG) {
        po_flag_t* p;

        p = (po_flag_t*) item->_desc;

        if (p->_value != 0) {
          TRI_AppendCharStringBuffer(&buffer, ':');
        }
      }
      else {
        TRI_AppendCharStringBuffer(&buffer, ':');
      }
    }
  }

  if (TRI_LengthStringBuffer(&buffer) == 0) {
    shortOptions = "";
  }
  else {
    shortOptions = TRI_BeginStringBuffer(&buffer);
  }

  optind = 1;
  maxIdx = options->_items._length;

  while (true) {
    c = getopt_long(argc, argv, shortOptions, (const struct option*) options->_longopts._buffer, &idx);

    if (c == -1) {
      for (i = optind;  i < (size_t) argc;  ++i) {
        TRI_PushBackVectorString(&options->_arguments, TRI_DuplicateString(argv[i]));
      }

      break;
    }

    if (c < 256) {
      for (i = 0;  i < options->_items._length;  ++i) {
        item = TRI_AtVector(&options->_items, i);

        if (item->_desc->_short == c) {
          break;
        }
      }

      if (i == options->_items._length) {
        TRI_set_errno(TRI_ERROR_ILLEGAL_OPTION);
        TRI_DestroyStringBuffer(&buffer);
        return false;
      }
    }
    else {
      c -= 256;

      if (c >= maxIdx) {
        TRI_set_errno(TRI_ERROR_ILLEGAL_OPTION);
        TRI_DestroyStringBuffer(&buffer);
        return false;
      }

      item = TRI_AtVector(&options->_items, c);
    }

    // the opt.. are external variables
    item->_used = true;
    item->parse(optarg, item->_desc);
  }

  TRI_AnnihilateStringBuffer(&buffer);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a text file containing the configuration variables
////////////////////////////////////////////////////////////////////////////////

bool TRI_ParseFileProgramOptions (TRI_program_options_t * options,
                                  char const * programName,
                                  char const * filename) {
  FILE* f;
  bool ok;
  char* buffer;
  char* option;
  char* section;
  char* value;
  char* tmpSection;
  int res;
  regex_t re1;
  regex_t re2;
  regex_t re3;
  regex_t re4;
  regex_t re5;
  regex_t re6;
  regmatch_t matches[4];
  size_t tmp;
  ssize_t len;

  TRI_set_errno(TRI_ERROR_NO_ERROR);

  f = fopen(filename, "r");

  if (f == NULL) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    return false;
  }

  // used by routine getline()
  buffer = NULL;
  len = 0;

  // current section
  section = TRI_DuplicateString("");

  // regexp for comments
  regcomp(&re1, "^[ \t]*(#.*)?$", REG_ICASE | REG_EXTENDED | REG_NOSUB);

  // regexp for section marker
  regcomp(&re2, "^[ \t]*\\[([-_a-z0-9]*)\\][ \t]*$", REG_ICASE | REG_EXTENDED);

  // regexp for option = value
  regcomp(&re3, "^[ \t]*([-_a-z0-9]*)[ \t]*=[ \t]*(.*[^ \t])[ \t]*$", REG_ICASE | REG_EXTENDED);

  // regexp for option =
  regcomp(&re4, "^[ \t]*([-_a-z0-9]*)[ \t]*=[ \t]*$", REG_ICASE | REG_EXTENDED);

  // regexp for section.option = value
  regcomp(&re5, "^[ \t]*([-_a-z0-9]*)\\.([-_a-z0-9]*)[ \t]*=[ \t]*(.*[^ \t])[ \t]*$", REG_ICASE | REG_EXTENDED);

  // regexp for section.option =
  regcomp(&re6, "^[ \t]*([-_a-z0-9]*)\\.([-_a-z0-9]*)[ \t]*=[ \t]*$", REG_ICASE | REG_EXTENDED);

  // corresponds to the argc variable of the main routine
  ok = true;

  while ((len = getline(&buffer, &tmp, f)) > 0) {
    while (0 < len && buffer[len - 1] == '\n') {
      buffer[len - 1] = '\0';
      --len;
    }

    // check for comments
    res = regexec(&re1, buffer, 0, 0, 0);

    if (res == 0) {
      TRI_SystemFree(buffer);
      buffer = NULL;
      continue;
    }

    // check for section marker
    res = regexec(&re2, buffer, sizeof(matches) / sizeof(matches[0]), matches, 0);

    if (res == 0) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, section);

      section = TRI_DuplicateString2(buffer + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
      TRI_SystemFree(buffer);
      buffer = NULL;

      continue;
    }

    // check for option = value
    res = regexec(&re3, buffer, sizeof(matches) / sizeof(matches[0]), matches, 0);

    if (res == 0) {
      option = TRI_DuplicateString2(buffer + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
      value = TRI_DuplicateString2(buffer + matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);

      TRI_SystemFree(buffer);
      buffer = NULL;

      ok = HandleOption(options, section, option, value);

      TRI_FreeString(TRI_CORE_MEM_ZONE, value);

      if (! ok) {
        TRI_set_errno(TRI_ERROR_ILLEGAL_OPTION);

        if (*section != '\0') {
          fprintf(stderr, "%s: unrecognized option '%s.%s'\n", programName, section, option);
        }
        else {
          fprintf(stderr, "%s: unrecognized option '%s'\n", programName, option);
        }

        TRI_FreeString(TRI_CORE_MEM_ZONE, option);
        break;
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, option);
      continue;
    }

    // check for option =
    res = regexec(&re4, buffer, sizeof(matches) / sizeof(matches[0]), matches, 0);

    if (res == 0) {
      option = TRI_DuplicateString2(buffer + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);

      TRI_SystemFree(buffer);
      buffer = NULL;

      ok = HandleOption(options, section, option, "");

      if (! ok) {
        TRI_set_errno(TRI_ERROR_ILLEGAL_OPTION);

        if (*section != '\0') {
          fprintf(stderr, "%s: unrecognized option '%s.%s'\n", programName, section, option);
        }
        else {
          fprintf(stderr, "%s: unrecognized option '%s'\n", programName, option);
        }

        TRI_FreeString(TRI_CORE_MEM_ZONE, option);
        break;
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, option);
      continue;
    }

    // check for option = value
    res = regexec(&re5, buffer, sizeof(matches) / sizeof(matches[0]), matches, 0);

    if (res == 0) {
      tmpSection = TRI_DuplicateString2(buffer + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
      option = TRI_DuplicateString2(buffer + matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);
      value = TRI_DuplicateString2(buffer + matches[3].rm_so, matches[3].rm_eo - matches[3].rm_so);

      TRI_SystemFree(buffer);
      buffer = NULL;

      ok = HandleOption(options, tmpSection, option, value);

      TRI_FreeString(TRI_CORE_MEM_ZONE, value);

      if (! ok) {
        TRI_set_errno(TRI_ERROR_ILLEGAL_OPTION);
        fprintf(stderr, "%s: unrecognized option '%s.%s'\n", programName, tmpSection, option);

        TRI_FreeString(TRI_CORE_MEM_ZONE, tmpSection);
        TRI_FreeString(TRI_CORE_MEM_ZONE, option);
        break;
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, tmpSection);
      TRI_FreeString(TRI_CORE_MEM_ZONE, option);
      continue;
    }

    // check for option =
    res = regexec(&re6, buffer, sizeof(matches) / sizeof(matches[0]), matches, 0);

    if (res == 0) {
      tmpSection = TRI_DuplicateString2(buffer + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
      option = TRI_DuplicateString2(buffer + matches[2].rm_so, matches[2].rm_eo - matches[1].rm_so);

      TRI_SystemFree(buffer);
      buffer = NULL;

      ok = HandleOption(options, tmpSection, option, "");

      if (! ok) {
        TRI_set_errno(TRI_ERROR_ILLEGAL_OPTION);
        fprintf(stderr, "%s: unrecognized option '%s.%s'\n", programName, tmpSection, option);

        TRI_FreeString(TRI_CORE_MEM_ZONE, tmpSection);
        TRI_FreeString(TRI_CORE_MEM_ZONE, option);
        break;
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, tmpSection);
      TRI_FreeString(TRI_CORE_MEM_ZONE, option);
      continue;
    }

    TRI_set_errno(TRI_ERROR_ILLEGAL_OPTION);
    fprintf(stderr, "%s: unrecognized entry '%s'\n", programName, buffer);

    TRI_SystemFree(buffer);
    buffer = NULL;
    break;
  }

  if (buffer != NULL) {
    TRI_SystemFree(buffer);
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, section);

  regfree(&re1);
  regfree(&re2);
  regfree(&re3);
  regfree(&re4);
  regfree(&re5);
  regfree(&re6);

  fclose(f);

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if the option was given
////////////////////////////////////////////////////////////////////////////////

bool TRI_HasOptionProgramOptions (TRI_program_options_t const * options, char const * name) {
  size_t i;

  for (i = 0;  i < options->_items._length;  ++i) {
    TRI_PO_item_t * item;

    item = TRI_AtVector(&options->_items, i);

    if (item->_used && TRI_EqualString(name, item->_desc->_name)) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

