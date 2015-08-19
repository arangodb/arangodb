////////////////////////////////////////////////////////////////////////////////
/// @brief application server feature
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
/// @author Copyright 2010-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_APPLICATION_SERVER_APPLICATION_FEATURE_H
#define ARANGODB_APPLICATION_SERVER_APPLICATION_FEATURE_H 1

#include "Basics/Common.h"

#include "ApplicationServer/ApplicationServer.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace basics {
    class ProgramOptionsDescription;
  }

// -----------------------------------------------------------------------------
// --SECTION--                                          class ApplicationFeature
// -----------------------------------------------------------------------------

  namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief application server feature
////////////////////////////////////////////////////////////////////////////////

    class ApplicationFeature {
      private:
        ApplicationFeature (ApplicationFeature const&);
        ApplicationFeature& operator= (ApplicationFeature const&);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        ApplicationFeature (std::string const& name);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        virtual ~ApplicationFeature ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the name
////////////////////////////////////////////////////////////////////////////////

        std::string const& getName () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief setups the options
////////////////////////////////////////////////////////////////////////////////

        virtual void setupOptions (std::map<std::string, basics::ProgramOptionsDescription>&);

////////////////////////////////////////////////////////////////////////////////
/// @brief parsing phase 1, before config file
////////////////////////////////////////////////////////////////////////////////

        virtual bool parsePhase1 (basics::ProgramOptions&);

////////////////////////////////////////////////////////////////////////////////
/// @brief parsing phase 2, after config file
////////////////////////////////////////////////////////////////////////////////

        virtual bool parsePhase2 (basics::ProgramOptions&);

////////////////////////////////////////////////////////////////////////////////
/// @brief prepares the feature
////////////////////////////////////////////////////////////////////////////////

        virtual bool prepare ();

////////////////////////////////////////////////////////////////////////////////
/// @brief prepares the feature
///
/// there might be dependencies between features, e.g. F1 depends on F2
/// F1->prepare() would need to be called after F2->prepare() but this is not
/// possible. prepare2() gives a feature a second chance to initialise itself.
/// it is called after all prepare() actions have been finished
////////////////////////////////////////////////////////////////////////////////

        virtual bool prepare2 ();

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the feature
////////////////////////////////////////////////////////////////////////////////

        virtual bool start ();

////////////////////////////////////////////////////////////////////////////////
/// @brief opens the feature for business
////////////////////////////////////////////////////////////////////////////////

        virtual bool open ();

////////////////////////////////////////////////////////////////////////////////
/// @brief closes the feature
////////////////////////////////////////////////////////////////////////////////

        virtual void close ();

////////////////////////////////////////////////////////////////////////////////
/// @brief stops everything
////////////////////////////////////////////////////////////////////////////////

        virtual void stop ();

////////////////////////////////////////////////////////////////////////////////
/// @brief disable feature
////////////////////////////////////////////////////////////////////////////////

        void disable ();

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief feature is disabled
////////////////////////////////////////////////////////////////////////////////

        bool _disabled;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the feature
////////////////////////////////////////////////////////////////////////////////

        std::string const _name;
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
