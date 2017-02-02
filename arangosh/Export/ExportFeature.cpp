////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Baesler
////////////////////////////////////////////////////////////////////////////////

#include "ExportFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::options;

ExportFeature::ExportFeature(application_features::ApplicationServer* server,
                             int* result)
    : ApplicationFeature(server, "Export"),
      _result(result) {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter("Client");
  startsAfter("Config");
  startsAfter("Logger");
}

void ExportFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
}

void ExportFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  auto const& positionals = options->processingResult()._positionals;
  size_t n = positionals.size();

/*  if (1 == n) {
    // only take positional file name attribute into account if user
    // did not specify the --file option as well
    if (!options->processingResult().touched("--file")) {
      _filename = positionals[0];
    }
  } else if (1 < n) {
    LOG(FATAL) << "expecting at most one filename, got " +
                      StringUtils::join(positionals, ", ");
    FATAL_ERROR_EXIT();
  } */
}

void ExportFeature::start() {
  ClientFeature* client = application_features::ApplicationServer::getFeature<ClientFeature>("Client");

  int ret = EXIT_SUCCESS;
  *_result = ret;

  std::unique_ptr<SimpleHttpClient> httpClient;

  try {
    httpClient = client->createHttpClient();
  } catch (...) {
    LOG(FATAL) << "cannot create server connection, giving up!";
    FATAL_ERROR_EXIT();
  }

  httpClient->setLocationRewriter(static_cast<void*>(client), &rewriteLocation);
  httpClient->setUserNamePassword("/", client->username(), client->password());

  // must stay here in order to establish the connection
  httpClient->getServerVersion();

  if (!httpClient->isConnected()) {
    LOG(ERR) << "Could not connect to endpoint '" << client->endpoint()
             << "', database: '" << client->databaseName() << "', username: '"
             << client->username() << "'";
    LOG(FATAL) << httpClient->getErrorMessage() << "'";
    FATAL_ERROR_EXIT();
  }

  // successfully connected
  std::cout << "Connected to ArangoDB '"
            << httpClient->getEndpointSpecification() << "', version "
            << httpClient->getServerVersion() << ", database: '"
            << client->databaseName() << "', username: '" << client->username()
            << "'" << std::endl;

  
  collectionExport(httpClient.get());
  graphExport(httpClient.get());

  *_result = ret;
}

void ExportFeature::collectionExport(SimpleHttpClient* httpClient) {

}

void ExportFeature::graphExport(SimpleHttpClient* httpClient) {
  
}