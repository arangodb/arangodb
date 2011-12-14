////////////////////////////////////////////////////////////////////////////////
/// @brief program options
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
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ProgramOptions.h"

#include <fstream>

#include <boost/program_options.hpp>

#include <Basics/Exceptions.h>

using namespace std;

namespace bpo = boost::program_options;

namespace {
  string const& lookup (map<string, string> const& m, string const& key) {
    static string empty = "";

    map<string, string>::const_iterator i = m.find(key);

    if (i == m.end()) {
      return empty;
    }
    else {
      return i->second;
    }
  }


  template<typename T>
  T* lookup (map<string, T*> const& m, string const& key) {
    typename map<string, T*>::const_iterator i = m.find(key);

    if (i == m.end()) {
      return 0;
    }
    else {
      return i->second;
    }
  }


  template<typename T>
  T find (map<string, T> const& m, string const& key) {
    typename map<string, T>::const_iterator i = m.find(key);

    if (i == m.end()) {
      THROW_INTERNAL_ERROR("cannot find option '" + key + "'");
    }

    return i->second;
  }
}

namespace triagens {
  namespace basics {

    // -----------------------------------------------------------------------------
    // implementation structure
    // -----------------------------------------------------------------------------

    struct ProgramOptionsData {
      bpo::options_description setupDescription (ProgramOptionsDescription const& description) {
        bpo::options_description desc = setupSubDescription(description);
        set<string> ho = description.helpOptions();

        for (set<string>::const_iterator i = ho.begin(); i != ho.end(); ++i) {
          string const& option = *i;

          desc.add_options()(option.c_str(), "more help");
        }

        if (! ho.empty()) {
          desc.add_options()("help-all", "show help for all options");
        }

        return desc;
      }

      bpo::options_description setupSubDescription (ProgramOptionsDescription const& description) {
        bpo::options_description desc(description._name);

        for (vector<string>::const_iterator i = description._optionNames.begin();  i != description._optionNames.end();  ++i) {
          string const& name = *i;
          string const& help = lookup(description._helpTexts, name);
          string option = name;

          map<string, string>::const_iterator j = description._long2short.find(name);

          if (j != description._long2short.end()) {
            option += "," + j->second;
          }

          _options.push_back(option);

          string const& boption = _options[_options.size() - 1];

          ProgramOptionsDescription::option_type_e type = find(description._optionTypes, name);

          switch (type) {
            case ProgramOptionsDescription::OPTION_TYPE_FLAG:
              desc.add_options()(boption.c_str(), help.c_str());
              break;

            case ProgramOptionsDescription::OPTION_TYPE_STRING:
              desc.add_options()(boption.c_str(), bpo::value<string>(find(description._stringOptions, name)), help.c_str());
              _stringValues[name] = find(description._stringOptions, name);
              break;

            case ProgramOptionsDescription::OPTION_TYPE_VECTOR_STRING:
              desc.add_options()(boption.c_str(), bpo::value< vector<string> >(find(description._vectorStringOptions, name)), help.c_str());
              break;

            case ProgramOptionsDescription::OPTION_TYPE_INT32:
              desc.add_options()(boption.c_str(), bpo::value<int32_t>(find(description._int32Options, name)), help.c_str());
              break;

            case ProgramOptionsDescription::OPTION_TYPE_VECTOR_INT32:
              desc.add_options()(boption.c_str(), bpo::value< vector<int32_t> >(find(description._vectorInt32Options, name)), help.c_str());
              break;

            case ProgramOptionsDescription::OPTION_TYPE_INT64:
              desc.add_options()(boption.c_str(), bpo::value<int64_t>(find(description._int64Options, name)), help.c_str());
              break;

            case ProgramOptionsDescription::OPTION_TYPE_VECTOR_INT64:
              desc.add_options()(boption.c_str(), bpo::value< vector<int64_t> >(find(description._vectorInt64Options, name)), help.c_str());
              break;

            case ProgramOptionsDescription::OPTION_TYPE_UINT32:
              desc.add_options()(boption.c_str(), bpo::value<uint32_t>(find(description._uint32Options, name)), help.c_str());
              break;

            case ProgramOptionsDescription::OPTION_TYPE_VECTOR_UINT32:
              desc.add_options()(boption.c_str(), bpo::value< vector<uint32_t> >(find(description._vectorUint32Options, name)), help.c_str());
              break;

            case ProgramOptionsDescription::OPTION_TYPE_UINT64:
              desc.add_options()(boption.c_str(), bpo::value<uint64_t>(find(description._uint64Options, name)), help.c_str());
              break;

            case ProgramOptionsDescription::OPTION_TYPE_VECTOR_UINT64:
              desc.add_options()(boption.c_str(), bpo::value< vector<uint64_t> >(find(description._vectorUint64Options, name)), help.c_str());
              break;

            case ProgramOptionsDescription::OPTION_TYPE_DOUBLE:
              desc.add_options()(boption.c_str(), bpo::value<double>(find(description._doubleOptions, name)), help.c_str());
              break;

            case ProgramOptionsDescription::OPTION_TYPE_VECTOR_DOUBLE:
              desc.add_options()(boption.c_str(), bpo::value< vector<double> >(find(description._vectorDoubleOptions, name)), help.c_str());
              break;

            case ProgramOptionsDescription::OPTION_TYPE_BOOL:
              desc.add_options()(boption.c_str(), bpo::value<bool>(find(description._boolOptions, name)), help.c_str());
              break;

            default:
              THROW_INTERNAL_ERROR("unknown option type");
          }
        }

        for (vector<ProgramOptionsDescription>::const_iterator i = description._subDescriptions.begin();
             i != description._subDescriptions.end();
             ++i) {
          desc.add(setupSubDescription(*i));
        }

        for (vector<ProgramOptionsDescription>::const_iterator i = description._hiddenSubDescriptions.begin();
             i != description._hiddenSubDescriptions.end();
             ++i) {
          desc.add(setupSubDescription(*i));
        }

        return desc;
      }

      map<string, string*> _stringValues;

      boost::program_options::variables_map _vm;
      vector<string> _options;
    };

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    ProgramOptions::ProgramOptions () {
      _data = new ProgramOptionsData;
    }



    ProgramOptions::~ProgramOptions () {
      delete _data;
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    bool ProgramOptions::parse (ProgramOptionsDescription const& description, int argc, char** argv) {
      boost::program_options::options_description desc = _data->setupDescription(description);
      set<string> const& ho = description.helpOptions();
      _helpOptions.insert(ho.begin(), ho.end());

      bpo::positional_options_description pdesc;

      if (description._positionals != 0) {
        pdesc.add("positional_arguments", -1);

        bpo::options_description hidden("Positional Arguments");
        hidden.add_options()("positional_arguments", boost::program_options::value< vector<string> >(description._positionals), "arguments");
        desc.add(hidden);
      }

      try {
        if (description._positionals != 0) {
          bpo::store(boost::program_options::command_line_parser(argc, argv).options(desc).positional(pdesc).run(), _data->_vm);
        }
        else {
          bpo::store(boost::program_options::command_line_parser(argc, argv).options(desc).run(), _data->_vm);
        }

        boost::program_options::notify(_data->_vm);
        return true;
      }
      catch (const boost::program_options::error& e) {
        _errorMessage = e.what();
        return false;
      }
    }



    bool ProgramOptions::parse (ProgramOptionsDescription const& description, string const& filename) {
      boost::program_options::options_description desc = _data->setupDescription(description);
      set<string> const& ho = description.helpOptions();
      _helpOptions.insert(ho.begin(), ho.end());

      try {
        ifstream ifs(filename.c_str());

        bpo::store(parse_config_file(ifs, desc), _data->_vm);
        boost::program_options::notify(_data->_vm);

        return true;
      }
      catch (const boost::program_options::error& e) {
        _errorMessage = e.what();
        return false;
      }
    }



    bool ProgramOptions::has (string const& key) const {
      return _data->_vm.count(key.c_str()) > 0;
    }



    set<string> ProgramOptions::needHelp (string const& key) const {
      set<string> result;

      if (_data->_vm.count(key.c_str()) > 0) {
        result.insert("--HELP--");
      }

      if (_data->_vm.count("help-all") > 0) {
        result.insert("--HELP-ALL--");
      }

      for (set<string>::const_iterator i = _helpOptions.begin();  i != _helpOptions.end();  ++i) {
        string const& hkey = *i;

        if (_data->_vm.count(hkey.c_str())) {
          result.insert(hkey);
        }
      }

      return result;
    }



    string ProgramOptions::stringValue (string const& key) const {
      string* value = lookup(_data->_stringValues, key);

      return value == 0 ? "" : *value;
    }
  }
}
