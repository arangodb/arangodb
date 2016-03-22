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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/SslFeature.h"

#include "Basics/ssl-helper.h"
#include "Logger/Logger.h"
#include "ProgramOptions2/ProgramOptions.h"
#include "ProgramOptions2/Section.h"

using namespace arangodb;
using namespace arangodb::options;

SslFeature::SslFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Ssl"),
      _cafile(),
      _keyfile(),
      _sessionCache(false),
      _chiperList(),
      _protocol(TLS_V1),
      _options(
          (long)(SSL_OP_TLS_ROLLBACK_BUG | SSL_OP_CIPHER_SERVER_PREFERENCE)) {
  setOptional(true);
  requiresElevatedPrivileges(true);
  startsAfter("Logger");
}

void SslFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::collectOptions";

  options->addSection(Section("ssl", "Configure SSL communication",
                              "ssl options", false, false));

  options->addOption("--ssl.cafile", "ca file used for secure connections",
                     new StringParameter(&_cafile));

  options->addOption("--ssl.keyfile", "key-file used for secure connections",
                     new StringParameter(&_keyfile));

  options->addOption("--ssl.session-cache",
                     "enable the session cache for connections",
                     new BooleanParameter(&_sessionCache));

  options->addOption("--ssl.chipher-list",
                     "ssl chipers to use, see OpenSSL documentation",
                     new StringParameter(&_chiperList));

  std::unordered_set<uint64_t> sslProtocols = {1, 2, 3, 4};

  options->addOption(
      "--ssl.protocol",
      "ssl protocol (1 = SSLv2, 2 = SSLv23, 3 = SSLv3, 4 = TLSv1)",
      new UInt64Parameter(&_protocol));

  options->addHiddenOption(
      "--ssl.options", "ssl connection options, see OpenSSL documentation",
      new DiscreteValuesParameter<UInt64Parameter>(&_options, sslProtocols));
}
