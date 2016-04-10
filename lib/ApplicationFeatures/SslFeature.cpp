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

#include <openssl/err.h>

#include "Basics/FileUtils.h"
#include "Basics/UniformCharacter.h"
#include "Basics/ssl-helper.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

SslFeature::SslFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Ssl"),
      _cafile(),
      _keyfile(),
      _sessionCache(false),
      _cipherList(),
      _protocol(TLS_V1),
      _options(
          (long)(SSL_OP_TLS_ROLLBACK_BUG | SSL_OP_CIPHER_SERVER_PREFERENCE)),
      _sslContext(nullptr) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
}

void SslFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::collectOptions";

  options->addSection("ssl", "Configure SSL communication");

  options->addOption("--ssl.cafile", "ca file used for secure connections",
                     new StringParameter(&_cafile));

  options->addOption("--ssl.keyfile", "key-file used for secure connections",
                     new StringParameter(&_keyfile));

  options->addOption("--ssl.session-cache",
                     "enable the session cache for connections",
                     new BooleanParameter(&_sessionCache));

  options->addOption("--ssl.chipher-list",
                     "ssl chipers to use, see OpenSSL documentation",
                     new StringParameter(&_cipherList));

  std::unordered_set<uint64_t> sslProtocols = {1, 2, 3, 4};

  options->addOption("--ssl.protocol",
                     "ssl protocol (1 = SSLv2, 2 = SSLv23, 3 = SSLv3, 4 = "
                     "TLSv1, 5 = TLSV1.2 (recommended)",
                     new UInt64Parameter(&_protocol));

  options->addHiddenOption(
      "--ssl.options", "ssl connection options, see OpenSSL documentation",
      new DiscreteValuesParameter<UInt64Parameter>(&_options, sslProtocols));
}

void SslFeature::prepare() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::prepare";

  createSslContext();
}

void SslFeature::stop() {
  if (_sslContext != nullptr) {
    SSL_CTX_free(_sslContext);
    _sslContext = nullptr;
  }
}

namespace {
class BIOGuard {
 public:
  explicit BIOGuard(BIO* bio) : _bio(bio) {}

  ~BIOGuard() { BIO_free(_bio); }

 public:
  BIO* _bio;
};
}

void SslFeature::createSslContext() {
  // check keyfile
  if (_keyfile.empty()) {
    return;
  }

  // validate protocol
  if (_protocol <= SSL_UNKNOWN || _protocol >= SSL_LAST) {
    LOG(FATAL) << "invalid SSL protocol version specified. Please use a valid "
                  "value for '--ssl.protocol.'";
    FATAL_ERROR_EXIT();
  }

  LOG(DEBUG) << "using SSL protocol version '"
             << protocolName((protocol_e)_protocol) << "'";

  if (!FileUtils::exists(_keyfile)) {
    LOG(FATAL) << "unable to find SSL keyfile '" << _keyfile << "'";
    FATAL_ERROR_EXIT();
  }

  // create context
  _sslContext = ::sslContext(protocol_e(_protocol), _keyfile);

  if (_sslContext == nullptr) {
    LOG(FATAL) << "failed to create SSL context, cannot create HTTPS server";
    FATAL_ERROR_EXIT();
  }

  // set cache mode
  SSL_CTX_set_session_cache_mode(
      _sslContext, _sessionCache ? SSL_SESS_CACHE_SERVER : SSL_SESS_CACHE_OFF);

  if (_sessionCache) {
    LOG(TRACE) << "using SSL session caching";
  }

  // set options
  SSL_CTX_set_options(_sslContext, (long)_options);

  LOG(INFO) << "using SSL options: " << _options;

  if (!_cipherList.empty()) {
    if (SSL_CTX_set_cipher_list(_sslContext, _cipherList.c_str()) != 1) {
      LOG(FATAL) << "cannot set SSL cipher list '" << _cipherList
                 << "': " << lastSSLError();
      FATAL_ERROR_EXIT();
    } else {
      LOG(INFO) << "using SSL cipher-list '" << _cipherList << "'";
    }
  }

  // set ssl context
  UniformCharacter r(
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
  _rctx = r.random(SSL_MAX_SSL_SESSION_ID_LENGTH);

  int res = SSL_CTX_set_session_id_context(
      _sslContext, (unsigned char const*)_rctx.c_str(), (int)_rctx.size());

  if (res != 1) {
    LOG(FATAL) << "cannot set SSL session id context '" << _rctx
               << "': " << lastSSLError();
    FATAL_ERROR_EXIT();
  }

  // check CA
  if (!_cafile.empty()) {
    LOG(TRACE) << "trying to load CA certificates from '" << _cafile << "'";

    int res = SSL_CTX_load_verify_locations(_sslContext, _cafile.c_str(), 0);

    if (res == 0) {
      LOG(FATAL) << "cannot load CA certificates from '" << _cafile
                 << "': " << lastSSLError();
      FATAL_ERROR_EXIT();
    }

    STACK_OF(X509_NAME) * certNames;

    certNames = SSL_load_client_CA_file(_cafile.c_str());

    if (certNames == nullptr) {
      LOG(FATAL) << "cannot load CA certificates from '" << _cafile
                 << "': " << lastSSLError();
      FATAL_ERROR_EXIT();
    }

    if (Logger::logLevel() == arangodb::LogLevel::TRACE) {
      for (int i = 0; i < sk_X509_NAME_num(certNames); ++i) {
        X509_NAME* cert = sk_X509_NAME_value(certNames, i);

        if (cert) {
          BIOGuard bout(BIO_new(BIO_s_mem()));

          X509_NAME_print_ex(bout._bio, cert, 0,
                             (XN_FLAG_SEP_COMMA_PLUS | XN_FLAG_DN_REV |
                              ASN1_STRFLGS_UTF8_CONVERT) &
                                 ~ASN1_STRFLGS_ESC_MSB);

          char* r;
          long len = BIO_get_mem_data(bout._bio, &r);

          LOG(TRACE) << "name: " << std::string(r, len);
        }
      }
    }

    SSL_CTX_set_client_CA_list(_sslContext, certNames);
  }
}
