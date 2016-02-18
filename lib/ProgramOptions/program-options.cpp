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
/// @author Esteban Lombeyda
////////////////////////////////////////////////////////////////////////////////

#include "program-options.h"

#include <getopt.h>
#include <regex.h>

#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/levenshtein.h"
#include "Basics/StringBuffer.h"
#include "Basics/tri-strings.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief description of a double
////////////////////////////////////////////////////////////////////////////////

typedef struct po_double_s {
  TRI_PO_desc_t base;

  double* _value;
} po_double_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief description of an attribute (a flag) without arguments
////////////////////////////////////////////////////////////////////////////////

typedef struct po_flag_s {
  TRI_PO_desc_t base;

  bool* _value;
} po_flag_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief description of a signed 16-bit integer
////////////////////////////////////////////////////////////////////////////////

typedef struct po_int16_s {
  TRI_PO_desc_t base;

  int16_t* _value;
} po_int16_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief description of a signed 32-bit integer
////////////////////////////////////////////////////////////////////////////////

typedef struct po_int32_s {
  TRI_PO_desc_t base;

  int32_t* _value;
} po_int32_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief description of a signed 64-bit integer
////////////////////////////////////////////////////////////////////////////////

typedef struct po_int64_s {
  TRI_PO_desc_t base;

  int64_t* _value;
} po_int64_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief description of an unsigned 16-bit integer
////////////////////////////////////////////////////////////////////////////////

typedef struct po_uint16_s {
  TRI_PO_desc_t base;

  uint16_t* _value;
} po_uint16_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief description of an unsigned 32-bit integer
////////////////////////////////////////////////////////////////////////////////

typedef struct po_uint32_s {
  TRI_PO_desc_t base;

  uint32_t* _value;
} po_uint32_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief description of an unsigned 64-bit integer
////////////////////////////////////////////////////////////////////////////////

typedef struct po_uint64_s {
  TRI_PO_desc_t base;

  uint64_t* _value;
} po_uint64_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief set of evaluation functions
////////////////////////////////////////////////////////////////////////////////

typedef struct po_visit_functions_s {
  void (*visitDoubleNode)(po_double_t*, const void* input, void* output);
  void (*visitFlagNode)(po_flag_t*, const void* input, void* output);
  void (*visitInt16Node)(po_int16_t*, const void* input, void* output);
  void (*visitInt32Node)(po_int32_t*, const void* input, void* output);
  void (*visitInt64Node)(po_int64_t*, const void* input, void* output);
  void (*visitSectionNodeBefore)(TRI_PO_section_t*, const void* input,
                                 void* output);
  void (*visitSectionNodeAfter)(TRI_PO_section_t*, const void* input,
                                void* output);
  void (*visitStringNode)(TRI_PO_string_t*, const void* input, void* output);
  void (*visitUInt16Node)(po_uint16_t*, const void* input, void* output);
  void (*visitUInt32Node)(po_uint32_t*, const void* input, void* output);
  void (*visitUInt64Node)(po_uint64_t*, const void* input, void* output);
  void (*visitVectorStringNode)(TRI_PO_vector_string_t*, const void* input,
                                void* output);
} po_visit_functions_t;

static bool HasPrintedError = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief prints out an error message about an unrecognized option
////////////////////////////////////////////////////////////////////////////////

static void printParseError(TRI_program_options_t const* options,
                            char const* programName, char const* option) {
  if (HasPrintedError) {
    // print only first error
    return;
  }

  HasPrintedError = true;
  fprintf(stderr, "%s: error parsing value of option '%s'\n", programName,
          option);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints out an error message about an unrecognized option
////////////////////////////////////////////////////////////////////////////////

static void printUnrecognizedOption(TRI_program_options_t const* options,
                                    char const* programName,
                                    char const* option) {
  if (HasPrintedError) {
    // print only first error
    return;
  }

  HasPrintedError = true;
  fprintf(stderr, "%s: unrecognized option '%s'\n", programName, option);

  std::multimap<int, std::string> distances;

  for (size_t i = 0; i < TRI_LengthVector(&options->_items); ++i) {
    auto item =
        static_cast<TRI_PO_item_t const*>(TRI_AtVector(&options->_items, i));

    distances.emplace(
        TRI_Levenshtein(std::string(option), std::string(item->_desc->_name)),
        item->_desc->_name);
  }

  if (!distances.empty()) {
    auto const value = distances.begin()->first;
    std::string suggestions;
    int i = 0;
    for (auto& item : distances) {
      if (item.first != value && (item.first - value) > 2) {
        break;
      }
      if (i > 0) {
        suggestions.append("  ");
      }
      suggestions.append("--");
      suggestions.append(item.second);
      ++i;

      if (i == 2) {
        break;
      }
    }
    fprintf(stderr, "Did you mean one of these? %s\n", suggestions.c_str());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints out an error message about an unrecognized option
////////////////////////////////////////////////////////////////////////////////

static void printUnrecognizedOption(TRI_program_options_t const* options,
                                    char const* programName,
                                    char const* section, char const* option) {
  printUnrecognizedOption(options, programName, (std::string(section) + "." +
                                                 std::string(option)).c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds and replaces variables
////////////////////////////////////////////////////////////////////////////////

static char* FillVariables(char const* value) {
  TRI_string_buffer_t buffer;

  char const* p;
  char const* e;
  char const* q;

  if (value == nullptr) {
    return nullptr;
  }

  p = value;
  e = p + strlen(value);

  TRI_InitSizedStringBuffer(&buffer, TRI_CORE_MEM_ZONE, strlen(value) + 1);

  for (q = p; q < e; q++) {
    if (*q == '@') {
      q++;

      if (*q == '@') {
        TRI_AppendCharStringBuffer(&buffer, '@');
      } else {
        char const* t = q;

        for (; q < e && *q != '@'; q++)
          ;

        if (q < e) {
          char* k = TRI_DuplicateString(t, q - t);
          char* v = getenv(k);

          if (v != nullptr && *v == '\0') {
            TRI_FreeString(TRI_CORE_MEM_ZONE, v);
            v = nullptr;
          }

          if (v == nullptr) {
#if _WIN32

            if (TRI_EqualString(k, "ROOTDIR")) {
              std::string vv = TRI_LocateInstallDirectory();

              if (!vv.empty()) {
                size_t lv = vv.length();

                if (vv[lv - 1] == TRI_DIR_SEPARATOR_CHAR || vv[lv - 1] == '/') {
                  v = TRI_DuplicateString(vv.c_str(), lv - 1);
                }
              }
            }

#endif

          } else {
            v = TRI_DuplicateString(v);
          }

          if (v != nullptr) {
            TRI_AppendStringStringBuffer(&buffer, v);
            TRI_FreeString(TRI_CORE_MEM_ZONE, v);
          }

          TRI_FreeString(TRI_CORE_MEM_ZONE, k);
        } else {
          TRI_AppendStringStringBuffer(&buffer, t - 1);
        }
      }
    } else {
      TRI_AppendCharStringBuffer(&buffer, *q);
    }
  }

  return TRI_StealStringBuffer(&buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize an option structure
////////////////////////////////////////////////////////////////////////////////

static struct option* InitOptionStructure(struct option* option,
                                          char const* name, int hasArg,
                                          int* flag, int val) {
  option->name = const_cast<char*>(name);
  option->has_arg = hasArg;
  option->flag = flag;
  option->val = 256 + val;

  return option;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a atomic option
////////////////////////////////////////////////////////////////////////////////

static void FreeOption(TRI_PO_desc_t* desc, const void* input, void* output) {
  TRI_FreeString(TRI_CORE_MEM_ZONE, desc->_name);

  if (desc->_desc != nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, desc->_desc);
  }

  TRI_Free(TRI_CORE_MEM_ZONE, desc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a double argument
////////////////////////////////////////////////////////////////////////////////

static int ParseDoubleArg(char const* userarg, void* value) {
  TRI_ASSERT(value != nullptr);

  po_double_t* desc = static_cast<po_double_t*>(value);
  double tmp = TRI_DoubleString(userarg);

  int res = TRI_errno();
  if (res == TRI_ERROR_NO_ERROR) {
    *desc->_value = tmp;
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a double option
////////////////////////////////////////////////////////////////////////////////

static void CreateDoubleOption(po_double_t* desc, const void* input,
                               void* output) {
  TRI_PO_item_t item;
  TRI_program_options_t* po;
  struct option doubleOpt;

  po = (TRI_program_options_t*)(output);

  InitOptionStructure(&doubleOpt, desc->base._name, 1, 0,
                      (int)TRI_LengthVector(&po->_longopts));

  memset(&item, 0, sizeof(item));

  item._desc = &desc->base;
  item.parse = ParseDoubleArg;

  TRI_PushBackVector(&po->_longopts, &doubleOpt);
  TRI_PushBackVector(&po->_items, &item);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a flag argument
////////////////////////////////////////////////////////////////////////////////

static int ParseFlagArg(char const* userarg, void* value) {
  TRI_ASSERT(value != nullptr);

  po_flag_t* flag = static_cast<po_flag_t*>(value);

  if (flag->_value != nullptr) {
    if (userarg == nullptr) {
      *flag->_value = true;
    } else if (TRI_CaseEqualString(userarg, "yes")) {
      *flag->_value = true;
    } else if (TRI_CaseEqualString(userarg, "no")) {
      *flag->_value = false;
    } else if (TRI_CaseEqualString(userarg, "true")) {
      *flag->_value = true;
    } else if (TRI_CaseEqualString(userarg, "false")) {
      *flag->_value = false;
    } else if (TRI_CaseEqualString(userarg, "1")) {
      *flag->_value = true;
    } else if (TRI_CaseEqualString(userarg, "0")) {
      *flag->_value = false;
    } else {
      return TRI_ERROR_BAD_PARAMETER;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a flag option
////////////////////////////////////////////////////////////////////////////////

static void CreateFlagOption(po_flag_t* desc, const void* input, void* output) {
  TRI_PO_item_t item;
  TRI_program_options_t* po;
  struct option flagOpt;

  po = (TRI_program_options_t*)(output);

  InitOptionStructure(&flagOpt, desc->base._name, (desc->_value == 0 ? 0 : 1),
                      0, (int)TRI_LengthVector(&po->_longopts));

  memset(&item, 0, sizeof(item));

  item._desc = &desc->base;
  item.parse = ParseFlagArg;

  TRI_PushBackVector(&po->_longopts, &flagOpt);
  TRI_PushBackVector(&po->_items, &item);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a 16-bit integer argument
////////////////////////////////////////////////////////////////////////////////

static int ParseInt16Arg(char const* userarg, void* value) {
  TRI_ASSERT(value != nullptr);

  po_int16_t* desc = static_cast<po_int16_t*>(value);
  int32_t tmp = TRI_Int32String(userarg);

  int res = TRI_errno();

  if (res == TRI_ERROR_NO_ERROR) {
    if (INT16_MIN <= tmp && tmp <= INT16_MAX) {
      *desc->_value = tmp;
    } else {
      res = TRI_ERROR_NUMERIC_OVERFLOW;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a 16-bit integer option
////////////////////////////////////////////////////////////////////////////////

static void CreateInt16Option(po_int16_t* desc, const void* input,
                              void* output) {
  TRI_PO_item_t item;
  TRI_program_options_t* po;
  struct option intOpt;

  po = (TRI_program_options_t*)(output);

  InitOptionStructure(&intOpt, desc->base._name, 1, 0,
                      (int)TRI_LengthVector(&po->_longopts));

  memset(&item, 0, sizeof(item));

  item._desc = &desc->base;
  item.parse = ParseInt16Arg;

  TRI_PushBackVector(&po->_longopts, &intOpt);
  TRI_PushBackVector(&po->_items, &item);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a 32-bit integer argument
////////////////////////////////////////////////////////////////////////////////

static int ParseInt32Arg(char const* userarg, void* value) {
  TRI_ASSERT(value != nullptr);

  po_int32_t* desc = static_cast<po_int32_t*>(value);
  int32_t tmp = TRI_Int32String(userarg);

  int res = TRI_errno();

  if (res == TRI_ERROR_NO_ERROR) {
    *desc->_value = tmp;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a 32-bit integer option
////////////////////////////////////////////////////////////////////////////////

static void CreateInt32Option(po_int32_t* desc, const void* input,
                              void* output) {
  TRI_PO_item_t item;
  TRI_program_options_t* po;
  struct option intOpt;

  po = (TRI_program_options_t*)(output);

  InitOptionStructure(&intOpt, desc->base._name, 1, 0,
                      (int)TRI_LengthVector(&po->_longopts));

  memset(&item, 0, sizeof(item));

  item._desc = &desc->base;
  item.parse = ParseInt32Arg;

  TRI_PushBackVector(&po->_longopts, &intOpt);
  TRI_PushBackVector(&po->_items, &item);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a 64-bit integer argument
////////////////////////////////////////////////////////////////////////////////

static int ParseInt64Arg(char const* userarg, void* value) {
  TRI_ASSERT(value != nullptr);

  po_int64_t* desc = static_cast<po_int64_t*>(value);
  int64_t tmp = TRI_Int64String(userarg);

  int res = TRI_errno();

  if (res == TRI_ERROR_NO_ERROR) {
    *desc->_value = tmp;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a 64-bit integer option
////////////////////////////////////////////////////////////////////////////////

static void CreateInt64Option(po_int64_t* desc, const void* input,
                              void* output) {
  TRI_PO_item_t item;
  TRI_program_options_t* po;
  struct option intOpt;

  po = (TRI_program_options_t*)(output);

  InitOptionStructure(&intOpt, desc->base._name, 1, 0,
                      (int)TRI_LengthVector(&po->_longopts));

  memset(&item, 0, sizeof(item));

  item._desc = &desc->base;
  item.parse = ParseInt64Arg;

  TRI_PushBackVector(&po->_longopts, &intOpt);
  TRI_PushBackVector(&po->_items, &item);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a section
////////////////////////////////////////////////////////////////////////////////

static void FreeSectionOption(TRI_PO_section_t* desc, const void* input,
                              void* output) {
  TRI_DestroyVectorPointer(&desc->_children);
  FreeOption(&desc->base, nullptr, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a section option
////////////////////////////////////////////////////////////////////////////////

static void CreateSectionOption(TRI_PO_section_t* section, const void* input,
                                void* output) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a string
////////////////////////////////////////////////////////////////////////////////

static int ParseStringArg(char const* userarg, void* value) {
  TRI_ASSERT(value != nullptr);
  TRI_ASSERT(userarg != nullptr);

  TRI_PO_string_t* desc = static_cast<TRI_PO_string_t*>(value);

  if (*desc->_value != nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, *desc->_value);
  }

  *desc->_value = TRI_DuplicateString(userarg);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a string option
////////////////////////////////////////////////////////////////////////////////

static void CreateStringOption(TRI_PO_string_t* desc, const void* input,
                               void* output) {
  TRI_PO_item_t item;
  TRI_program_options_t* po;
  struct option stringOpt;

  po = (TRI_program_options_t*)output;

  InitOptionStructure(&stringOpt, desc->base._name, 1, 0,
                      (int)TRI_LengthVector(&po->_longopts));

  memset(&item, 0, sizeof(item));

  item._desc = &desc->base;
  item.parse = ParseStringArg;

  TRI_PushBackVector(&po->_longopts, &stringOpt);
  TRI_PushBackVector(&po->_items, &item);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses an unsigned 16-bit integer argument
////////////////////////////////////////////////////////////////////////////////

static int ParseUInt16Arg(char const* userarg, void* value) {
  TRI_ASSERT(value != nullptr);

  po_uint16_t* desc = static_cast<po_uint16_t*>(value);
  uint32_t tmp = TRI_UInt32String(userarg);

  int res = TRI_errno();

  if (res == TRI_ERROR_NO_ERROR) {
    if (tmp <= UINT16_MAX) {
      *desc->_value = tmp;
    } else {
      res = TRI_ERROR_NUMERIC_OVERFLOW;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an unsigned 16-bit integer option
////////////////////////////////////////////////////////////////////////////////

static void CreateUInt16Option(po_uint16_t* desc, const void* input,
                               void* output) {
  TRI_PO_item_t item;
  TRI_program_options_t* po;
  struct option intOpt;

  po = (TRI_program_options_t*)(output);

  InitOptionStructure(&intOpt, desc->base._name, 1, 0,
                      (int)TRI_LengthVector(&po->_longopts));

  memset(&item, 0, sizeof(item));

  item._desc = &desc->base;
  item.parse = ParseUInt16Arg;

  TRI_PushBackVector(&po->_longopts, &intOpt);
  TRI_PushBackVector(&po->_items, &item);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a 32-bit integer argument
////////////////////////////////////////////////////////////////////////////////

static int ParseUInt32Arg(char const* userarg, void* value) {
  TRI_ASSERT(value != nullptr);

  po_uint32_t* desc = static_cast<po_uint32_t*>(value);
  uint32_t tmp = TRI_UInt32String(userarg);

  int res = TRI_errno();

  if (res == TRI_ERROR_NO_ERROR) {
    *desc->_value = tmp;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a 32-bit integer option
////////////////////////////////////////////////////////////////////////////////

static void CreateUInt32Option(po_uint32_t* desc, const void* input,
                               void* output) {
  TRI_PO_item_t item;
  TRI_program_options_t* po;
  struct option intOpt;

  po = (TRI_program_options_t*)(output);

  InitOptionStructure(&intOpt, desc->base._name, 1, 0,
                      (int)TRI_LengthVector(&po->_longopts));

  memset(&item, 0, sizeof(item));

  item._desc = &desc->base;
  item.parse = ParseUInt32Arg;

  TRI_PushBackVector(&po->_longopts, &intOpt);
  TRI_PushBackVector(&po->_items, &item);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a 64-bit integer argument
////////////////////////////////////////////////////////////////////////////////

static int ParseUInt64Arg(char const* userarg, void* value) {
  TRI_ASSERT(value != nullptr);

  po_uint64_t* desc = static_cast<po_uint64_t*>(value);
  uint64_t tmp = TRI_UInt64String(userarg);

  int res = TRI_errno();

  if (res == TRI_ERROR_NO_ERROR) {
    *desc->_value = tmp;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a 64-bit integer option
////////////////////////////////////////////////////////////////////////////////

static void CreateUInt64Option(po_uint64_t* desc, const void* input,
                               void* output) {
  TRI_PO_item_t item;
  TRI_program_options_t* po;
  struct option intOpt;

  po = (TRI_program_options_t*)(output);

  InitOptionStructure(&intOpt, desc->base._name, 1, 0,
                      (int)TRI_LengthVector(&po->_longopts));

  memset(&item, 0, sizeof(item));

  item._desc = &desc->base;
  item.parse = ParseUInt64Arg;

  TRI_PushBackVector(&po->_longopts, &intOpt);
  TRI_PushBackVector(&po->_items, &item);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a vector of strings
////////////////////////////////////////////////////////////////////////////////

static int ParseVectorStringArg(char const* userarg, void* value) {
  TRI_ASSERT(value != nullptr);
  TRI_ASSERT(userarg != nullptr);

  TRI_PO_vector_string_t* desc = static_cast<TRI_PO_vector_string_t*>(value);

  TRI_PushBackVectorString(desc->_value, TRI_DuplicateString(userarg));

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a vector string option
////////////////////////////////////////////////////////////////////////////////

static void CreateVectorStringOption(TRI_PO_vector_string_t* desc,
                                     const void* input, void* output) {
  TRI_PO_item_t item;
  TRI_program_options_t* po;
  struct option vectorOpt;

  po = (TRI_program_options_t*)(output);

  InitOptionStructure(&vectorOpt, desc->base._name, 1, 0,
                      (int)TRI_LengthVector(&po->_longopts));

  memset(&item, 0, sizeof(item));

  item._desc = &desc->base;
  item.parse = ParseVectorStringArg;

  TRI_PushBackVector(&po->_longopts, &vectorOpt);
  TRI_PushBackVector(&po->_items, &item);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a vector string option
////////////////////////////////////////////////////////////////////////////////

static void VisitProgramOptions(TRI_PO_desc_t* ptr,
                                po_visit_functions_t* functions,
                                const void* input, void* output) {
  TRI_PO_section_t* section;
  size_t n;
  size_t i;

  switch (ptr->_type) {
    case TRI_PO_DOUBLE:
      functions->visitDoubleNode((po_double_t*)ptr, input, output);
      break;

    case TRI_PO_FLAG:
      functions->visitFlagNode((po_flag_t*)ptr, input, output);
      break;

    case TRI_PO_INT16:
      functions->visitInt16Node((po_int16_t*)ptr, input, output);
      break;

    case TRI_PO_INT32:
      functions->visitInt32Node((po_int32_t*)ptr, input, output);
      break;

    case TRI_PO_INT64:
      functions->visitInt64Node((po_int64_t*)ptr, input, output);
      break;

    case TRI_PO_SECTION:
      section = (TRI_PO_section_t*)ptr;

      if (functions->visitSectionNodeBefore != nullptr) {
        functions->visitSectionNodeBefore(section, input, output);
      }

      n = section->_children._length;

      for (i = 0; i < n; ++i) {
        TRI_PO_desc_t* child =
            static_cast<TRI_PO_desc_t*>(section->_children._buffer[i]);

        VisitProgramOptions(child, functions, input, output);
      }

      if (functions->visitSectionNodeAfter != nullptr) {
        functions->visitSectionNodeAfter(section, input, output);
      }

      break;

    case TRI_PO_STRING:
      functions->visitStringNode((TRI_PO_string_t*)ptr, input, output);
      break;

    case TRI_PO_UINT16:
      functions->visitUInt16Node((po_uint16_t*)ptr, input, output);
      break;

    case TRI_PO_UINT32:
      functions->visitUInt32Node((po_uint32_t*)ptr, input, output);
      break;

    case TRI_PO_UINT64:
      functions->visitUInt64Node((po_uint64_t*)ptr, input, output);
      break;

    case TRI_PO_VECTOR_STRING:
      functions->visitVectorStringNode((TRI_PO_vector_string_t*)ptr, input,
                                       output);
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles an option entry
////////////////////////////////////////////////////////////////////////////////

static bool HandleOption(TRI_program_options_t* options,
                         char const* programName, char const* section,
                         char const* option, char const* value) {
  char* full;

  if (*section == '\0') {
    full = TRI_DuplicateString(option);
  } else {
    full = TRI_Concatenate3String(section, ".", option);
  }

  for (size_t i = 0; i < TRI_LengthVector(&options->_items); ++i) {
    TRI_PO_item_t* item =
        static_cast<TRI_PO_item_t*>(TRI_AtVector(&options->_items, i));

    if (TRI_EqualString(full, item->_desc->_name)) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, full);

      int res = item->parse(value, item->_desc);

      if (res != TRI_ERROR_NO_ERROR) {
        printParseError(options, programName, option);
        return false;
      }

      item->_used = true;
      return true;
    }
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, full);

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new program options description
////////////////////////////////////////////////////////////////////////////////

TRI_PO_section_t* TRI_CreatePODescription(char const* description) {
  TRI_PO_section_t* desc;

  desc = static_cast<TRI_PO_section_t*>(
      TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_PO_section_t), false));

  desc->base._type = TRI_PO_SECTION;
  desc->base._name = TRI_DuplicateString("Program Options");
  desc->base._desc =
      (description != nullptr) ? TRI_DuplicateString(description) : nullptr;

  TRI_InitVectorPointer(&desc->_children, TRI_CORE_MEM_ZONE);

  return desc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees all pointers for a program options description
////////////////////////////////////////////////////////////////////////////////

void TRI_FreePODescription(TRI_PO_section_t* desc) {
  po_visit_functions_t freeFunc;

  freeFunc.visitDoubleNode =
      (void (*)(po_double_t*, const void* input, void* output))FreeOption;
  freeFunc.visitFlagNode =
      (void (*)(po_flag_t*, const void* input, void* output))FreeOption;
  freeFunc.visitInt16Node =
      (void (*)(po_int16_t*, const void* input, void* output))FreeOption;
  freeFunc.visitInt32Node =
      (void (*)(po_int32_t*, const void* input, void* output))FreeOption;
  freeFunc.visitInt64Node =
      (void (*)(po_int64_t*, const void* input, void* output))FreeOption;
  freeFunc.visitSectionNodeBefore = nullptr;
  freeFunc.visitSectionNodeAfter = FreeSectionOption;
  freeFunc.visitStringNode =
      (void (*)(TRI_PO_string_t*, const void* input, void* output))FreeOption;
  freeFunc.visitUInt16Node =
      (void (*)(po_uint16_t*, const void* input, void* output))FreeOption;
  freeFunc.visitUInt32Node =
      (void (*)(po_uint32_t*, const void* input, void* output))FreeOption;
  freeFunc.visitUInt64Node =
      (void (*)(po_uint64_t*, const void* input, void* output))FreeOption;
  freeFunc.visitVectorStringNode = (void (
      *)(TRI_PO_vector_string_t*, const void* input, void* output))FreeOption;

  VisitProgramOptions(&desc->base, &freeFunc, nullptr, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the runtime structures which handles the user options
////////////////////////////////////////////////////////////////////////////////

TRI_program_options_t* TRI_CreateProgramOptions(TRI_PO_section_t* desc) {
  TRI_program_options_t* po;
  po_visit_functions_t optionBuilders;
  struct option nullOpt;

  po = (TRI_program_options_t*)TRI_Allocate(
      TRI_CORE_MEM_ZONE, sizeof(TRI_program_options_t), false);

  if (po == nullptr) {
    // this should never happen in CORE_MEM_ZONE
    return nullptr;
  }

  TRI_InitVector(&po->_longopts, TRI_CORE_MEM_ZONE, sizeof(struct option));
  TRI_InitVector(&po->_items, TRI_CORE_MEM_ZONE, sizeof(TRI_PO_item_t));

  optionBuilders.visitDoubleNode = CreateDoubleOption;
  optionBuilders.visitFlagNode = CreateFlagOption;
  optionBuilders.visitInt16Node = CreateInt16Option;
  optionBuilders.visitInt32Node = CreateInt32Option;
  optionBuilders.visitInt64Node = CreateInt64Option;
  optionBuilders.visitSectionNodeBefore = CreateSectionOption;
  optionBuilders.visitSectionNodeAfter = nullptr;
  optionBuilders.visitStringNode = CreateStringOption;
  optionBuilders.visitUInt16Node = CreateUInt16Option;
  optionBuilders.visitUInt32Node = CreateUInt32Option;
  optionBuilders.visitUInt64Node = CreateUInt64Option;
  optionBuilders.visitVectorStringNode = CreateVectorStringOption;

  VisitProgramOptions(&desc->base, &optionBuilders, nullptr, po);

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

void TRI_DestroyProgramOptions(TRI_program_options_t* options) {
  TRI_DestroyVector(&options->_longopts);
  TRI_DestroyVector(&options->_items);
  TRI_DestroyVectorString(&options->_arguments);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroyes alls allocated memory and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeProgramOptions(TRI_program_options_t* options) {
  TRI_DestroyProgramOptions(options);
  TRI_Free(TRI_CORE_MEM_ZONE, options);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a flag option
////////////////////////////////////////////////////////////////////////////////

void TRI_AddFlagPODescription(TRI_PO_section_t* desc, char const* name,
                              char shortName, char const* description,
                              bool* variable) {
  po_flag_t* res;

  TRI_ASSERT(name != nullptr);

  res = static_cast<po_flag_t*>(
      TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(po_flag_t), false));

  res->base._type = TRI_PO_FLAG;
  res->base._name = TRI_DuplicateString(name);
  res->base._short = shortName;
  res->base._desc =
      (description != nullptr) ? TRI_DuplicateString(description) : nullptr;

  res->_value = variable;

  TRI_PushBackVectorPointer(&desc->_children, res);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a string option
////////////////////////////////////////////////////////////////////////////////

void TRI_AddStringPODescription(TRI_PO_section_t* desc, char const* name,
                                char shortName, char const* description,
                                char** variable) {
  TRI_PO_string_t* res;

  TRI_ASSERT(variable != nullptr);
  TRI_ASSERT(name != nullptr);

  res = static_cast<TRI_PO_string_t*>(
      TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_PO_string_t), false));

  res->base._type = TRI_PO_STRING;
  res->base._name = TRI_DuplicateString(name);
  res->base._short = shortName;
  res->base._desc =
      (description != nullptr) ? TRI_DuplicateString(description) : nullptr;

  res->_value = variable;

  TRI_PushBackVectorPointer(&desc->_children, res);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a vector string option
////////////////////////////////////////////////////////////////////////////////

void TRI_AddVectorStringPODescription(TRI_PO_section_t* desc, char const* name,
                                      char shortName, char const* description,
                                      TRI_vector_string_t* variable) {
  TRI_PO_vector_string_t* res;

  TRI_ASSERT(variable != nullptr);
  TRI_ASSERT(name != nullptr);

  res = static_cast<TRI_PO_vector_string_t*>(
      TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_PO_vector_string_t), false));

  res->base._type = TRI_PO_VECTOR_STRING;
  res->base._name = TRI_DuplicateString(name);
  res->base._short = shortName;
  res->base._desc =
      (description != nullptr) ? TRI_DuplicateString(description) : nullptr;

  res->_value = variable;

  TRI_PushBackVectorPointer(&desc->_children, res);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a command line of arguments
////////////////////////////////////////////////////////////////////////////////

bool TRI_ParseArgumentsProgramOptions(TRI_program_options_t* options,
                                      char const* programName, int argc,
                                      char** argv) {
  extern char* optarg;
  extern int optind;
  extern int opterr;

  TRI_string_buffer_t buffer;
  TRI_PO_item_t* item = nullptr;
  size_t i;
  int idx;
  int maxIdx;

  // turn off error messages by getopt_long()
  opterr = 0;

  TRI_set_errno(TRI_ERROR_NO_ERROR);

  TRI_InitStringBuffer(&buffer, TRI_CORE_MEM_ZONE);
  // append an initial ':' so getopt_long() will return ':' if it encounters an
  // option with a missing value
  TRI_AppendCharStringBuffer(&buffer, ':');

  for (i = 0; i < TRI_LengthVector(&options->_items); ++i) {
    item = static_cast<TRI_PO_item_t*>(TRI_AtVector(&options->_items, i));

    if (item->_desc->_short != '\0') {
      TRI_AppendCharStringBuffer(&buffer, item->_desc->_short);

      if (item->_desc->_type == TRI_PO_FLAG) {
        po_flag_t* p = (po_flag_t*)item->_desc;

        if (p->_value != 0) {
          TRI_AppendCharStringBuffer(&buffer, ':');
        }
      } else {
        TRI_AppendCharStringBuffer(&buffer, ':');
      }
    }
  }

  char const* shortOptions;
  if (TRI_LengthStringBuffer(&buffer) == 0) {
    shortOptions = "";
  } else {
    shortOptions = TRI_BeginStringBuffer(&buffer);
  }

  optind = 1;
  maxIdx = (int)TRI_LengthVector(&options->_items);

  while (true) {
    int c = getopt_long(argc, argv, shortOptions,
                        (const struct option*)options->_longopts._buffer, &idx);
    char* t;

    if (c == -1) {
      for (i = optind; i < (size_t)argc; ++i) {
        TRI_PushBackVectorString(&options->_arguments, FillVariables(argv[i]));
      }

      break;
    }

    if (c < 256) {
      size_t ni = TRI_LengthVector(&options->_items);
      for (i = 0; i < ni; ++i) {
        item = static_cast<TRI_PO_item_t*>(TRI_AtVector(&options->_items, i));

        if (item->_desc->_short == c) {
          break;
        }
      }

      if (i == ni) {
        if (optind - 1 > 0 && optind - 1 < argc) {
          if ((char)c == ':') {
            // missing argument
            printParseError(options, programName, argv[optind - 1]);
          } else {
            printUnrecognizedOption(options, programName, argv[optind - 1]);
          }
        }
        TRI_set_errno(TRI_ERROR_ILLEGAL_OPTION);
        TRI_DestroyStringBuffer(&buffer);
        return false;
      }
    } else {
      c -= 256;

      if (c >= maxIdx) {
        if (optind - 1 > 0 && optind - 1 < argc) {
          printUnrecognizedOption(options, programName, argv[optind - 1]);
        }
        TRI_set_errno(TRI_ERROR_ILLEGAL_OPTION);
        TRI_DestroyStringBuffer(&buffer);
        return false;
      }

      item = static_cast<TRI_PO_item_t*>(TRI_AtVector(&options->_items, c));
    }

    // the opt.. are external variables
    item->_used = true;

    t = FillVariables(optarg);

    int res = item->parse(t, item->_desc);

    if (res != TRI_ERROR_NO_ERROR) {
      if (optind - 1 > 0 && optind - 1 < argc) {
        printParseError(options, programName, item->_desc->_name);
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, t);
      TRI_DestroyStringBuffer(&buffer);
      TRI_set_errno(TRI_ERROR_ILLEGAL_OPTION);
      return false;
    }

    if (t != nullptr) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, t);
    }
  }

  TRI_AnnihilateStringBuffer(&buffer);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a text file containing the configuration variables
////////////////////////////////////////////////////////////////////////////////

bool TRI_ParseFileProgramOptions(TRI_program_options_t* options,
                                 char const* programName,
                                 char const* filename) {
  FILE* f;
  bool ok;
  char* buffer;
  char* option;
  char* section;
  char* value;
  char* raw;
  char* tmpSection;
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

  if (f == nullptr) {
    TRI_set_errno(TRI_ERROR_FILE_NOT_FOUND);
    return false;
  }

  // used by routine getline()
  buffer = nullptr;

  // current section
  section = TRI_DuplicateString("");

  // regexp for comments
  regcomp(&re1, "^[ \t]*([#;].*)?$", REG_ICASE | REG_EXTENDED | REG_NOSUB);

  // regexp for section marker
  regcomp(&re2, "^[ \t]*\\[([-_a-z0-9]*)\\][ \t]*$", REG_ICASE | REG_EXTENDED);

  // regexp for option = value
  regcomp(&re3, "^[ \t]*([-_a-z0-9]*)[ \t]*=[ \t]*(.*[^ \t])[ \t]*$",
          REG_ICASE | REG_EXTENDED);

  // regexp for option =
  regcomp(&re4, "^[ \t]*([-_a-z0-9]*)[ \t]*=[ \t]*$", REG_ICASE | REG_EXTENDED);

  // regexp for section.option = value
  regcomp(&re5,
          "^[ \t]*([-_a-z0-9]*)\\.([-_a-z0-9]*)[ \t]*=[ \t]*(.*[^ \t])[ \t]*$",
          REG_ICASE | REG_EXTENDED);

  // regexp for section.option =
  regcomp(&re6, "^[ \t]*([-_a-z0-9]*)\\.([-_a-z0-9]*)[ \t]*=[ \t]*$",
          REG_ICASE | REG_EXTENDED);

  // corresponds to the argc variable of the main routine
  ok = true;

  while ((len = getline(&buffer, &tmp, f)) > 0) {
    int res;

    while (0 < len && buffer[len - 1] == '\n') {
      buffer[len - 1] = '\0';
      --len;
    }

    // check for comments
    res = regexec(&re1, buffer, 0, 0, 0);

    if (res == 0) {
      TRI_SystemFree(buffer);
      buffer = nullptr;
      continue;
    }

    // check for section marker
    res =
        regexec(&re2, buffer, sizeof(matches) / sizeof(matches[0]), matches, 0);

    if (res == 0) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, section);

      section = TRI_DuplicateString(buffer + matches[1].rm_so,
                                    matches[1].rm_eo - matches[1].rm_so);
      TRI_SystemFree(buffer);
      buffer = nullptr;

      continue;
    }

    // check for option = value
    res =
        regexec(&re3, buffer, sizeof(matches) / sizeof(matches[0]), matches, 0);

    if (res == 0) {
      option = TRI_DuplicateString(buffer + matches[1].rm_so,
                                   matches[1].rm_eo - matches[1].rm_so);
      raw = TRI_DuplicateString(buffer + matches[2].rm_so,
                                matches[2].rm_eo - matches[2].rm_so);
      value = FillVariables(raw);
      TRI_FreeString(TRI_CORE_MEM_ZONE, raw);

      TRI_SystemFree(buffer);
      buffer = nullptr;

      ok = HandleOption(options, programName, section, option, value);

      TRI_FreeString(TRI_CORE_MEM_ZONE, value);

      if (!ok) {
        if (*section != '\0') {
          printUnrecognizedOption(options, programName, section, option);
        } else {
          printUnrecognizedOption(options, programName, option);
        }
        TRI_set_errno(TRI_ERROR_ILLEGAL_OPTION);

        TRI_FreeString(TRI_CORE_MEM_ZONE, option);
        break;
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, option);
      continue;
    }

    // check for option =
    res =
        regexec(&re4, buffer, sizeof(matches) / sizeof(matches[0]), matches, 0);

    if (res == 0) {
      option = TRI_DuplicateString(buffer + matches[1].rm_so,
                                   matches[1].rm_eo - matches[1].rm_so);

      TRI_SystemFree(buffer);
      buffer = nullptr;

      ok = HandleOption(options, programName, section, option, "");

      if (!ok) {
        if (*section != '\0') {
          printUnrecognizedOption(options, programName, section, option);
        } else {
          printUnrecognizedOption(options, programName, option);
        }
        TRI_set_errno(TRI_ERROR_ILLEGAL_OPTION);

        TRI_FreeString(TRI_CORE_MEM_ZONE, option);
        break;
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, option);
      continue;
    }

    // check for option = value
    res =
        regexec(&re5, buffer, sizeof(matches) / sizeof(matches[0]), matches, 0);

    if (res == 0) {
      tmpSection = TRI_DuplicateString(buffer + matches[1].rm_so,
                                       matches[1].rm_eo - matches[1].rm_so);
      option = TRI_DuplicateString(buffer + matches[2].rm_so,
                                   matches[2].rm_eo - matches[2].rm_so);
      raw = TRI_DuplicateString(buffer + matches[3].rm_so,
                                matches[3].rm_eo - matches[3].rm_so);
      value = FillVariables(raw);
      TRI_FreeString(TRI_CORE_MEM_ZONE, raw);

      TRI_SystemFree(buffer);
      buffer = nullptr;

      ok = HandleOption(options, programName, tmpSection, option, value);

      TRI_FreeString(TRI_CORE_MEM_ZONE, value);

      if (!ok) {
        printUnrecognizedOption(options, programName, tmpSection, option);
        TRI_set_errno(TRI_ERROR_ILLEGAL_OPTION);

        TRI_FreeString(TRI_CORE_MEM_ZONE, tmpSection);
        TRI_FreeString(TRI_CORE_MEM_ZONE, option);
        break;
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, tmpSection);
      TRI_FreeString(TRI_CORE_MEM_ZONE, option);
      continue;
    }

    // check for option =
    res =
        regexec(&re6, buffer, sizeof(matches) / sizeof(matches[0]), matches, 0);

    if (res == 0) {
      tmpSection = TRI_DuplicateString(buffer + matches[1].rm_so,
                                       matches[1].rm_eo - matches[1].rm_so);
      option = TRI_DuplicateString(buffer + matches[2].rm_so,
                                   matches[2].rm_eo - matches[1].rm_so);

      TRI_SystemFree(buffer);
      buffer = nullptr;

      ok = HandleOption(options, programName, tmpSection, option, "");

      if (!ok) {
        printUnrecognizedOption(options, programName, tmpSection, option);
        TRI_set_errno(TRI_ERROR_ILLEGAL_OPTION);

        TRI_FreeString(TRI_CORE_MEM_ZONE, tmpSection);
        TRI_FreeString(TRI_CORE_MEM_ZONE, option);
        break;
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, tmpSection);
      TRI_FreeString(TRI_CORE_MEM_ZONE, option);
      continue;
    }

    printUnrecognizedOption(options, programName, buffer);
    TRI_set_errno(TRI_ERROR_ILLEGAL_OPTION);
    ok = false;

    TRI_SystemFree(buffer);
    buffer = nullptr;
    break;
  }

  if (buffer != nullptr) {
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
