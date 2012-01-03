////////////////////////////////////////////////////////////////////////////////
/// @brief program options
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
/// @author Esteban Lombeyda
/// @author Dr. Frank Celler
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_PROGRAM_OPTIONS_PROGRAM_OPTIONS_H
#define TRIAGENS_PROGRAM_OPTIONS_PROGRAM_OPTIONS_H 1

#include "BasicsC/common.h"

#include "BasicsC/vector.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ProgramOptions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief describes the supported description types
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_PO_DOUBLE,
  TRI_PO_FLAG,
  TRI_PO_INT16,
  TRI_PO_INT32,
  TRI_PO_INT64,
  TRI_PO_SECTION,
  TRI_PO_STRING,
  TRI_PO_UINT16,
  TRI_PO_UINT32,
  TRI_PO_UINT64,
  TRI_PO_VECTOR_STRING
} TRI_PO_description_types_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base structure for descriptions
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_PO_desc_s {
  TRI_PO_description_types_e _type;
  char * _name;
  char _short;
  char * _desc;
} TRI_PO_desc_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief description of attribute of type string
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_PO_string_s {
  TRI_PO_desc_t base;

  char ** _value;
} TRI_PO_string_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief description of attribute of type vector of strings
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_PO_vector_string_s {
  TRI_PO_desc_t base;

  TRI_vector_string_t * _value;
} TRI_PO_vector_string_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief program options description
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_PO_section_s {
  TRI_PO_desc_t base;

  struct TRI_PO_section_s * _parent;
  TRI_vector_pointer_t _children;
} TRI_PO_section_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief storage for the getopt data
///
/// The attribute "_used" is true, if the option occurred in the supplied
/// options. The attribute "_error" holds an error messages in case of a
/// parse error.
///
/// @see man 3 getopt
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_PO_item_s {
  bool _used;
  TRI_PO_desc_t * _desc;

  void (*parse) (char const * arg, void * value);
} TRI_PO_item_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief parsed program options
///
/// The attribute "_longopts" holds the description for "getopt" as array of
/// "struct options". The attribute "_items" holds the various descriptions of
/// type "TRI_PO_item_t".
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_program_options_s {
  TRI_vector_t _longopts;
  TRI_vector_t _items;
  TRI_vector_string_t _arguments;
} TRI_program_options_t;

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
/// @brief creates a new program options description
////////////////////////////////////////////////////////////////////////////////

TRI_PO_section_t* TRI_CreatePODescription (char const *description);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees all pointers for a program options description
////////////////////////////////////////////////////////////////////////////////

void TRI_FreePODescription (TRI_PO_section_t* desc);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the runtime structures which handles the user options
////////////////////////////////////////////////////////////////////////////////

TRI_program_options_t * TRI_CreateProgramOptions (TRI_PO_section_t * desc);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroyes alls allocated memory, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyProgramOptions (TRI_program_options_t * options);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroyes alls allocated memory and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeProgramOptions (TRI_program_options_t * options);

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
///
/// @param TRI_PO_section_t* desc contains the section
/// @param char * name is the name (identifier) of the option
/// @param char * description is the description of the  option
/// @param double * variable that will hold the result
////////////////////////////////////////////////////////////////////////////////

void TRI_AddDoublePODescription (TRI_PO_section_t * desc,
                                 char const * name,
                                 char shortName,
                                 char const * description,
                                 double *variable);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a flag option
///
/// @param TRI_PO_section_t* desc contains the section
/// @param char * name is the name (identifier) of the option
/// @param char * description is the description of the  option
/// @param bool * variable that will hold the result
////////////////////////////////////////////////////////////////////////////////

void TRI_AddFlagPODescription (TRI_PO_section_t * desc,
                               char const * name,
                               char shortName,
                               char const * description,
                               bool * variable);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a 16-bit integer option
///
/// @param TRI_PO_section_t* desc contains the section
/// @param char * name is the name (identifier) of the option
/// @param char * description is the description of the  option
/// @param int16 * variable that will hold the result
////////////////////////////////////////////////////////////////////////////////

void TRI_AddInt16PODescription (TRI_PO_section_t * desc,
                                char const * name,
                                char shortName,
                                char const * description,
                                int16_t * variable);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a 32-bit integer option
///
/// @param TRI_PO_section_t* desc contains the section
/// @param char * name is the name (identifier) of the option
/// @param char * description is the description of the  option
/// @param int32 * variable that will hold the result
////////////////////////////////////////////////////////////////////////////////

void TRI_AddInt32PODescription (TRI_PO_section_t * desc,
                                char const * name,
                                char shortName,
                                char const * description,
                                int32_t * variable);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a 64-bit integer option
///
/// @param TRI_PO_section_t* desc contains the section
/// @param char * name is the name (identifier) of the option
/// @param char * description is the description of the  option
/// @param int64 * variable that will hold the result
////////////////////////////////////////////////////////////////////////////////

void TRI_AddInt64PODescription (TRI_PO_section_t * desc,
                                char const * name,
                                char shortName,
                                char const * description,
                                int64_t * variable);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a string option
///
/// @param TRI_PO_section_t* desc contains the section
/// @param char * name is the name (identifier) of the option
/// @param char * description is the description of the  option
/// @param char ** variable that will hold the result
////////////////////////////////////////////////////////////////////////////////

void TRI_AddStringPODescription (TRI_PO_section_t * desc,
                                 char const * name,
                                 char shortName,
                                 char const * description,
                                 char ** variable);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an unsigned 16-bit integer option
///
/// @param TRI_PO_section_t* desc contains the section
/// @param char * name is the name (identifier) of the option
/// @param char * description is the description of the  option
/// @param uint16 * variable that will hold the result
////////////////////////////////////////////////////////////////////////////////

void TRI_AddUInt16PODescription (TRI_PO_section_t * desc,
                                 char const * name,
                                 char shortName,
                                 char const * description,
                                 uint16_t * variable);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an unsigned 32-bit integer option
///
/// @param TRI_PO_section_t* desc contains the section
/// @param char * name is the name (identifier) of the option
/// @param char * description is the description of the  option
/// @param uint32 * variable that will hold the result
////////////////////////////////////////////////////////////////////////////////

void TRI_AddUInt32PODescription (TRI_PO_section_t * desc,
                                 char const * name,
                                 char shortName,
                                 char const * description,
                                 uint32_t * variable);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an unsigned 64-bit integer option
///
/// @param TRI_PO_section_t* desc contains the section
/// @param char * name is the name (identifier) of the option
/// @param char * description is the description of the  option
/// @param uint64 * variable that will hold the result
////////////////////////////////////////////////////////////////////////////////

void TRI_AddUInt64PODescription (TRI_PO_section_t * desc,
                                 char const * name,
                                 char shortName,
                                 char const * description,
                                 uint64_t * variable);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a vector string option
///
/// @param TRI_PO_section_t* desc contains the section
/// @param char * name is the name (identifier) of the option
/// @param char * description is the description of the  option
/// @param TRI_vector_string_t * variable will hold the result
////////////////////////////////////////////////////////////////////////////////

void TRI_AddVectorStringPODescription (TRI_PO_section_t * desc,
                                       char const * name,
                                       char shortName,
                                       char const * description,
                                       TRI_vector_string_t * str_vector);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new section
///
/// @param TRI_PO_section_t* parent contains the section
/// @param TRI_PO_section_t* child contains the sub-section
////////////////////////////////////////////////////////////////////////////////

void TRI_AddOptionsPODescription (TRI_PO_section_t * parent, TRI_PO_section_t * child);

////////////////////////////////////////////////////////////////////////////////
/// @brief builds a usage string
////////////////////////////////////////////////////////////////////////////////

char * TRI_UsagePODescription (TRI_PO_section_t * desc);

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a command line of arguments according the description desc_ptr
////////////////////////////////////////////////////////////////////////////////

bool TRI_ParseArgumentsProgramOptions (TRI_program_options_t * options,
                                       int argc,
                                       char ** argv);

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a text file containing the configuration variables
////////////////////////////////////////////////////////////////////////////////

bool TRI_ParseFileProgramOptions (TRI_program_options_t * options,
                                  char const * programName,
                                  char const * filename);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if the option was given
////////////////////////////////////////////////////////////////////////////////

bool TRI_HasOptionProgramOptions (TRI_program_options_t const * options,
                                  char const * name);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
