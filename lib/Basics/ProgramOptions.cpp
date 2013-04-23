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
/// @author Dr. Frank Celler
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ProgramOptions.h"

#include <fstream>

#include "BasicsC/tri-strings.h"
#include "BasicsC/conversions.h"
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"

#include "ProgramOptions/program-options.h"

using namespace std;
using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ProgramOptions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a key in a map, returns empty if not found
////////////////////////////////////////////////////////////////////////////////

static string const& lookup (map<string, string> const& m, string const& key) {
  static string empty = "";

  map<string, string>::const_iterator i = m.find(key);

  if (i == m.end()) {
    return empty;
  }
  else {
    return i->second;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a key in a map, returns 0 if not found
////////////////////////////////////////////////////////////////////////////////

template<typename T>
static T lookup (map<string, T> const& m, string const& key) {
  typename map<string, T>::const_iterator i = m.find(key);

  if (i == m.end()) {
    return (T) 0;
  }
  else {
    return i->second;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a key in a map, throws an exception if not found
////////////////////////////////////////////////////////////////////////////////

template<typename T>
static T find (map<string, T> const& m, string const& key) {
  typename map<string, T>::const_iterator i = m.find(key);

  if (i == m.end()) {
    THROW_INTERNAL_ERROR("cannot find option '" + key + "'");
  }

  return i->second;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a double
////////////////////////////////////////////////////////////////////////////////

static void ExtractDouble (char const* ptr, double* value) {
  *value = TRI_DoubleString(ptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a double vector
////////////////////////////////////////////////////////////////////////////////

static void ExtractVectorDouble (TRI_vector_string_t* ptr, vector<double>* value) {
  size_t i;

  for (i = 0;  i < ptr->_length;  ++i) {
    char const* elem;
    double e;

    elem = ptr->_buffer[i];
    e = TRI_DoubleString(elem);

    value->push_back(e);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts an int32
////////////////////////////////////////////////////////////////////////////////

static void ExtractInt32 (char const* ptr, int32_t* value) {
  *value = TRI_Int32String(ptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a int32 vector
////////////////////////////////////////////////////////////////////////////////

static void ExtractVectorInt32 (TRI_vector_string_t* ptr, vector<int32_t>* value) {
  size_t i;

  for (i = 0;  i < ptr->_length;  ++i) {
    char const* elem;
    int32_t e;

    elem = ptr->_buffer[i];
    e = TRI_Int32String(elem);

    value->push_back(e);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts an int64
////////////////////////////////////////////////////////////////////////////////

static void ExtractInt64 (char const* ptr, int64_t* value) {
  *value = TRI_Int64String(ptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a int64 vector
////////////////////////////////////////////////////////////////////////////////

static void ExtractVectorInt64 (TRI_vector_string_t* ptr, vector<int64_t>* value) {
  size_t i;

  for (i = 0;  i < ptr->_length;  ++i) {
    char const* elem;
    int64_t e;

    elem = ptr->_buffer[i];
    e = TRI_Int64String(elem);

    value->push_back(e);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a string
////////////////////////////////////////////////////////////////////////////////

static void ExtractString (char const* ptr, string* value) {
  *value = ptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a string vector
////////////////////////////////////////////////////////////////////////////////

static void ExtractVectorString (TRI_vector_string_t* ptr, vector<string>* value) {
  size_t i;

  for (i = 0;  i < ptr->_length;  ++i) {
    char const* elem;

    elem = ptr->_buffer[i];

    value->push_back(elem);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts an uint32
////////////////////////////////////////////////////////////////////////////////

static void ExtractUInt32 (char const* ptr, uint32_t* value) {
  *value = TRI_UInt32String(ptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a uint32 vector
////////////////////////////////////////////////////////////////////////////////

static void ExtractVectorUInt32 (TRI_vector_string_t* ptr, vector<uint32_t>* value) {
  size_t i;

  for (i = 0;  i < ptr->_length;  ++i) {
    char const* elem;
    uint32_t e;

    elem = ptr->_buffer[i];
    e = TRI_UInt32String(elem);

    value->push_back(e);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts an uint64
////////////////////////////////////////////////////////////////////////////////

static void ExtractUInt64 (char const* ptr, uint64_t* value) {
  *value = TRI_UInt64String(ptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a uint64 vector
////////////////////////////////////////////////////////////////////////////////

static void ExtractVectorUInt64 (TRI_vector_string_t* ptr, vector<uint64_t>* value) {
  size_t i;

  for (i = 0;  i < ptr->_length;  ++i) {
    char const* elem;
    uint64_t e;

    elem = ptr->_buffer[i];
    e = TRI_UInt64String(elem);

    value->push_back(e);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              class ProgramOptions
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ProgramOptions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ProgramOptions::ProgramOptions ()
  : _valuesBool(),
    _valuesString(),
    _valuesVector(),
    _options(),
    _errorMessage(),
    _helpOptions(),
    _flags(),
    _seen(),
    _programName() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ProgramOptions::~ProgramOptions () {
  for (map<string, char**>::iterator i = _valuesString.begin();  i != _valuesString.end();  ++i) {
    char** ptr = i->second;

    if (*ptr != 0) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, *ptr);
    }

    TRI_Free(TRI_CORE_MEM_ZONE, ptr);
  }

  for (map<string, TRI_vector_string_t*>::iterator i = _valuesVector.begin();  i != _valuesVector.end();  ++i) {
    if ((*i).second != 0) {
      TRI_FreeVectorString(TRI_CORE_MEM_ZONE, (*i).second);
    }
  }

  for (map<string, bool*>::iterator i = _valuesBool.begin();  i != _valuesBool.end();  ++i) {
    if ((*i).second != 0) {
      TRI_Free(TRI_CORE_MEM_ZONE, (*i).second);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ProgramOptions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief parse command line
////////////////////////////////////////////////////////////////////////////////

bool ProgramOptions::parse (ProgramOptionsDescription const& description, int argc, char** argv) {
  TRI_PO_section_t* desc = setupDescription(description);
  set<string> const& ho = description.helpOptions();
  _helpOptions.insert(ho.begin(), ho.end());

  // set the program name from the argument vector
  _programName = argv[0];

  // parse the options
  TRI_program_options_t* options = TRI_CreateProgramOptions(desc);

  bool ok = TRI_ParseArgumentsProgramOptions(options, argc, argv);

  if (ok) {
    ok = extractValues(description, options, _seen);

    if (ok) {
      if (description._positionals != 0) {
        for (size_t i = 0;  i < options->_arguments._length;  ++i) {
          description._positionals->push_back(options->_arguments._buffer[i]);
        }
      }
    }
  }
  else {
    _errorMessage = TRI_last_error();
  }

  // freeValues(options);
  TRI_FreeProgramOptions(options);
  TRI_FreePODescription(desc);

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse configuration file
////////////////////////////////////////////////////////////////////////////////

bool ProgramOptions::parse (ProgramOptionsDescription const& description, string const& filename) {
  TRI_PO_section_t* desc = setupDescription(description);
  set<string> const& ho = description.helpOptions();
  _helpOptions.insert(ho.begin(), ho.end());

  TRI_program_options_t* options = TRI_CreateProgramOptions(desc);

  bool ok = TRI_ParseFileProgramOptions(options, _programName.c_str(), filename.c_str());

  if (ok) {
    ok = extractValues(description, options, _seen);
  }
  else {
    _errorMessage = TRI_last_error();
  }

  // freeValues(options);
  TRI_FreeProgramOptions(options);
  TRI_FreePODescription(desc);

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if option was given
////////////////////////////////////////////////////////////////////////////////

bool ProgramOptions::has (string const& key) const {
  return _flags.find(key) != _flags.end();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if help option was given
////////////////////////////////////////////////////////////////////////////////

set<string> ProgramOptions::needHelp (string const& key) const {
  set<string> result;

  if (_flags.find(key) != _flags.end()) {
    result.insert("--HELP--");
  }

  if (_flags.find("help-all") != _flags.end()) {
    result.insert("--HELP-ALL--");
  }

  for (set<string>::const_iterator i = _helpOptions.begin();  i != _helpOptions.end();  ++i) {
    string const& hkey = *i;

    if (_flags.find(hkey) != _flags.end()) {
      result.insert(hkey);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the last error
////////////////////////////////////////////////////////////////////////////////

string ProgramOptions::lastError () {
  return _errorMessage;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ProgramOptions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief generates description for the main section
////////////////////////////////////////////////////////////////////////////////

TRI_PO_section_t* ProgramOptions::setupDescription (ProgramOptionsDescription const& description) {

  // generate description file
  TRI_PO_section_t* desc = TRI_CreatePODescription("STANDARD");
  setupSubDescription(description, desc);

  // generate help options
  set<string> ho = description.helpOptions();

  for (set<string>::const_iterator i = ho.begin();  i != ho.end();  ++i) {
    string const& option = *i;

    TRI_AddFlagPODescription(desc, option.c_str(), 0, "more help", 0);
  }

  if (! ho.empty()) {
    TRI_AddFlagPODescription(desc, "help-all", 0, "show help for all options", 0);
  }

  return desc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates description for the sub-sections
////////////////////////////////////////////////////////////////////////////////

void ProgramOptions::setupSubDescription (ProgramOptionsDescription const& description, TRI_PO_section_t* desc) {
  for (vector<string>::const_iterator i = description._optionNames.begin();  i != description._optionNames.end();  ++i) {
    string const& name = *i;
    string const& help = lookup(description._helpTexts, name);
    string option = name;

    // check the short option
    map<string, string>::const_iterator k = description._long2short.find(option);
    char shortOption = '\0';

    if (k != description._long2short.end() && ! k->second.empty()) {
      shortOption = k->second[0];
    }

    // store long option name
    _options.push_back(option);

    // either a string or an vector
    map<string, ProgramOptionsDescription::option_type_e>::const_iterator j = description._optionTypes.find(name);

    if (j != description._optionTypes.end()) {
      ProgramOptionsDescription::option_type_e type = j->second;
      char** ptr;
      bool* btr;
      TRI_vector_string_t* wtr;

      switch (type) {
        case ProgramOptionsDescription::OPTION_TYPE_FLAG:
          TRI_AddFlagPODescription(desc, option.c_str(), shortOption, help.c_str(), 0);
          break;

        case ProgramOptionsDescription::OPTION_TYPE_BOOL:
          btr = _valuesBool[option];

          if (btr == 0) {
            btr = _valuesBool[option] = (bool*) TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(bool), true);
          }

          TRI_AddFlagPODescription(desc, option.c_str(), shortOption, help.c_str(), btr);
          break;

        case ProgramOptionsDescription::OPTION_TYPE_DOUBLE:
        case ProgramOptionsDescription::OPTION_TYPE_INT32:
        case ProgramOptionsDescription::OPTION_TYPE_INT64:
        case ProgramOptionsDescription::OPTION_TYPE_STRING:
        case ProgramOptionsDescription::OPTION_TYPE_UINT32:
        case ProgramOptionsDescription::OPTION_TYPE_UINT64:
        case ProgramOptionsDescription::OPTION_TYPE_TIME:
          ptr = _valuesString[option];

          if (ptr == 0) {
            ptr = _valuesString[option] = (char**) TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(char*), true);
          }

          TRI_AddStringPODescription(desc, option.c_str(), shortOption, help.c_str(), ptr);
          break;

        case ProgramOptionsDescription::OPTION_TYPE_VECTOR_DOUBLE:
        case ProgramOptionsDescription::OPTION_TYPE_VECTOR_INT32:
        case ProgramOptionsDescription::OPTION_TYPE_VECTOR_INT64:
        case ProgramOptionsDescription::OPTION_TYPE_VECTOR_STRING:
        case ProgramOptionsDescription::OPTION_TYPE_VECTOR_UINT32:
        case ProgramOptionsDescription::OPTION_TYPE_VECTOR_UINT64:
          wtr = _valuesVector[option];

          if (wtr == 0) {
            wtr = _valuesVector[option] = (TRI_vector_string_t*) TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_vector_string_t), false);
            TRI_InitVectorString(wtr, TRI_CORE_MEM_ZONE);
          }

          TRI_AddVectorStringPODescription(desc, option.c_str(), shortOption, help.c_str(), wtr);
          break;

        default:
          THROW_INTERNAL_ERROR("unknown option type");
      }
    }
  }

  // add the visible children
  for (vector<ProgramOptionsDescription>::const_iterator i = description._subDescriptions.begin();
       i != description._subDescriptions.end();
       ++i) {
    setupSubDescription(*i, desc);
  }

  // add the invisible children
  for (vector<ProgramOptionsDescription>::const_iterator i = description._hiddenSubDescriptions.begin();
       i != description._hiddenSubDescriptions.end();
       ++i) {
    setupSubDescription(*i, desc);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the parsed options
////////////////////////////////////////////////////////////////////////////////

bool ProgramOptions::extractValues (ProgramOptionsDescription const& description, TRI_program_options_t* options, set<string> seen) {
  size_t i;
  char** ptr;
  bool* btr;
  bool* b;
  TRI_vector_string_t* wtr;

  for (i = 0;  i < options->_items._length;  ++i) {
    TRI_PO_item_t * item;

    item = (TRI_PO_item_t*) TRI_AtVector(&options->_items, i);

    if (item->_used) {
      string name = item->_desc->_name;

      if (seen.find(name) != seen.end()) {
        continue;
      }

      _flags.insert(name);
      _seen.insert(name);

      if (item->_desc->_type == TRI_PO_FLAG) {
        btr = _valuesBool[name];

        if (btr != 0) {
          ProgramOptionsDescription::option_type_e type = lookup(description._optionTypes, name);

          switch (type) {
            case ProgramOptionsDescription::OPTION_TYPE_BOOL:
              b = lookup(description._boolOptions, name);

              if (b != 0) {
                *b = *btr;
              }

              break;

            default:
              break;
          }
        }
      }
      else if (item->_desc->_type == TRI_PO_STRING) {
        ptr = _valuesString[name];

        if (ptr != 0) {
          ProgramOptionsDescription::option_type_e type = lookup(description._optionTypes, name);

          switch (type) {
            case ProgramOptionsDescription::OPTION_TYPE_DOUBLE:
              ExtractDouble(*ptr, lookup(description._doubleOptions, name));
              break;

            case ProgramOptionsDescription::OPTION_TYPE_INT32:
              ExtractInt32(*ptr, lookup(description._int32Options, name));
              break;

            case ProgramOptionsDescription::OPTION_TYPE_INT64:
              ExtractInt64(*ptr, lookup(description._int64Options, name));
              break;

            case ProgramOptionsDescription::OPTION_TYPE_STRING:
              ExtractString(*ptr, lookup(description._stringOptions, name));
              break;

            case ProgramOptionsDescription::OPTION_TYPE_UINT32:
              ExtractUInt32(*ptr, lookup(description._uint32Options, name));
              break;

            case ProgramOptionsDescription::OPTION_TYPE_UINT64:
              ExtractUInt64(*ptr, lookup(description._uint64Options, name));
              break;

            case ProgramOptionsDescription::OPTION_TYPE_TIME:
#if __WORDSIZE == 32
              ExtractInt32(*ptr, (int32_t*) lookup(description._timeOptions, name));
#endif
              break;

            default:
              break;
          }
        }
        else {
          _errorMessage = "unknown option '" + name + "'";
          return false;
        }
      }
      else if (item->_desc->_type == TRI_PO_VECTOR_STRING) {
        wtr = _valuesVector[name];

        if (wtr != 0) {
          ProgramOptionsDescription::option_type_e type = lookup(description._optionTypes, name);

          switch (type) {
            case ProgramOptionsDescription::OPTION_TYPE_VECTOR_DOUBLE:
              ExtractVectorDouble(wtr, lookup(description._vectorDoubleOptions, name));
              break;

            case ProgramOptionsDescription::OPTION_TYPE_VECTOR_INT32:
              ExtractVectorInt32(wtr, lookup(description._vectorInt32Options, name));
              break;

            case ProgramOptionsDescription::OPTION_TYPE_VECTOR_INT64:
              ExtractVectorInt64(wtr, lookup(description._vectorInt64Options, name));
              break;

            case ProgramOptionsDescription::OPTION_TYPE_VECTOR_STRING:
              ExtractVectorString(wtr, lookup(description._vectorStringOptions, name));
              break;

            case ProgramOptionsDescription::OPTION_TYPE_VECTOR_UINT32:
              ExtractVectorUInt32(wtr, lookup(description._vectorUInt32Options, name));
              break;

            case ProgramOptionsDescription::OPTION_TYPE_VECTOR_UINT64:
              ExtractVectorUInt64(wtr, lookup(description._vectorUInt64Options, name));
              break;

            default:
              break;
          }
        }
        else {
          _errorMessage = "unknown option '" + name + "'";
          return false;
        }
      }

      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        return false;
      }
    }
  }

  for (vector<ProgramOptionsDescription>::const_iterator i = description._subDescriptions.begin();
       i != description._subDescriptions.end();
       ++i) {
    if (! extractValues(*i, options, seen)) {
      return false;
    }
  }

  for (vector<ProgramOptionsDescription>::const_iterator i = description._hiddenSubDescriptions.begin();
       i != description._hiddenSubDescriptions.end();
       ++i) {
    if (! extractValues(*i, options, seen)) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
