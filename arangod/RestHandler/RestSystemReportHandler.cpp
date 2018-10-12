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

#include "RestSystemReportHandler.h"

#include "Agency/AgencyFeature.h"
#include "Agency/Agent.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ServerState.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"
#include "RestServer/ServerFeature.h"

#if defined(TRI_HAVE_POSIX_THREADS)
#include <unistd.h>
#endif

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

RestSystemReportHandler::RestSystemReportHandler(GeneralRequest* request,
                                     GeneralResponse* response)
    : RestBaseHandler(request, response) {}

std::string executeOSCmd(std::string const& cmd, std::string const& optional) {
  static std::string const tmpFile("/tmp/arango-inspect.tmp");
  static std::string const pipe(" | tee ");
  static std::string const terminate(" > /dev/null");
  std::system ((cmd + optional + pipe + tmpFile + terminate).c_str());  
  std::ifstream tmpStream(tmpFile.c_str());
  return std::string(
    std::istreambuf_iterator<char>(tmpStream), std::istreambuf_iterator<char>());
}

RestStatus RestSystemReportHandler::execute() {
  
  std::unordered_map<std::string> cmds{
    {"date", "date -u \"+%Y-%m-%d %H:%M:%S %Z\""}, {"dmesg", "dmesg"},
    {"df", "df -h"}, {"memory", "cat /proc/meminfo"}, {"uptime", "uptime"},
    {"uname", "uname -a"}, {"top", std::string("top -b -n 1 -H -p ") + Thread::currentProcessId()}};
  
  
  VPackBuilder result;
  { VPackObjectBuilder o(&result);
    for (auto cmd : cmds) {
      try {
        result.add(cmd.first, VPackValue(executeOSCmd(cmd.second)));
      } catch (std::exception const& e) {
        result.add(cmd.first, VPackValue(e.what()));
      }
    }}
  
  generateResult(rest::ResponseCode::OK, result.slice());
  return RestStatus::DONE;
}
