////////////////////////////////////////////////////////////////////////////////
/// @brief program options description
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
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ProgramOptionsDescription.h"

#include "BasicsC/terminal-utils.h"

#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"

#include <iterator>

using namespace std;
using namespace triagens::basics;

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

ProgramOptionsDescription::ProgramOptionsDescription ()
  : _name(""),
    _helpOptions(),
    _subDescriptions(),
    _hiddenSubDescriptions(),
    _optionNames(),
    _optionTypes(),
    _long2short(),
    _helpTexts(),
    _stringOptions(),
    _vectorStringOptions(),
    _int32Options(),
    _vectorInt32Options(),
    _int64Options(),
    _vectorInt64Options(),
    _uint32Options(),
    _vectorUInt32Options(),
    _uint64Options(),
    _vectorUInt64Options(),
    _doubleOptions(),
    _vectorDoubleOptions(),
    _boolOptions(),
#if __WORDSIZE == 32
    _timeOptions(),
#endif
    _positionals(0) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription::ProgramOptionsDescription (string const& name)
  : _name(""),
    _helpOptions(),
    _subDescriptions(),
    _hiddenSubDescriptions(),
    _optionNames(),
    _optionTypes(),
    _long2short(),
    _helpTexts(),
    _stringOptions(),
    _vectorStringOptions(),
    _int32Options(),
    _vectorInt32Options(),
    _int64Options(),
    _vectorInt64Options(),
    _uint32Options(),
    _vectorUInt32Options(),
    _uint64Options(),
    _vectorUInt64Options(),
    _doubleOptions(),
    _vectorDoubleOptions(),
    _boolOptions(),
#if __WORDSIZE == 32
    _timeOptions(),
#endif
    _positionals(0) {
  setName(name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy constructor
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription::ProgramOptionsDescription (ProgramOptionsDescription const& that)
  : _name(that._name),
    _helpOptions(that._helpOptions),
    _subDescriptions(that._subDescriptions),
    _hiddenSubDescriptions(that._hiddenSubDescriptions),
    _optionNames(that._optionNames),
    _optionTypes(that._optionTypes),
    _long2short(that._long2short),
    _helpTexts(that._helpTexts),
    _stringOptions(that._stringOptions),
    _vectorStringOptions(that._vectorStringOptions),
    _int32Options(that._int32Options),
    _vectorInt32Options(that._vectorInt32Options),
    _int64Options(that._int64Options),
    _vectorInt64Options(that._vectorInt64Options),
    _uint32Options(that._uint32Options),
    _vectorUInt32Options(that._vectorUInt32Options),
    _uint64Options(that._uint64Options),
    _vectorUInt64Options(that._vectorUInt64Options),
    _doubleOptions(that._doubleOptions),
    _vectorDoubleOptions(that._vectorDoubleOptions),
    _boolOptions(that._boolOptions),
#if __WORDSIZE == 32
    _timeOptions(that._timeOptions),
#endif
    _positionals(that._positionals) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief assignment constructor
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator= (ProgramOptionsDescription const& that) {
  if (this != &that) {
    _name = that._name;
    _helpOptions = that._helpOptions;
    _subDescriptions = that._subDescriptions;
    _hiddenSubDescriptions = that._hiddenSubDescriptions;
    _optionNames = that._optionNames;
    _optionTypes = that._optionTypes;
    _long2short = that._long2short;
    _helpTexts = that._helpTexts;
    _stringOptions = that._stringOptions;
    _vectorStringOptions = that._vectorStringOptions;
    _int32Options = that._int32Options;
    _vectorInt32Options = that._vectorInt32Options;
    _int64Options = that._int64Options;
    _vectorInt64Options = that._vectorInt64Options;
    _uint32Options = that._uint32Options;
    _vectorUInt32Options = that._vectorUInt32Options;
    _uint64Options = that._uint64Options;
    _vectorUInt64Options = that._vectorUInt64Options;
    _doubleOptions = that._doubleOptions;
    _vectorDoubleOptions = that._vectorDoubleOptions;
    _boolOptions = that._boolOptions;
#if __WORDSIZE == 32
    _timeOptions = that._timeOptions;
#endif
    _positionals = that._positionals;
  }

  return *this;
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

void ProgramOptionsDescription::setName (string const& name) {
  vector<string> n = StringUtils::split(name, ':');

  if (! n.empty()) {
    _name = n[0];

    for (vector<string>::iterator i = n.begin() + 1;  i != n.end();  ++i) {
      _helpOptions.insert(*i);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new hidden section
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator() (ProgramOptionsDescription& sub, bool hidden) {
  if (hidden) {
    _hiddenSubDescriptions.push_back(sub);
  }
  else {
    _subDescriptions.push_back(sub);
  }

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new flag
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator() (string const& full, string const& text) {
  string name = check(full);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_FLAG;
  _helpTexts[name] = text;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a string argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator() (string const& full, string* value, string const& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_STRING;
  _stringOptions[name] = value;

  if (value->empty()) {
    _helpTexts[name] = text;
  }
  else {
    _helpTexts[name] = text + " (default: \"" + *value + "\")";
  }

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a string vector argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator() (string const& full, vector<string>* value, string const& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_VECTOR_STRING;
  _vectorStringOptions[name] = value;
  _helpTexts[name] = text;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an int32_t argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator() (string const& full, int32_t* value, string const& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_INT32;
  _int32Options[name] = value;
  _helpTexts[name] = text + " (default: " + StringUtils::itoa(*value) + ")";

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an int32_t vector argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator() (string const& full, vector<int32_t>* value, string const& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_VECTOR_INT32;
  _vectorInt32Options[name] = value;
  _helpTexts[name] = text;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an int64_t argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator() (string const& full, int64_t* value, string const& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_INT64;
  _int64Options[name] = value;
  _helpTexts[name] = text + " (default: " + StringUtils::itoa(*value) + ")";

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an int64_t vector argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator() (string const& full, vector<int64_t>* value, string const& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_VECTOR_INT64;
  _vectorInt64Options[name] = value;
  _helpTexts[name] = text;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an uint32_t argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator() (string const& full, uint32_t* value, string const& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_UINT32;
  _uint32Options[name] = value;
  _helpTexts[name] = text + " (default: " + StringUtils::itoa(*value) + ")";

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an uint32_t vector argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator() (string const& full, vector<uint32_t>* value, string const& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_VECTOR_UINT32;
  _vectorUInt32Options[name] = value;
  _helpTexts[name] = text;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an uint64_t argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator() (string const& full, uint64_t* value, string const& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_UINT64;
  _uint64Options[name] = value;
  _helpTexts[name] = text + " (default: " + StringUtils::itoa(*value) + ")";

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an uint64_t vector argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator() (string const& full, vector<uint64_t>* value, string const& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_VECTOR_UINT64;
  _vectorUInt64Options[name] = value;
  _helpTexts[name] = text;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a double argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator() (string const& full, double* value, string const& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_DOUBLE;
  _doubleOptions[name] = value;
  _helpTexts[name] = text + " (default: " + StringUtils::ftoa(*value) + ")";

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a double vector argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator() (string const& full, vector<double>* value, string const& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_VECTOR_DOUBLE;
  _vectorDoubleOptions[name] = value;
  _helpTexts[name] = text;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a boolean argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator() (string const& full, bool* value, string const& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_BOOL;
  _boolOptions[name] = value;
  _helpTexts[name] = text + " (default: " + (*value ? "true" : "false") + ")";

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a time_t argument
////////////////////////////////////////////////////////////////////////////////

#if __WORDSIZE == 32

ProgramOptionsDescription& ProgramOptionsDescription::operator() (string const& full, time_t* value, string const& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_TIME;
  _timeOptions[name] = value;
  _helpTexts[name] = text + " (default: " + StringUtils::itoa((int64_t)*value) + ")";

  return *this;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief adds positional arguments
////////////////////////////////////////////////////////////////////////////////

void ProgramOptionsDescription::arguments (vector<string>* value) {
  if (value == 0) {
    THROW_INTERNAL_ERROR("value is 0");
  }

  if (_positionals != 0) {
    THROW_INTERNAL_ERROR("_positional arguments are already defined");
  }

  _positionals = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the usage message
////////////////////////////////////////////////////////////////////////////////

string ProgramOptionsDescription::usage () const {
  set<string> help;
  return usage(help);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the usage message
////////////////////////////////////////////////////////////////////////////////

string ProgramOptionsDescription::usage (set<string> const& help, bool addHelpOptions) const {
  string desc;

  bool helpAll = help.find("--HELP-ALL--") != help.end();
  bool helpStandard = help.find("--HELP--") != help.end();

  // extract help-able sub-descriptions
  vector<ProgramOptionsDescription> subDescriptions;

  for (vector<ProgramOptionsDescription>::const_iterator i = _subDescriptions.begin();  i != _subDescriptions.end();  ++i) {
    ProgramOptionsDescription pod = *i;
    set<string> ho = pod._helpOptions;

    if (ho.empty() || helpAll) {
      subDescriptions.push_back(pod);
    }
    else {
      set<string> is;
      set_intersection(ho.begin(), ho.end(), help.begin(), help.end(), std::inserter(is, is.end()));

      if (! is.empty()) {
        subDescriptions.push_back(pod);
      }
    }
  }

  // write help only if help options match
  if (! _helpOptions.empty() || helpStandard || helpAll) {

    // produce a head line
    if (! _name.empty() && ! (subDescriptions.empty() && _optionNames.empty())) {
      if (_helpOptions.empty()) {
        desc = _name + ":\n";
      }
      else {
        desc = "Extended " + _name + ":\n";
      }
    }

    // collect the maximal width of the option names
    size_t oWidth = 0;

    map<string, string> names;

    for (vector<string>::const_iterator i = _optionNames.begin();  i != _optionNames.end();  ++i) {
      string const& option = *i;
      option_type_e type = _optionTypes.find(option)->second;

      string name;

      switch (type) {
        case OPTION_TYPE_FLAG:
          name = option;
          break;

        case OPTION_TYPE_STRING:
          name = option + " <string>";
          break;

        case OPTION_TYPE_VECTOR_STRING:
          name = option + " <string>";
          break;

        case OPTION_TYPE_INT32:
          name = option + " <int32>";
          break;

        case OPTION_TYPE_VECTOR_INT64:
          name = option + " <int64>";
          break;

        case OPTION_TYPE_INT64:
          name = option + " <int64>";
          break;

        case OPTION_TYPE_VECTOR_INT32:
          name = option + " <int32>";
          break;

        case OPTION_TYPE_UINT32:
          name = option + " <uint32>";
          break;

        case OPTION_TYPE_VECTOR_UINT32:
          name = option + " <uint32>";
          break;

        case OPTION_TYPE_UINT64:
          name = option + " <uint64>";
          break;

        case OPTION_TYPE_VECTOR_UINT64:
          name = option + " <uint64>";
          break;

        case OPTION_TYPE_DOUBLE:
          name = option + " <double>";
          break;

        case OPTION_TYPE_VECTOR_DOUBLE:
          name = option + " <double>";
          break;

        case OPTION_TYPE_TIME:
          name = option + " <time>";
          break;

        case OPTION_TYPE_BOOL:
          name = option + " <bool>";
          break;
      }

      names[option] = name;

      if (name.size() > oWidth) {
        oWidth = name.size();
      }
    }

    // construct the parameters
    size_t tWidth = TRI_ColumnsWidth();

    if (tWidth < 40) {
      tWidth = 40;
    }

    size_t sWidth = 8;
    size_t dWidth = (oWidth + sWidth < tWidth) ? (tWidth - oWidth - sWidth) : (tWidth / 2);

    vector<string> on = _optionNames;
    sort(on.begin(), on.end());

    for (vector<string>::iterator i = on.begin();  i != on.end();  ++i) {
      string const& option = *i;
      string const& name = names[option];
      string const& text = _helpTexts.find(option)->second;

      if (text.size() <= dWidth) {
        desc += "  --" + StringUtils::rFill(name, oWidth) + "    " + text + "\n";
      }
      else {
        vector<string> wrap = StringUtils::wrap(text, dWidth, " ,");

        string sep = "  --" + StringUtils::rFill(name, oWidth) + "    ";

        for (vector<string>::iterator j = wrap.begin();  j != wrap.end();  ++j) {
          desc += sep + *j + "\n";
          sep = string(oWidth + sWidth, ' ');
        }
      }
    }
  }

  // check for sub-descriptions
  string sep = desc.empty() ? "" : "\n";

  for (vector<ProgramOptionsDescription>::iterator i = subDescriptions.begin();  i != subDescriptions.end();  ++i) {
    string u = i->usage(help, false);

    if (! u.empty()) {
      desc += sep + u;
      sep = "\n";
    }
  }

  // add help options
  if (addHelpOptions) {
    set<string> ho = helpOptions();
    set<string> is;
    set_difference(ho.begin(), ho.end(), help.begin(), help.end(), inserter(is, is.end()));


    if (! is.empty()) {
      desc += sep + "For more information use: " + StringUtils::join(is, ", ");
    }
  }

  return desc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a list of help options
////////////////////////////////////////////////////////////////////////////////

set<string> ProgramOptionsDescription::helpOptions () const {
  set<string> options = _helpOptions;

  for (vector<ProgramOptionsDescription>::const_iterator i = _subDescriptions.begin();  i != _subDescriptions.end();  ++i) {
    set<string> o = i->helpOptions();

    options.insert(o.begin(), o.end());
  }

  return options;
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
/// @brief checks if the name is an option, defines short/long mapping
////////////////////////////////////////////////////////////////////////////////

string ProgramOptionsDescription::check (string const& name) {
  vector<string> s = StringUtils::split(name, ',');

  if (name.empty()) {
    THROW_INTERNAL_ERROR("option name is empty");
  }

  if (s.size() > 2) {
    THROW_INTERNAL_ERROR("option '" + name + "' should be <long-option>,<short-option> or <long-option>");
  }

  string longOption = s[0];

  if (_optionTypes.find(longOption) != _optionTypes.end()) {
    THROW_INTERNAL_ERROR("option '" + longOption + "' is already defined");
  }

  if (s.size() > 1) {
    _long2short[longOption] = s[1];
  }

  return longOption;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the name is an option, defines short/long mapping
////////////////////////////////////////////////////////////////////////////////

string ProgramOptionsDescription::check (string const& name, void* value) {
  if (value == 0) {
    THROW_INTERNAL_ERROR("value is 0");
  }

  return check(name);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
