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

#ifndef TRIAGENS_BASICS_PROGRAM_OPTIONS_H
#define TRIAGENS_BASICS_PROGRAM_OPTIONS_H 1

#include "Basics/Common.h"

#include "BasicsC/vector.h"
#include "Basics/ProgramOptionsDescription.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_program_options_s;
struct TRI_PO_section_s;

namespace triagens {
  namespace basics {

// -----------------------------------------------------------------------------
// --SECTION--                                   class ProgramOptionsDescription
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ProgramOptions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief ProgramOptions
////////////////////////////////////////////////////////////////////////////////

    class ProgramOptions {
      private:
        ProgramOptions (ProgramOptions const&);
        ProgramOptions& operator= (ProgramOptions const&);

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

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        ProgramOptions ();

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~ProgramOptions ();

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

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief parse command line
////////////////////////////////////////////////////////////////////////////////

        bool parse (ProgramOptionsDescription const&, int argc, char** argv);

////////////////////////////////////////////////////////////////////////////////
/// @brief parse configuration file
////////////////////////////////////////////////////////////////////////////////

        bool parse (ProgramOptionsDescription const&, string const& filename);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if option was given
////////////////////////////////////////////////////////////////////////////////

        bool has (string const& name) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if help option was given
////////////////////////////////////////////////////////////////////////////////

        set<string> needHelp (string const& name) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the last error
////////////////////////////////////////////////////////////////////////////////

        string lastError ();

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

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief generates description for the main section
////////////////////////////////////////////////////////////////////////////////

        struct TRI_PO_section_s* setupDescription (ProgramOptionsDescription const& description);

////////////////////////////////////////////////////////////////////////////////
/// @brief generates description for the sub-sections
////////////////////////////////////////////////////////////////////////////////

        void setupSubDescription (ProgramOptionsDescription const& description, struct TRI_PO_section_s* desc);

////////////////////////////////////////////////////////////////////////////////
/// @brief result data
////////////////////////////////////////////////////////////////////////////////

        bool extractValues (ProgramOptionsDescription const&, struct TRI_program_options_s*, set<string> seen);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ProgramOptions
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief bool values
////////////////////////////////////////////////////////////////////////////////

        map<string, bool*> _valuesBool;

////////////////////////////////////////////////////////////////////////////////
/// @brief string values
////////////////////////////////////////////////////////////////////////////////

        map<string, char**> _valuesString;

////////////////////////////////////////////////////////////////////////////////
/// @brief vector values
////////////////////////////////////////////////////////////////////////////////

        map<string, TRI_vector_string_t*> _valuesVector;

////////////////////////////////////////////////////////////////////////////////
/// @brief options
////////////////////////////////////////////////////////////////////////////////

        vector<string> _options;

////////////////////////////////////////////////////////////////////////////////
/// @brief error messages
////////////////////////////////////////////////////////////////////////////////

        string _errorMessage;

////////////////////////////////////////////////////////////////////////////////
/// @brief help options
////////////////////////////////////////////////////////////////////////////////

        set<string> _helpOptions;

////////////////////////////////////////////////////////////////////////////////
/// @brief flags which are set
////////////////////////////////////////////////////////////////////////////////

        set<string> _flags;

////////////////////////////////////////////////////////////////////////////////
/// @brief flags which are already set
////////////////////////////////////////////////////////////////////////////////

        set<string> _seen;

////////////////////////////////////////////////////////////////////////////////
/// @brief program name if known
////////////////////////////////////////////////////////////////////////////////

        string _programName;
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
