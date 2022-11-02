////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <string_view>

#include <velocypack/Slice.h>

#include "Options.h"
#include "Report.h"

namespace arangodb {
class LogicalCollection;
}

namespace arangodb::sepp {

struct Server;

class Runner {
 public:
  Runner(std::string_view executable, std::string_view reportFile,
         velocypack::Slice config);
  ~Runner();
  void run();

 private:
  void startServer();
  void setup();
  auto createCollection(std::string const& name)
      -> std::shared_ptr<LogicalCollection>;
  void createIndex(LogicalCollection& col, IndexSetup const& index);
  auto runBenchmark() -> Report;
  void printSummary(Report const& report);
  void writeReport(Report const& report);

  std::string_view _executable;
  std::string _reportFile;

  Options _options;
  std::unique_ptr<Server> _server;
};

}  // namespace arangodb::sepp