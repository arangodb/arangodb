////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "RestSystemReportHandler.h"

#include "Agency/AgencyFeature.h"
#include "Agency/Agent.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"
#include "RestServer/ServerFeature.h"
#include "Utils/ExecContext.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include <fstream>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

std::mutex RestSystemReportHandler::_exclusive;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

RestSystemReportHandler::RestSystemReportHandler(
  application_features::ApplicationServer& server, GeneralRequest* request,
  GeneralResponse* response) :
  RestBaseHandler(server, request, response),
  cmds ({
      {"date", "time date -u \"+%Y-%m-%d %H:%M:%S %Z\" 2>&1"},
      {"dmesg", "time dmesg 2>&1"},
      {"df", "time df -h 2>&1"},
      {"memory", "time cat /proc/meminfo 2>&1"},
      {"uptime", "time uptime 2>&1"},
      {"uname", "time uname -a 2>&1"},
      {"topp", std::string("time top -b -n 1 -H -p ") +
          std::to_string(Thread::currentProcessId()) + " 2>&1"},
      {"top", "time top -b -n 1 2>&1"}
    }) {}

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
namespace {
std::string exec(std::string const& cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}
}
#endif

bool RestSystemReportHandler::isAdminUser() const {
  if (!ExecContext::isAuthEnabled()) {
    return true;
  } else {
    return ExecContext::current().isAdminUser();
  }
}

RestStatus RestSystemReportHandler::execute() {

  ServerSecurityFeature& security = server().getFeature<ServerSecurityFeature>();

  if (!security.canAccessHardenedApi()) {
    // dont leak information about server internals here
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
    return RestStatus::DONE;
  }

  if (_request->requestType() != RequestType::GET) {
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  using namespace std::chrono;
  auto start = steady_clock::now();

  VPackBuilder result;
  { VPackObjectBuilder o(&result);

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
    while (true) {

      if (steady_clock::now() - start > seconds(60)) {
        generateError(ResponseCode::BAD, TRI_ERROR_LOCK_TIMEOUT);
        return RestStatus::DONE;
      }

      if (!server().isStopping()) {
        // Allow only one simultaneous call
        std::unique_lock<std::mutex> exclusive(_exclusive, std::defer_lock);
        if (exclusive.try_lock()) {
          for (auto const& cmd : cmds) {
            if (server().isStopping()) {
              generateError(ResponseCode::BAD, TRI_ERROR_SHUTTING_DOWN);
              return RestStatus::DONE;
            }
            try {
              result.add(cmd.first, VPackValue(exec(cmd.second)));
            } catch (std::exception const& e) {
              result.add(cmd.first, VPackValue(e.what()));
            }
          }
          break;
        } else {
          std::this_thread::sleep_for(seconds(1));
        }
      } else {
        generateError(ResponseCode::BAD, TRI_ERROR_SHUTTING_DOWN);
        return RestStatus::DONE;
      }

    }

#else
    result.add("result", VPackValue("not supported on POSIX uncompliant systems"));
#endif

  } // result

  generateResult(rest::ResponseCode::OK, result.slice());
  return RestStatus::DONE;
}
