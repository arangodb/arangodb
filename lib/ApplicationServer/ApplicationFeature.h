////////////////////////////////////////////////////////////////////////////////
/// @brief application server feature
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

#ifndef TRIAGENS_FYN_REST_APPLICATION_FEATURE_H
#define TRIAGENS_FYN_REST_APPLICATION_FEATURE_H 1

#include <Basics/Common.h>

#include "ApplicationServer/ApplicationServer.h"

namespace triagens {
  namespace basics {
    class ProgramOptionsDescription;
  }

  namespace rest {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief application server feature
    ////////////////////////////////////////////////////////////////////////////////

    class ApplicationFeature {
      private:
        ApplicationFeature (ApplicationFeature const&);
        ApplicationFeature& operator= (ApplicationFeature const&);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructor
        ////////////////////////////////////////////////////////////////////////////////

        ApplicationFeature () {
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief destructor
        ////////////////////////////////////////////////////////////////////////////////

        virtual ~ApplicationFeature () {
        }

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief setups the options
        ////////////////////////////////////////////////////////////////////////////////

        virtual void setupOptions (map<string, basics::ProgramOptionsDescription>&) {
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief parsing phase 1, before config file
        ////////////////////////////////////////////////////////////////////////////////

        virtual bool parsePhase1 (basics::ProgramOptions&) {
          return true;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief parsing phase 2, after config file
        ////////////////////////////////////////////////////////////////////////////////

        virtual bool parsePhase2 (basics::ProgramOptions&) {
          return true;
        }
    };
  }
}

#endif
