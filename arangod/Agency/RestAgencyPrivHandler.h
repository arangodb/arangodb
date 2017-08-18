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

#include "Agency/Agent.h"
#include "RestHandler/RestBaseHandler.h"

namespace arangodb {

// String to number conversions:

template<class T> struct sto;

template<> struct sto<uint64_t> {
  static uint64_t convert(std::string const& s) {
    return std::stoull(s);
  }
};
template<> struct sto<int64_t> {
  static uint64_t convert(std::string const& s) {
    return std::stoll(s);
  }
};
template<> struct sto<int32_t> {
   static long convert(std::string const& s) {
     return std::stol(s);
   }
};
template<> struct sto<uint32_t> {
  static uint64_t convert(std::string const& s) {
    return std::stoul(s);
  }
};


////////////////////////////////////////////////////////////////////////////////
/// @brief REST handler for private agency communication
///        (vote, appendentries, notify)
////////////////////////////////////////////////////////////////////////////////

class RestAgencyPrivHandler : public arangodb::RestBaseHandler {
 public:
  RestAgencyPrivHandler(GeneralRequest*, GeneralResponse*, consensus::Agent*);

 public:
  char const* name() const override final { return "RestAgencyPrivHandler"; }
  bool isDirect() const override;
  bool needsOwnThread() const { return true; }
  RestStatus execute() override;

 private:
  template <class T>
  inline bool readValue(char const* name, T& val) const {
    bool found = true;
    std::string const& val_str = _request->value(name, found);

    if (!found) {
      LOG_TOPIC(WARN, Logger::AGENCY) << "Mandatory query string " << name
                                      << " missing.";
      return false;
    } else {
      try {
        val = sto<T>::convert(val_str);
      } catch (std::invalid_argument const&) {
        LOG_TOPIC(WARN, Logger::AGENCY)
            << "Value for query string " << name
            << " cannot be converted to integral type";
        return false;
      } catch (std::out_of_range const&) {
        LOG_TOPIC(WARN, Logger::AGENCY)
            << "Value for query string " << name
            << " does not fit into range of integral type";
        return false;
      }
    }
    return true;
  }

  RestStatus reportErrorEmptyRequest();
  RestStatus reportTooManySuffices();
  RestStatus reportBadQuery(std::string const& message = "bad parameter");
  RestStatus reportMethodNotAllowed();
  RestStatus reportGone();

  consensus::Agent* _agent;
};

template <>
inline bool RestAgencyPrivHandler::readValue(char const* name,
                                             std::string& val) const {
  bool found = true;
  val = _request->value(name, found);
  if (!found) {
    LOG_TOPIC(WARN, Logger::AGENCY) << "Mandatory query string " << name
                                    << " missing.";
    return false;
  }
  return true;
}
}

#endif
