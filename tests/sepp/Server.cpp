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

#include "Server.h"

#include <chrono>
#include <cstdlib>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <type_traits>

#include "RestServer/arangod_includes.h"

namespace {

using namespace arangodb;
using namespace arangodb::application_features;

constexpr auto kNonServerFeatures =
    std::array{ArangodServer::id<ActionFeature>(),
               ArangodServer::id<AgencyFeature>(),
               ArangodServer::id<ClusterFeature>(),
#ifdef ARANGODB_HAVE_FORK
               ArangodServer::id<SupervisorFeature>(),
               ArangodServer::id<DaemonFeature>(),
#endif
               ArangodServer::id<FoxxFeature>(),
               ArangodServer::id<GeneralServerFeature>(),
               ArangodServer::id<GreetingsFeature>(),
               ArangodServer::id<HttpEndpointProvider>(),
               ArangodServer::id<LogBufferFeature>(),
               ArangodServer::id<pregel::PregelFeature>(),
               ArangodServer::id<ServerFeature>(),
               ArangodServer::id<SslServerFeature>(),
               ArangodServer::id<StatisticsFeature>()};

}  // namespace

namespace arangodb::sepp {

struct Server::Impl {
  Impl(RocksDBOptionsProvider const& optionsProvider,
       std::string databaseDirectory);
  ~Impl();

  void start(char const* exectuable);

  TRI_vocbase_t* vocbase() { return _vocbase; }

 private:
  void setupServer(std::string const& name, int& result);
  void runServer(char const* exectuable);

  std::shared_ptr<arangodb::options::ProgramOptions> _options;
  RocksDBOptionsProvider const& _optionsProvider;
  std::string _databaseDirectory;
  ::ArangodServer _server;
  std::thread _serverThread;
  TRI_vocbase_t* _vocbase = nullptr;
};

Server::Impl::Impl(RocksDBOptionsProvider const& optionsProvider,
                   std::string databaseDirectory)
    : _options(std::make_shared<arangodb::options::ProgramOptions>("sepp", "",
                                                                   "", "")),
      _optionsProvider(optionsProvider),
      _databaseDirectory(std::move(databaseDirectory)),
      _server(_options, "") {
  std::string name = "arangod";  // we simply reuse the arangod config
  int ret = EXIT_FAILURE;
  setupServer(name, ret);
}

Server::Impl::~Impl() {
  if (_vocbase) {
    _vocbase->release();
    _vocbase = nullptr;
  }
  _server.beginShutdown();
  if (_serverThread.joinable()) {
    _serverThread.join();
  }
}

void Server::Impl::start(char const* exectuable) {
  _serverThread = std::thread(&Impl::runServer, this, exectuable);
  using arangodb::application_features::ApplicationServer;

  // wait for server
  unsigned count = 0;
  while (_server.state() < ApplicationServer::State::IN_WAIT && count < 20) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  if (_server.state() != ApplicationServer::State::IN_WAIT) {
    throw std::runtime_error("Failed to initialize ApplicationServer");
  }

  DatabaseFeature& databaseFeature = _server.getFeature<DatabaseFeature>();
  _vocbase = databaseFeature.useDatabase("_system");
}

void Server::Impl::setupServer(std::string const& name, int& result) {
  _server.addReporter(
      {[&](ArangodServer::State state) {
         if (state == ArangodServer::State::IN_START) {
           // drop privileges before starting features
           _server.getFeature<PrivilegeFeature>().dropPrivilegesPermanently();
         }
       },
       {}});

  _server.addFeatures(Visitor{
      []<typename T>(auto& server, TypeTag<T>) {
        return std::make_unique<T>(server);
      },
      [](auto& server, TypeTag<GreetingsFeaturePhase>) {
        return std::make_unique<GreetingsFeaturePhase>(server,
                                                       std::false_type{});
      },
      [&result](auto& server, TypeTag<CheckVersionFeature>) {
        return std::make_unique<CheckVersionFeature>(server, &result,
                                                     kNonServerFeatures);
      },
      [&name](auto& server, TypeTag<ConfigFeature>) {
        return std::make_unique<ConfigFeature>(server, name);
      },
      [](auto& server, TypeTag<InitDatabaseFeature>) {
        return std::make_unique<InitDatabaseFeature>(server,
                                                     kNonServerFeatures);
      },
      [](auto& server, TypeTag<LoggerFeature>) {
        return std::make_unique<LoggerFeature>(server, true);
      },
      [this](auto& server, TypeTag<RocksDBEngine>) {
        return std::make_unique<RocksDBEngine>(server, _optionsProvider);
      },
      [&result](auto& server, TypeTag<ScriptFeature>) {
        return std::make_unique<ScriptFeature>(server, &result);
      },
      [&result](auto& server, TypeTag<ServerFeature>) {
        return std::make_unique<ServerFeature>(server, &result);
      },
      [](auto& server, TypeTag<ShutdownFeature>) {
        return std::make_unique<ShutdownFeature>(
            server, std::array{ArangodServer::id<ScriptFeature>()});
      },
      [&name](auto& server, TypeTag<TempFeature>) {
        return std::make_unique<TempFeature>(server, name);
      },
      [](auto& server, TypeTag<SslServerFeature>) {
#ifdef USE_ENTERPRISE
        return std::make_unique<SslServerFeatureEE>(server);
#else
        return std::make_unique<SslServerFeature>(server);
#endif
      },
      [&result](auto& server, TypeTag<UpgradeFeature>) {
        return std::make_unique<UpgradeFeature>(server, &result,
                                                kNonServerFeatures);
      },
      [](auto& server, TypeTag<HttpEndpointProvider>) {
        return std::make_unique<EndpointFeature>(server);
      }});
}

void Server::Impl::runServer(char const* exectuable) {
  std::vector<std::string> args{exectuable, "--database.directory",
                                _databaseDirectory, "--server.endpoint",
                                "tcp://127.0.0.1:8530"};
  std::vector<char*> argv;
  for (auto& arg : args) {
    argv.push_back(arg.data());
  }
  ArangoGlobalContext context(argv.size(), argv.data(), "");
  ServerState::reset();
  ServerState state{_server};
  try {
    _server.run(argv.size(), argv.data());
  } catch (std::exception const& ex) {
    LOG_TOPIC("5d508", ERR, arangodb::Logger::FIXME)
        << "sepp ArangodServer terminated because of an exception: "
        << ex.what();
  } catch (...) {
    LOG_TOPIC("3c63a", ERR, arangodb::Logger::FIXME)
        << "sepp ArangodServer terminated because of an exception of "
           "unknown type";
  }
  arangodb::Logger::flush();
}

Server::Server(arangodb::RocksDBOptionsProvider const& optionsProvider,
               std::string databaseDirectory)
    : _impl(std::make_unique<Impl>(optionsProvider,
                                   std::move(databaseDirectory))) {}

Server::~Server() = default;

void Server::start(char const* exectuable) { _impl->start(exectuable); }

TRI_vocbase_t* Server::vocbase() { return _impl->vocbase(); }

}  // namespace arangodb::sepp
