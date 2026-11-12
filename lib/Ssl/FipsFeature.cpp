////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "FipsFeature.h"

#include "ApplicationFeatures/GreetingsFeaturePhase.h"

#ifdef USE_FIPS
#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"

#define OPENSSL_THREAD_DEFINES
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/opensslconf.h>
#include <openssl/provider.h>
#include <openssl/ssl.h>

#ifndef OPENSSL_THREADS
#error missing thread support for openssl, please recomple OpenSSL with threads
#endif
#endif  // USE_FIPS

using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

FipsFeature::FipsFeature(application_features::ApplicationServer& server,
                         FipsFeatureOptions options)
    : application_features::ApplicationFeature(server, *this),
      _options(std::move(options)) {
  setOptional(true);
  startsAfter<application_features::GreetingsFeaturePhase>();
}

void FipsFeature::prepare() {
#ifdef USE_FIPS
  if (_options.enableFips) {
    if (OPENSSL_init_crypto(OPENSSL_INIT_NO_LOAD_CONFIG, nullptr) != 1 ||
        OPENSSL_init_ssl(OPENSSL_INIT_NO_LOAD_CONFIG, nullptr) != 1) {
      LOG_TOPIC("a1b2c", FATAL, Logger::SSL) << "Failed to initialize OpenSSL";
      FATAL_ERROR_EXIT();
    }

    // Fixed module location, independent of OPENSSL_MODULES.
    if (OSSL_PROVIDER_set_default_search_path(nullptr,
                                              "/opt/lib/ossl-modules") != 1) {
      LOG_TOPIC("b2c3d", FATAL, Logger::SSL)
          << "Failed to set OpenSSL module search path";
      FATAL_ERROR_EXIT();
    }

    if (OSSL_LIB_CTX_load_config(nullptr, "/opt/ssl/fips-only.cnf") != 1) {
      LOG_TOPIC("40a8f", FATAL, Logger::SSL)
          << "Failed to load OpenSSL FIPS configuration. "
          << "Ensure /opt/ssl/fips-only.cnf and fipsmodule.cnf are set up.";
      FATAL_ERROR_EXIT();
    }

    _fipsProvider = OSSL_PROVIDER_load(nullptr, "fips");
    if (_fipsProvider == nullptr) {
      LOG_TOPIC("c2e21", FATAL, Logger::SSL)
          << "Failed to load OpenSSL FIPS provider. "
          << "Ensure fipsmodule.cnf has been generated";
      FATAL_ERROR_EXIT();
    }

    _baseProvider = OSSL_PROVIDER_load(nullptr, "base");
    if (_baseProvider == nullptr) {
      LOG_TOPIC("ef067", FATAL, Logger::SSL)
          << "Failed to load OpenSSL base provider";
      FATAL_ERROR_EXIT();
    }

    if (EVP_default_properties_enable_fips(nullptr, 1) != 1) {
      LOG_TOPIC("ef3c2", FATAL, Logger::SSL)
          << "Failed to enable FIPS as default OpenSSL property";
      FATAL_ERROR_EXIT();
    }
    LOG_TOPIC("eb4c5", INFO, Logger::SSL)
        << "FIPS module was loaded successfully.";
  }
#endif  // USE_FIPS
}

void FipsFeature::unprepare() {
#ifdef USE_FIPS
  if (_baseProvider != nullptr) {
    OSSL_PROVIDER_unload(_baseProvider);
    _baseProvider = nullptr;
  }
  if (_fipsProvider != nullptr) {
    OSSL_PROVIDER_unload(_fipsProvider);
    _fipsProvider = nullptr;
  }
#endif  // USE_FIPS
}

}  // namespace arangodb
