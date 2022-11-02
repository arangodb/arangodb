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

#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <memory>
#include <stdexcept>

#include "Inspection/VPack.h"

#include "Basics/overload.h"
#include "Execution.h"
#include "Server.h"
#include "Workloads/GetByPrimaryKey.h"
#include "Workloads/InsertDocuments.h"
#include "Workloads/IterateDocuments.h"
#include "velocypack/Collection.h"
#include "velocypack/Parser.h"

#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"

namespace {
std::size_t getFolderSize(std::string_view path) {
  return std::accumulate(std::filesystem::recursive_directory_iterator(path),
                         std::filesystem::recursive_directory_iterator(), 0ull,
                         [](auto size, auto const& path) {
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
  velocypack::deserializeUnsafe(config, _options);
}

Runner::~Runner() = default;

void Runner::run() {
  auto report = runBenchmark();
  printSummary(report);
  writeReport(report);
}

auto Runner::runBenchmark() -> Report {
  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch());

  startServer();
  setup();

  std::cout << "Running benchmark...\n";
  auto workload = std::visit(
      overload{[](workloads::GetByPrimaryKey::Options& opts)
                   -> std::shared_ptr<Workload> {
                 return std::make_shared<workloads::GetByPrimaryKey>(opts);
               },
               [](workloads::InsertDocuments::Options& opts)
                   -> std::shared_ptr<Workload> {
                 return std::make_shared<workloads::InsertDocuments>(opts);
               },
               [](workloads::IterateDocuments::Options& opts)
                   -> std::shared_ptr<Workload> {
                 return std::make_shared<workloads::IterateDocuments>(opts);
               }},
      _options.workload);

  Execution exec(_options, workload);
  exec.createThreads(*_server);
  auto report = exec.run();
  report.timestamp = timestamp.count();

  // we need to stop the server before we calculate the size of the DB folder
  // because otherwise RocksDB might still write/delete some files
  _server.reset();

  report.databaseSize = getFolderSize(_options.databaseDirectory);

  return report;
}

void Runner::printSummary(Report const& report) {
  std::cout << "Summary:\n"
            << "  runtime: " << report.runtime << " ms\n"
            << "  operations: " << report.operations() << " ops\n"
            << "  throughput: " << report.throughput() << " ops/ms"
            << std::endl;
}

void Runner::writeReport(Report const& report) {
  if (_reportFile.empty()) {
    return;
  }

  velocypack::Builder reportBuilder;
  reportBuilder.openArray();

  std::ifstream oldReportFile(_reportFile);
  if (oldReportFile.is_open()) {
    try {
      std::stringstream buffer;
      buffer << oldReportFile.rdbuf();

      auto oldReport = arangodb::velocypack::Parser::fromJson(buffer.str());
      if (oldReport) {
        velocypack::Collection::appendArray(reportBuilder, oldReport->slice());
      }
    } catch (std::exception const& e) {
      std::cerr << "Failed to parse existing report file \"" << _reportFile
                << "\" - " << e.what() << "\nSkipping report generation!"
                << std::endl;
      return;
    }
  }

  velocypack::serialize(reportBuilder, report);
  reportBuilder.close();

  std::ofstream out(_reportFile);
  out << reportBuilder.slice().toString();
}

void Runner::startServer() {
  if (_options.clearDatabaseDirectory) {
    std::filesystem::remove_all(_options.databaseDirectory);
  }
  _server =
      std::make_unique<Server>(_options.rocksdb, _options.databaseDirectory);
  _server->start(_executable.data());
}

void Runner::setup() {
  std::cout << "Setting up collections\n";
  for (auto& col : _options.setup.collections) {
    auto collection = createCollection(col.name);
    for (auto& idx : col.indexes) {
      createIndex(*collection, idx);
    }
  }

  std::cout << "Running prefill...\n";
  for (auto& opts : _options.setup.prefill) {
    auto workload = std::make_shared<workloads::InsertDocuments>(opts.second);
    Execution exec(_options, workload);
    exec.createThreads(*_server);
    std::ignore = exec.run();  // TODO - do we need the report?
  }
}

auto Runner::createCollection(std::string const& name)
    -> std::shared_ptr<LogicalCollection> {
  VPackBuilder optionsBuilder;
  optionsBuilder.openObject();
  optionsBuilder.close();

  std::shared_ptr<LogicalCollection> collection;
  auto res = methods::Collections::create(
      *_server->vocbase(),     // collection vocbase
      {},                      // operation options
      name,                    // collection name
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
  return collection;
}

void Runner::createIndex(LogicalCollection& col, IndexSetup const& index) {
  VPackBuilder builder;
  builder.openObject();
  builder.add("type", index.type);
  builder.add(VPackValue("fields"));
  builder.openArray();
  for (auto& f : index.fields) {
    builder.add(VPackValue(f));
  }
  builder.close();
  builder.close();
  bool created = false;
  std::ignore = col.createIndex(builder.slice(), created);
}

}  // namespace arangodb::sepp
