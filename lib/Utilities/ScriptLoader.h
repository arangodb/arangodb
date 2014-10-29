////////////////////////////////////////////////////////////////////////////////
/// @brief source code loader
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_UTILITIES_SCRIPT_LOADER_H
#define ARANGODB_UTILITIES_SCRIPT_LOADER_H 1

#include "Basics/Common.h"

#include "Basics/Mutex.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    class JSLoader
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript source code loader
////////////////////////////////////////////////////////////////////////////////

    class ScriptLoader {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a loader
////////////////////////////////////////////////////////////////////////////////

        ScriptLoader ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the directory for scripts
////////////////////////////////////////////////////////////////////////////////

        std::string const& getDirectory () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the directory for scripts
////////////////////////////////////////////////////////////////////////////////

        void setDirectory (std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief build a script from an array of strings
////////////////////////////////////////////////////////////////////////////////

        std::string buildScript (const char** script);

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a new named script
////////////////////////////////////////////////////////////////////////////////

        void defineScript (std::string const& name, std::string const& script);

        void defineScript (std::string const& name, const char** script);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a named script
////////////////////////////////////////////////////////////////////////////////

        std::string const& findScript (std::string const& name);

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a list of all specified directory parts
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::string> getDirectoryParts ();

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief all scripts
////////////////////////////////////////////////////////////////////////////////

        std::map<std::string, std::string> _scripts;

////////////////////////////////////////////////////////////////////////////////
/// @brief script directory
////////////////////////////////////////////////////////////////////////////////

        std::string _directory;

////////////////////////////////////////////////////////////////////////////////
/// @brief mutex for _scripts
////////////////////////////////////////////////////////////////////////////////

        basics::Mutex _lock;
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
