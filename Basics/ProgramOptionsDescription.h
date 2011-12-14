////////////////////////////////////////////////////////////////////////////////
/// @brief program options description
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
/// @author Copyright 2010-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_JUTLAND_BASICS_PROGRAM_OPTIONS_DESCRIPTION_H
#define TRIAGENS_JUTLAND_BASICS_PROGRAM_OPTIONS_DESCRIPTION_H 1

#include <Basics/Common.h>

namespace triagens {
  namespace basics {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief program options description
    ////////////////////////////////////////////////////////////////////////////////

    class ProgramOptionsDescription {
      friend struct ProgramOptionsData;
      friend class ProgramOptions;

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructor
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription ();

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructor
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription (string const& name);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief copy constructor
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription (ProgramOptionsDescription const&);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief assignment constructor
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription& operator= (ProgramOptionsDescription const&);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief changes the name
        ////////////////////////////////////////////////////////////////////////////////

        void setName (string const& name);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief adds a new section
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription& operator() (ProgramOptionsDescription&);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief adds a new hidden section
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription& operator() (ProgramOptionsDescription&, bool hidden);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief adds a new flag
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription& operator() (string const& name, string const& text);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief adds a string argument
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription& operator() (string const& name, string* value, string const& text);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief adds a string vector argument
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription& operator() (string const& name, vector<string>* value, string const& text);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief adds an int32_t argument
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription& operator() (string const& name, int32_t* value, string const& text);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief adds an int32_t vector argument
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription& operator() (string const& name, vector<int32_t>* value, string const& text);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief adds an int64_t argument
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription& operator() (string const& name, int64_t* value, string const& text);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief adds an int64_t vector argument
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription& operator() (string const& name, vector<int64_t>* value, string const& text);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief adds an uint32_t argument
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription& operator() (string const& name, uint32_t* value, string const& text);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief adds an uint32_t vector argument
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription& operator() (string const& name, vector<uint32_t>* value, string const& text);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief adds an uint64_t argument
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription& operator() (string const& name, uint64_t* value, string const& text);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief adds an uint64_t vector argument
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription& operator() (string const& name, vector<uint64_t>* value, string const& text);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief adds a double argument
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription& operator() (string const& name, double* value, string const& text);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief adds a double vector argument
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription& operator() (string const& name, vector<double>* value, string const& text);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief adds a boolean argument
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription& operator() (string const& name, bool* value, string const& text);

#if __WORDSIZE == 32
        ////////////////////////////////////////////////////////////////////////////////
        /// @brief adds a time_t argument
        ////////////////////////////////////////////////////////////////////////////////

        ProgramOptionsDescription& operator() (string const& name, time_t* value, string const& text);
#endif
        ////////////////////////////////////////////////////////////////////////////////
        /// @brief adds positional arguments
        ////////////////////////////////////////////////////////////////////////////////

        void arguments (vector<string>*);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the usage message
        ////////////////////////////////////////////////////////////////////////////////

        string usage () const;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the usage message
        ////////////////////////////////////////////////////////////////////////////////

        string usage (set<string> const& help, bool addHelpOptions = true) const;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns a list of help options
        ////////////////////////////////////////////////////////////////////////////////

        set<string> helpOptions () const;

      private:
        enum option_type_e {
          OPTION_TYPE_FLAG,
          OPTION_TYPE_STRING,
          OPTION_TYPE_VECTOR_STRING,
          OPTION_TYPE_INT32,
          OPTION_TYPE_VECTOR_INT64,
          OPTION_TYPE_INT64,
          OPTION_TYPE_VECTOR_INT32,
          OPTION_TYPE_UINT32,
          OPTION_TYPE_VECTOR_UINT32,
          OPTION_TYPE_UINT64,
          OPTION_TYPE_VECTOR_UINT64,
          OPTION_TYPE_DOUBLE,
          OPTION_TYPE_VECTOR_DOUBLE,
          OPTION_TYPE_BOOL

#if __WORDSIZE == 32
          ,OPTION_TYPE_TIME_T
#endif
        };

      private:
        string check (string const& name);
        string check (string const& name, void* value);

      private:
        string _name;
        set<string> _helpOptions;

        vector<ProgramOptionsDescription> _subDescriptions;
        vector<ProgramOptionsDescription> _hiddenSubDescriptions;

        vector<string> _optionNames;

        map< string, option_type_e > _optionTypes;
        map< string, string > _long2short;
        map< string, string > _helpTexts;

        map< string, string* > _stringOptions;
        map< string, vector<string>* > _vectorStringOptions;

        map< string, int32_t* > _int32Options;
        map< string, vector<int32_t>* > _vectorInt32Options;

        map< string, int64_t* > _int64Options;
        map< string, vector<int64_t>* > _vectorInt64Options;

        map< string, uint32_t* > _uint32Options;
        map< string, vector<uint32_t>* > _vectorUint32Options;

        map< string, uint64_t* > _uint64Options;
        map< string, vector<uint64_t>* > _vectorUint64Options;

        map< string, double* > _doubleOptions;
        map< string, vector<double>* > _vectorDoubleOptions;

        map< string, bool* > _boolOptions;

#if __WORDSIZE == 32
        map< string, time_t* > _timeOptions;
#endif
        vector<string>* _positionals;
    };
  }
}

#endif
