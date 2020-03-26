////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_REST_HANDLER_REST_AGENCY_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_AGENCY_HANDLER_H 1

#include "Agency/Agent.h"
#include "Futures/Future.h"
#include "Futures/Unit.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

namespace arangodb {

// String to number conversions:

template <class T>
struct stp;

template <>
struct stp<uint64_t> {
  static uint64_t convert(std::string const& s) { return std::stoull(s); }
};
template <>
struct stp<int64_t> {
  static uint64_t convert(std::string const& s) { return std::stoll(s); }
};
template <>
struct stp<int32_t> {
  static long convert(std::string const& s) { return std::stol(s); }
};
template <>
struct stp<uint32_t> {
  static uint64_t convert(std::string const& s) { return std::stoul(s); }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief REST handler for outside calls to agency (write & read)
////////////////////////////////////////////////////////////////////////////////

class RestAgencyHandler : public RestVocbaseBaseHandler {
 public:
  RestAgencyHandler(application_features::ApplicationServer&, GeneralRequest*,
                    GeneralResponse*, consensus::Agent*);

 public:
  char const* name() const override final { return "RestAgencyHandler"; }

  RequestLane lane() const override final {
    return RequestLane::AGENCY_CLUSTER;
  }

  RestStatus execute() override;

  using fvoid = futures::Future<futures::Unit>;

  RestStatus pollIndex(consensus::index_t index);

 private:
  template <class T>
  inline bool readValue(char const* name, T& val) const {
    bool found = true;
    std::string const& val_str = _request->value(name, found);

    if (!found) {
      LOG_TOPIC("f4732", DEBUG, Logger::AGENCY) << "Mandatory query string " << name << " missing.";
      return false;
    } else {
      try {
        val = stp<T>::convert(val_str);
      } catch (std::invalid_argument const&) {
        LOG_TOPIC("c7aeb", WARN, Logger::AGENCY) << "Value for query string "
                                        << name << " cannot be converted to integral type";
        return false;
      } catch (std::out_of_range const&) {
        LOG_TOPIC("59881", WARN, Logger::AGENCY)
            << "Value for query string " << name
            << " does not fit into range of integral type";
        return false;
      }
    }
    return true;
  }

  RestStatus reportErrorEmptyRequest();
  RestStatus reportTooManySuffices();
  RestStatus reportUnknownMethod();
  RestStatus reportMessage(arangodb::rest::ResponseCode, std::string const&);
  RestStatus handleStores();
  RestStatus handleStore();
  RestStatus handleRead();
  RestStatus handleWrite();
  RestStatus handleTransact();
  RestStatus handleConfig();
  RestStatus reportMethodNotAllowed();
  RestStatus handleState();
  RestStatus handleTransient();
  RestStatus handleInquire();
  RestStatus handlePoll();

  void redirectRequest(std::string const& leaderId);
  consensus::Agent* _agent;
};
}  // namespace arangodb

#endif
