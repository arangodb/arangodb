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

#include "Runner.h"

#include <iostream>

#include <chrono>
#include <filesystem>
#include <numeric>
#include <memory>
#include <stdexcept>

#include "Logger/LogMacros.h"
#include "Inspection/VPack.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/Methods/Collections.h"

#include "Server.h"

namespace {
std::size_t getFolderSize(std::string_view path) {
  return std::accumulate(std::filesystem::recursive_directory_iterator(path),
                         std::filesystem::recursive_directory_iterator(), 0ull,
                         [](auto size, auto& path) {
                           return std::filesystem::is_directory(path)
                                      ? size
                                      : size + std::filesystem::file_size(path);
                         });
}
}  // namespace

namespace arangodb::sepp {

Runner::Runner(std::string_view executable, std::string_view reportFile,
               velocypack::Slice config)
    : _executable(executable), _reportFile(reportFile) {
  std::cout << config.toJson();
  velocypack::deserialize(config, _options, {.ignoreMissingFields = true});
}

Runner::~Runner() = default;

void Runner::run() {
  runBenchmark();
  // printSummary(report);
  // writeReport(report);
}

auto Runner::runBenchmark() -> Report {
  _server =
      std::make_unique<Server>(_options.rocksdb, _options.databaseDirectory);
  _server->start(_executable.data());

  VPackBuilder optionsBuilder;
  optionsBuilder.openObject();
  optionsBuilder.close();

  std::shared_ptr<LogicalCollection> collection;
  auto res = methods::Collections::create(
      *_server->vocbase(),     // collection vocbase
      {},                      // operation options
      "testcol",               // collection name
      TRI_COL_TYPE_DOCUMENT,   // collection type
      optionsBuilder.slice(),  // collection properties
      false,                   // replication wait flag
      false,                   // replication factor flag
      false,                   // new Database?, here always false
      collection);
  if (!res.ok()) {
    throw std::runtime_error("Failed to create collection: " +
                             std::string(res.errorMessage()));
  }

  auto start = std::chrono::steady_clock::now();
  auto end = start + std::chrono::seconds(5);
  std::size_t cnt = 0;
  bool stop = false;
  do {
    auto trx = std::make_unique<SingleCollectionTransaction>(
        transaction::StandaloneContext::Create(*_server->vocbase()), "testcol",
        AccessMode::Type::WRITE);

    VPackBuilder docBuilder;
    docBuilder.openObject();
    docBuilder.add("foo", VPackValue("bar"));
    docBuilder.close();

    trx->begin();
    trx->insert("testcol", docBuilder.slice(), {});
    trx->commit();

    if (++cnt % 128 == 0) {
      stop = std::chrono::steady_clock::now() > end;
    }
  } while (!stop);
  auto runtime = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - start)
                     .count() /
                 1000.0;

  LOG_DEVEL << "performed " << cnt << " operations in " << runtime << "s";
  LOG_DEVEL << "Throughput: " << cnt / runtime << "ops/s";
  LOG_DEVEL << "Size of database: "
            << getFolderSize(_options.databaseDirectory) / (1024.0) << "kb";

  /*auto rounds = _config.optional<std::uint32_t>("rounds").value_or(10);
  auto runtime = _config.optional<std::uint32_t>("runtime").value_or(10000);
  auto timestamp =
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

  std::vector<round_report> round_reports;
  round_reports.reserve(rounds);
  for (std::uint32_t i = 0; i < rounds; ++i) {
    std::cout << "round " << i << std::flush;
    auto report = exec_round(runtime);
    std::cout << " - " << static_cast<double>(report.operations()) /
  report.runtime << " ops/ms" << std::endl;
    round_reports.push_back(std::move(report));
  }

  return
  {_config.optional<std::string>("name").value_or(_config.as<std::string>("type")),
          timestamp.count(),
          _config,
          round_reports};*/
  return {};
}

}  // namespace arangodb::sepp