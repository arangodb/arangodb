////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_HANDLER_REST_AGENCY_PRIV_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_AGENCY_PRIV_HANDLER_H 1

#include "RestHandler/RestBaseHandler.h"
#include "Agency/Agent.h"

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief REST handler for private agency communication
///        (vote, appendentries, notify)
////////////////////////////////////////////////////////////////////////////////

class RestAgencyPrivHandler : public arangodb::RestBaseHandler {
 public:
  explicit RestAgencyPrivHandler(HttpRequest*, consensus::Agent*);

  bool isDirect() const override;

  status_t execute() override;

 private:
  template <class T>
  inline bool readValue(char const* name, T& val) const {
    bool found = true;
    std::string const& val_str = _request->value(name, found);

    if (!found) {
      LOG_TOPIC(WARN, Logger::AGENCY)
        << "Mandatory query string " << name << " missing.";
      return false;
    } else {
      try {
        val = std::stoul(val_str);
      } catch (std::invalid_argument const&) {
        LOG_TOPIC(WARN, Logger::AGENCY) << "Value for query string " << name
                  << " cannot be converted to integral type";
        return false;
      }
    }
    return true;
  }

  status_t reportErrorEmptyRequest();
  status_t reportTooManySuffices();
  status_t reportBadQuery();
  status_t reportMethodNotAllowed();

  consensus::Agent* _agent;
};
}

#endif
