////////////////////////////////////////////////////////////////////////////////
/// @brief application server skeleton
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

#include "ApplicationServer.h"

#include "ApplicationServer/ApplicationServerSchedulerImpl.h"

using namespace triagens::basics;

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // constants
    // -----------------------------------------------------------------------------

    string const ApplicationServer::OPTIONS_CMDLINE = "Command Line Options";
    string const ApplicationServer::OPTIONS_HIDDEN = "Hidden Options";
    string const ApplicationServer::OPTIONS_LIMITS = "Limit Options";
    string const ApplicationServer::OPTIONS_LOGGER = "Logging Options";
    string const ApplicationServer::OPTIONS_SERVER = "Server Options";
    string const ApplicationServer::OPTIONS_RECOVERY_REPLICATION = "Recovery Replication Options";

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    ApplicationServer* ApplicationServer::create (string const& description, string const& version) {
      return new ApplicationServerSchedulerImpl(description, version);
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    bool ApplicationServer::parse (int argc, char* argv[]) {
      map<string, ProgramOptionsDescription> none;

      return parse(argc, argv, none);
    }
  }
}
