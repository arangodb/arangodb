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

#include "SslServerFeature.h"

#include "Basics/FileUtils.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Random/UniformCharacter.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

SslServerFeature* SslServerFeature::SSL = nullptr;

SslServerFeature::SslServerFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "SslServer"),
      _cafile(),
      _keyfile(),
      _sessionCache(false),
      _cipherList("HIGH:!EXPORT:!aNULL@STRENGTH"),
      _sslProtocol(TLS_V12),
      _sslOptions(asio::ssl::context::default_workarounds | asio::ssl::context::single_dh_use)
      {
  setOptional(true);
  startsAfter("AQLPhase");
}

void SslServerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOldOption("server.cafile", "ssl.cafile");
  options->addOldOption("server.keyfile", "ssl.keyfile");
  options->addOldOption("server.ssl-cache", "ssl.session-cache");
  options->addOldOption("server.ssl-cipher-list", "ssl.cipher-list");
  options->addOldOption("server.ssl-options", "ssl.options");
  options->addOldOption("server.ssl-protocol", "ssl.protocol");

  options->addSection("ssl", "Configure SSL communication");

  options->addOption("--ssl.cafile", "ca file used for secure connections",
                     new StringParameter(&_cafile));

  options->addOption("--ssl.keyfile", "key-file used for secure connections",
                     new StringParameter(&_keyfile));

  options->addOption("--ssl.session-cache",
                     "enable the session cache for connections",
                     new BooleanParameter(&_sessionCache));

  options->addOption("--ssl.cipher-list",
                     "ssl ciphers to use, see OpenSSL documentation",
                     new StringParameter(&_cipherList));

  std::unordered_set<uint64_t> sslProtocols = {1, 2, 3, 4, 5};

  options->addOption("--ssl.protocol",
                     "ssl protocol (1 = SSLv2, 2 = SSLv2 or SSLv3 (negotiated), 3 = SSLv3, 4 = "
                     "TLSv1, 5 = TLSv1.2)",
                     new DiscreteValuesParameter<UInt64Parameter>(
                         &_sslProtocol, sslProtocols));

  options->addHiddenOption("--ssl.options",
                           "ssl connection options, see OpenSSL documentation",
                           new UInt64Parameter(&_sslOptions));

  options->addOption(
      "--ssl.ecdh-curve",
      "SSL ECDH Curve, see the output of \"openssl ecparam -list_curves\"",
      new StringParameter(&_ecdhCurve));
}

void SslServerFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // check for SSLv2
  if (_sslProtocol == 1) {
    LOG_TOPIC(FATAL, arangodb::Logger::SSL) << "SSLv2 is not supported any longer because of security vulnerabilities in this protocol";
    FATAL_ERROR_EXIT();
  }
}

void SslServerFeature::prepare() {
  LOG_TOPIC(INFO, arangodb::Logger::SSL) << "using SSL options: "
                                         << stringifySslOptions(_sslOptions);

  if (!_cipherList.empty()) {
    LOG_TOPIC(INFO, arangodb::Logger::SSL) << "using SSL cipher-list '"
                                           << _cipherList << "'";
  }

  SSL = this;
}

void SslServerFeature::createSslContext() {
  try
  {
    _context = arangodb::sslContext(SslProtocol(_sslProtocol), _keyfile);

    asio::ssl::context::native_handle_type nativeContext =
      _context->native_handle();

    // set cache mode
    SSL_CTX_set_session_cache_mode(nativeContext, _sessionCache
                                                      ? SSL_SESS_CACHE_SERVER
                                                      : SSL_SESS_CACHE_OFF);
    if (_sessionCache) {
      LOG_TOPIC(TRACE, arangodb::Logger::SSL) << "using SSL session caching";
    }

    // set cipher list
    if (!_cipherList.empty()) {
      if (SSL_CTX_set_cipher_list(nativeContext, _cipherList.c_str()) != 1) {
        LOG_TOPIC(ERR, arangodb::Logger::SSL) << "cannot set SSL cipher list '"
                                              << _cipherList
                                              << "': " << lastSSLError();
        throw std::runtime_error("cannot create SSL context");
      }
    }


    // This code was
#if OPENSSL_VERSION_NUMBER >= 0x0090800fL
    if (!_ecdhCurve.empty()) {
      int sslEcdhNid = OBJ_sn2nid(_ecdhCurve.c_str());

      if (sslEcdhNid == 0) {
        LOG_TOPIC(ERR, arangodb::Logger::SSL)
            << "SSL error: " << lastSSLError()
            << " Unknown curve name: " << _ecdhCurve;
        throw std::runtime_error("cannot create SSL context");
      }

      // https://www.openssl.org/docs/man1.1.0/crypto/EC_KEY_new_by_curve_name.html
      EC_KEY* ecdhKey = EC_KEY_new_by_curve_name(sslEcdhNid);
      if (ecdhKey == nullptr) {
        LOG_TOPIC(ERR, arangodb::Logger::SSL)
            << "SSL error: " << lastSSLError()
            << ". unable to create curve by name: " << _ecdhCurve;
        throw std::runtime_error("cannot create SSL context");
      }

      if (SSL_CTX_set_tmp_ecdh(nativeContext, ecdhKey) != 1) {
        EC_KEY_free(ecdhKey);
        LOG_TOPIC(ERR, arangodb::Logger::SSL)
            << "cannot set ECDH option" << lastSSLError();
        throw std::runtime_error("cannot create SSL context");
      }

      EC_KEY_free(ecdhKey);
      SSL_CTX_set_options(nativeContext, SSL_OP_SINGLE_ECDH_USE);
    }
#endif

    _context->set_options(_sslOptions);
    _context->use_private_key_file(_keyfile, asio::ssl::context::pem);
    _context->set_verify_mode(SSL_VERIFY_NONE);
    if (!_cafile.empty()) {
      _context->use_certificate_file(_cafile, asio::ssl::context::pem);
    } else {
      _context->use_certificate_file(_keyfile, asio::ssl::context::pem);
    }

  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::SSL)
        << "failed to create SSL context: " << ex.what();
    throw std::runtime_error("cannot create SSL context");
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::SSL)
        << "failed to create SSL context, cannot create HTTPS server";
    throw std::runtime_error("cannot create SSL context");
  }
}

void SslServerFeature::unprepare() {
  LOG_TOPIC(TRACE, arangodb::Logger::SSL) << "unpreparing ssl: "
                                          << stringifySslOptions(_sslOptions);
  _context.reset();
}

// called from GeneralServer::start
void SslServerFeature::verifySslOptions() {
  // check keyfile
  if (_keyfile.empty()) {
    LOG_TOPIC(FATAL, arangodb::Logger::SSL) << "no value specified for '--ssl.keyfile'";
    FATAL_ERROR_EXIT();
  }

  // validate protocol
  if (_sslProtocol <= SSL_UNKNOWN || _sslProtocol >= SSL_LAST) {
    LOG_TOPIC(FATAL, arangodb::Logger::SSL)
        << "invalid SSL protocol version specified. Please use a valid "
           "value for '--ssl.protocol'";
    FATAL_ERROR_EXIT();
  }

  LOG_TOPIC(DEBUG, arangodb::Logger::SSL)
      << "using SSL protocol version '"
      << protocolName(SslProtocol(_sslProtocol)) << "'";

  if (!FileUtils::exists(_keyfile)) {
    LOG_TOPIC(FATAL, arangodb::Logger::SSL) << "unable to find SSL keyfile '"
                                            << _keyfile << "'";
    FATAL_ERROR_EXIT();
  }

  createSslContext();
}
