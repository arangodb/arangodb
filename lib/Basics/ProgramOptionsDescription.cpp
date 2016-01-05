////////////////////////////////////////////////////////////////////////////////
/// @brief program options description
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ProgramOptionsDescription.h"
#include "Basics/Exceptions.h"
#include "Basics/json.h"
#include "Basics/StringUtils.h"
#include "Basics/terminal-utils.h"

#include <iterator>

using namespace std;
using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription::ProgramOptionsDescription()
    : _name(""),
      _helpOptions(),
      _subDescriptions(),
      _hiddenSubDescriptions(),
      _optionNames(),
      _optionTypes(),
      _long2short(),
      _helpTexts(),
      _defaultTexts(),
      _currentTexts(),
      _values(),
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
      _positionals(nullptr) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription::ProgramOptionsDescription(const string& name)
    : ProgramOptionsDescription() {
  setName(name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy constructor
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription::ProgramOptionsDescription(
    ProgramOptionsDescription const& that)
    : _name(that._name),
      _helpOptions(that._helpOptions),
      _subDescriptions(that._subDescriptions),
      _hiddenSubDescriptions(that._hiddenSubDescriptions),
      _optionNames(that._optionNames),
      _optionTypes(that._optionTypes),
      _long2short(that._long2short),
      _helpTexts(that._helpTexts),
      _defaultTexts(that._defaultTexts),
      _currentTexts(that._currentTexts),
      _values(that._values),
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

ProgramOptionsDescription& ProgramOptionsDescription::operator=(
    const ProgramOptionsDescription& that) {
  if (this != &that) {
    _name = that._name;
    _helpOptions = that._helpOptions;
    _subDescriptions = that._subDescriptions;
    _hiddenSubDescriptions = that._hiddenSubDescriptions;
    _optionNames = that._optionNames;
    _optionTypes = that._optionTypes;
    _long2short = that._long2short;
    _helpTexts = that._helpTexts;
    _defaultTexts = that._defaultTexts;
    _currentTexts = that._currentTexts;
    _values = that._values;
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

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

void ProgramOptionsDescription::setName(const string& name) {
  vector<string> n = StringUtils::split(name, ':');

  if (!n.empty()) {
    _name = n[0];

    for (vector<string>::iterator i = n.begin() + 1; i != n.end(); ++i) {
      _helpOptions.insert(*i);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new hidden section
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator()(
    ProgramOptionsDescription& sub, bool hidden) {
  if (hidden) {
    _hiddenSubDescriptions.emplace_back(sub);
  } else {
    _subDescriptions.emplace_back(sub);
  }

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new flag
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator()(
    const string& full, const string& text) {
  string name = check(full);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_FLAG;
  _helpTexts[name] = text;
  _defaultTexts[name] = "";

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a string argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator()(
    const string& full, string* value, const string& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_STRING;
  _stringOptions[name] = value;
  _helpTexts[name] = text;
  _defaultTexts[name] = (value->empty()) ? "" : ("\"" + *value + "\"");
  _currentTexts[name] = [](void* p) -> string {
    return ((string*)p)->empty() ? "" : "\"" + (*(string*)p) + "\"";
  };
  _values[name] = (void*)value;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a string vector argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator()(
    const string& full, vector<string>* value, const string& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_VECTOR_STRING;
  _vectorStringOptions[name] = value;
  _helpTexts[name] = text;
  _defaultTexts[name] = value->empty() ? "" : StringUtils::join(*value, ", ");
  _currentTexts[name] = [](void* p) -> string {
    return ((vector<string>*)p)->empty()
               ? ""
               : "\"" + StringUtils::join(*(vector<string>*)p, " ,") + "\"";
  };
  _values[name] = (void*)value;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an int32_t argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator()(
    const string& full, int32_t* value, const string& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_INT32;
  _int32Options[name] = value;
  _helpTexts[name] = text;
  _defaultTexts[name] = StringUtils::itoa(*value);
  _currentTexts[name] = [](void* p) -> string {
    return p == nullptr ? "" : StringUtils::itoa(*(int32_t*)p);
  };
  _values[name] = (void*)value;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an int32_t vector argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator()(
    const string& full, vector<int32_t>* value, const string& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_VECTOR_INT32;
  _vectorInt32Options[name] = value;
  _helpTexts[name] = text;
  _defaultTexts[name] = "";

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an int64_t argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator()(
    const string& full, int64_t* value, const string& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_INT64;
  _int64Options[name] = value;
  _helpTexts[name] = text;
  _defaultTexts[name] = StringUtils::itoa(*value);
  _currentTexts[name] = [](void* p) -> string {
    return p == nullptr ? "" : StringUtils::itoa(*(int64_t*)p);
  };
  _values[name] = (void*)value;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an int64_t vector argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator()(
    const string& full, vector<int64_t>* value, const string& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_VECTOR_INT64;
  _vectorInt64Options[name] = value;
  _helpTexts[name] = text;
  _defaultTexts[name] = "";

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an uint32_t argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator()(
    const string& full, uint32_t* value, const string& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_UINT32;
  _uint32Options[name] = value;
  _helpTexts[name] = text;
  _defaultTexts[name] = StringUtils::itoa(*value);
  _currentTexts[name] = [](void* p) -> string {
    return p == nullptr ? "" : StringUtils::itoa(*(uint32_t*)p);
  };
  _values[name] = (void*)value;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an uint32_t vector argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator()(
    const string& full, vector<uint32_t>* value, const string& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_VECTOR_UINT32;
  _vectorUInt32Options[name] = value;
  _helpTexts[name] = text;
  _defaultTexts[name] = "";

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an uint64_t argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator()(
    const string& full, uint64_t* value, const string& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_UINT64;
  _uint64Options[name] = value;
  _helpTexts[name] = text;
  _defaultTexts[name] = StringUtils::itoa(*value);
  _currentTexts[name] = [](void* p) -> string {
    return p == nullptr ? "" : StringUtils::itoa(*(uint64_t*)p);
  };
  _values[name] = (void*)value;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an uint64_t vector argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator()(
    const string& full, vector<uint64_t>* value, const string& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_VECTOR_UINT64;
  _vectorUInt64Options[name] = value;
  _helpTexts[name] = text;
  _defaultTexts[name] = "";

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a double argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator()(
    const string& full, double* value, const string& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_DOUBLE;
  _doubleOptions[name] = value;
  _helpTexts[name] = text;
  _defaultTexts[name] = StringUtils::ftoa(*value);
  _currentTexts[name] = [](void* p) -> string {
    return p == nullptr ? "" : StringUtils::ftoa(*(double*)p);
  };
  _values[name] = (void*)value;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a double vector argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator()(
    const string& full, vector<double>* value, const string& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_VECTOR_DOUBLE;
  _vectorDoubleOptions[name] = value;
  _helpTexts[name] = text;
  _defaultTexts[name] = "";

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a boolean argument
////////////////////////////////////////////////////////////////////////////////

ProgramOptionsDescription& ProgramOptionsDescription::operator()(
    const string& full, bool* value, const string& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_BOOL;
  _boolOptions[name] = value;
  _helpTexts[name] = text;
  _defaultTexts[name] = (*value ? "true" : "false");
  _currentTexts[name] = [](void* p) -> string {
    return p == nullptr ? "" : string(((*(bool*)p) ? "true" : "false"));
  };
  _values[name] = (void*)value;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a time_t argument
////////////////////////////////////////////////////////////////////////////////

#if __WORDSIZE == 32

ProgramOptionsDescription& ProgramOptionsDescription::operator()(
    const string& full, time_t* value, const string& text) {
  string name = check(full, value);

  _optionNames.push_back(name);
  _optionTypes[name] = OPTION_TYPE_TIME;
  _timeOptions[name] = value;
  _helpTexts[name] = text;
  _defaultTexts[name] = StringUtils::itoa((int64_t)*value);
  _currentTexts[name] = [](void* p) -> string {
    return p == nullptr ? "" : StringUtils::itoa((int64_t)(*(time_t*)p));
  };
  _values[name] = (void*)value;

  return *this;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief adds positional arguments
////////////////////////////////////////////////////////////////////////////////

void ProgramOptionsDescription::arguments(vector<string>* value) {
  if (value == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "value is nullptr");
  }

  if (_positionals != nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "_positional arguments are already defined");
  }

  _positionals = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the usage message
////////////////////////////////////////////////////////////////////////////////

string ProgramOptionsDescription::usage() const {
  set<string> help;
  return usage(help);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the usage message
////////////////////////////////////////////////////////////////////////////////

string ProgramOptionsDescription::usage(set<string> help) const {
  // footer with info about specific sections
  string footer;

  // we want all help, therefore use all help sections
  bool helpAll = help.find("--HELP-ALL--") != help.end();
  help.erase("--HELP-ALL--");
  help.erase("--HELP--");

  if (help.empty()) {
    help.insert("help-default");
  }

  set<string> ho = helpOptions();

  // remove help default
  set<string> hd = ho;
  hd.erase("help-default");

  // add footer
  if (helpAll) {
    help = ho;
    footer = "\nFor specific sections use: " + StringUtils::join(hd, ", ") +
             " or help";
  } else {
    set<string> is;
    set_difference(hd.begin(), hd.end(), help.begin(), help.end(),
                   inserter(is, is.end()));

    if (!is.empty()) {
      footer = "\nFor more information use: " + StringUtils::join(is, ", ") +
               " or help-all";
    }
  }

  // compute all relevant names
  map<string, string> names;
  fillAllNames(help, names);
  size_t oWidth = 0;

  for (auto const& name : names) {
    size_t len = name.second.length();

    if (oWidth < len) {
      oWidth = len;
    }
  }

  return usageString(help, names, oWidth) + footer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a list of help options
////////////////////////////////////////////////////////////////////////////////

set<string> ProgramOptionsDescription::helpOptions() const {
  set<string> options = _helpOptions;

  for (vector<ProgramOptionsDescription>::const_iterator i =
           _subDescriptions.begin();
       i != _subDescriptions.end(); ++i) {
    set<string> o = i->helpOptions();

    options.insert(o.begin(), o.end());
  }

  return options;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get default value for an option
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* ProgramOptionsDescription::getDefault(
    std::string const& option) const {
  auto it = _optionTypes.find(option);
  if (it == _optionTypes.end()) {
    return nullptr;
  }
  auto type = (*it).second;

  auto it2 = _values.find(option);
  if (it2 == _values.end()) {
    return nullptr;
  }
  auto value = (*it2).second;

  TRI_json_t* json = nullptr;

  switch (type) {
    case OPTION_TYPE_STRING: {
      auto v = static_cast<std::string*>(value);
      if (v == nullptr || v->empty()) {
        json = TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "", 0);
      } else {
        json = TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, v->c_str(),
                                        v->size());
      }
      break;
    }

    case OPTION_TYPE_BOOL: {
      auto v = static_cast<bool*>(value);
      if (v != nullptr) {
        json = TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, *v);
      }
      break;
    }

    case OPTION_TYPE_DOUBLE: {
      auto v = static_cast<double*>(value);
      if (v != nullptr) {
        json = TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, *v);
      }
      break;
    }

    case OPTION_TYPE_INT32: {
      auto v = static_cast<int32_t*>(value);
      if (v != nullptr) {
        json =
            TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, static_cast<double>(*v));
      }
      break;
    }

    case OPTION_TYPE_INT64: {
      auto v = static_cast<int64_t*>(value);
      if (v != nullptr) {
        json =
            TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, static_cast<double>(*v));
      }
      break;
    }

    case OPTION_TYPE_UINT32: {
      auto v = static_cast<uint32_t*>(value);
      if (v != nullptr) {
        json =
            TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, static_cast<double>(*v));
      }
      break;
    }

    case OPTION_TYPE_UINT64: {
      auto v = static_cast<uint64_t*>(value);
      if (v != nullptr) {
        json =
            TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, static_cast<double>(*v));
      }
      break;
    }

    default: { TRI_ASSERT(json == nullptr); }
  }

  return json;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief computes all names
////////////////////////////////////////////////////////////////////////////////

void ProgramOptionsDescription::fillAllNames(const set<string>& help,
                                             map<string, string>& names) const {
  for (vector<string>::const_iterator i = _optionNames.begin();
       i != _optionNames.end(); ++i) {
    const string& option = *i;
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
  }

  for (vector<ProgramOptionsDescription>::const_iterator i =
           _subDescriptions.begin();
       i != _subDescriptions.end(); ++i) {
    ProgramOptionsDescription pod = *i;
    set<string> ho = pod._helpOptions;

    if (ho.empty()) {
      pod.fillAllNames(help, names);
    } else {
      set<string> is;
      set_intersection(ho.begin(), ho.end(), help.begin(), help.end(),
                       std::inserter(is, is.end()));

      if (!is.empty()) {
        pod.fillAllNames(help, names);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the usage message for given sections
////////////////////////////////////////////////////////////////////////////////

string ProgramOptionsDescription::usageString(const set<string>& help,
                                              const map<string, string>& names,
                                              size_t oWidth) const {
  // extract help-able sub-descriptions
  vector<ProgramOptionsDescription> subDescriptions;

  for (vector<ProgramOptionsDescription>::const_iterator i =
           _subDescriptions.begin();
       i != _subDescriptions.end(); ++i) {
    ProgramOptionsDescription pod = *i;
    set<string> ho = pod._helpOptions;

    if (ho.empty()) {
      subDescriptions.push_back(pod);
    } else {
      set<string> is;
      set_intersection(ho.begin(), ho.end(), help.begin(), help.end(),
                       std::inserter(is, is.end()));

      if (!is.empty()) {
        subDescriptions.push_back(pod);
      }
    }
  }

  // write help only if help options match
  string desc = usageString(names, oWidth);

  // check for sub-descriptions
  string sep = desc.empty() ? "" : "\n";
  string lastName;

  for (vector<ProgramOptionsDescription>::iterator i = subDescriptions.begin();
       i != subDescriptions.end(); ++i) {
    string u = i->usageString(help, names, oWidth);

    if (!u.empty()) {
      if (lastName == i->_name) {
        desc += sep + u;
      } else {
        desc += sep + i->_name + "\n" + u;
      }

      sep = "\n";
      lastName = i->_name;
    }
  }

  return desc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs the usage string
////////////////////////////////////////////////////////////////////////////////

string ProgramOptionsDescription::usageString(const map<string, string>& names,
                                              size_t oWidth) const {
  // the usage string without a headline
  string desc = "";

  // construct the parameters
  size_t tWidth = TRI_ColumnsWidth();

  if (tWidth < 40) {
    tWidth = 40;
  }

  size_t sWidth = 8;
  size_t dWidth =
      (oWidth + sWidth < tWidth) ? (tWidth - oWidth - sWidth) : (tWidth / 2);

  vector<string> on = _optionNames;
  sort(on.begin(), on.end());

  for (vector<string>::iterator i = on.begin(); i != on.end(); ++i) {
    const string& option = *i;
    const string& name = names.find(option)->second;

    string text = _helpTexts.find(option)->second;
    string defval = _defaultTexts.find(option)->second;

    auto curitr = _currentTexts.find(option);
    string current = "";

    if (curitr != _currentTexts.end()) {
      current = (curitr->second)(_values.find(option)->second);
    }

    if (defval == "") {
      if (current != "") {
        text += " (current: " + current + ")";
      }
    } else if (defval == current || current == "") {
      text += " (default: " + defval + ")";
    } else {
      text += " (default: " + defval + ", current: " + current + ")";
    }

    if (text.size() <= dWidth) {
      desc += "  --" + StringUtils::rFill(name, oWidth) + "    " + text + "\n";
    } else {
      vector<string> wrap = StringUtils::wrap(text, dWidth, " ,");

      string sep = "  --" + StringUtils::rFill(name, oWidth) + "    ";

      for (vector<string>::iterator j = wrap.begin(); j != wrap.end(); ++j) {
        desc += sep + *j + "\n";
        sep = string(oWidth + sWidth, ' ');
      }
    }
  }

  return desc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the name is an option, defines short/long mapping
////////////////////////////////////////////////////////////////////////////////

string ProgramOptionsDescription::check(const string& name) {
  vector<string> s = StringUtils::split(name, ',');

  if (name.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "option name is empty");
  }

  if (s.size() > 2) {
    std::string message(
        "option '" + name +
        "' should be <long-option>,<short-option> or <long-option>");
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
  }

  string longOption = s[0];

  if (_optionTypes.find(longOption) != _optionTypes.end()) {
    std::string message("option '" + longOption + "' is already defined");
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
  }

  if (s.size() > 1) {
    _long2short[longOption] = s[1];
  }

  return longOption;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the name is an option, defines short/long mapping
////////////////////////////////////////////////////////////////////////////////

string ProgramOptionsDescription::check(const string& name, void* value) {
  if (value == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "value is nullptr");
  }

  return check(name);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|//
// --SECTION--\\|/// @\\}"
// End:
