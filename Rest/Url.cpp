////////////////////////////////////////////////////////////////////////////////
/// @brief url class
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
/// @author Achim Brandt
/// @author Copyright 2007-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Url.h"

#include <iostream>

#include <boost/regex.hpp>

#include <Basics/Exceptions.h>
#include <Basics/StringUtils.h>

using namespace triagens::basics;

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    Url::Url (string const& urlName) {
      static string pattern = "([a-zA-Z]*:)//([a-zA-Z0-9\\._-]*)(:[0-9]+)?(/.*)";
      static boost::regex_constants::syntax_option_type flags = boost::regex_constants::perl;

      boost::regex re;

      re.assign(pattern, flags);

      boost::smatch what;

      bool result = boost::regex_match(urlName, what, re);

      if (result) {
        string port = what[3];

        serviceValue = what[1];
        hostValue = what[2];
        portValue = port.empty() ? 80 : StringUtils::uint32(port.substr(1));
        pathValue = what[4];
      }
      else {
        THROW_PARAMETER_ERROR("url", "constructor", "url not valid");
      }

      if (serviceValue.empty()) {
        serviceValue = "http";
      }
      else {
        serviceValue.erase(serviceValue.size() - 1);
      }
    }
  }
}
