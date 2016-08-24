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
#include "Basics/locks.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Random/UniformCharacter.h"
#include "Ssl/ssl-helper.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

SslServerFeature::SslServerFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "SslServer"),
      _cafile(),
      _keyfile(),
      _sessionCache(false),
      _cipherList(),
      _sslProtocol(TLS_V1),
      _sslOptions(
          (long)(SSL_OP_TLS_ROLLBACK_BUG | SSL_OP_CIPHER_SERVER_PREFERENCE)),
      _ecdhCurve("prime256v1"),
      _sslContext(nullptr) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Ssl");
  startsAfter("Logger");
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
                     "ssl cipers to use, see OpenSSL documentation",
                     new StringParameter(&_cipherList));

  std::unordered_set<uint64_t> sslProtocols = {1, 2, 3, 4, 5};

  options->addOption("--ssl.protocol",
                     "ssl protocol (1 = SSLv2, 2 = SSLv23, 3 = SSLv3, 4 = "
                     "TLSv1, 5 = TLSV1.2 (recommended)",
                     new DiscreteValuesParameter<UInt64Parameter>(
                         &_sslProtocol, sslProtocols));

  options->addHiddenOption(
      "--ssl.options", "ssl connection options, see OpenSSL documentation",
      new UInt64Parameter(&_sslOptions));

  options->addOption(
      "--ssl.ecdh-curve",
      "SSL ECDH Curve, see the output of \"openssl ecparam -list_curves\"",
      new StringParameter(&_ecdhCurve));
}

void SslServerFeature::prepare() {
  createSslContext();

  LOG(INFO) << "using SSL options: " << stringifySslOptions(_sslOptions);

  if (!_cipherList.empty()) {
      LOG(INFO) << "using SSL cipher-list '" << _cipherList << "'";
  }
}

void SslServerFeature::unprepare() {
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

void SslServerFeature::createSslContext() {
  // check keyfile
  if (_keyfile.empty()) {
    return;
  }

  // validate protocol
  if (_sslProtocol <= SSL_UNKNOWN || _sslProtocol >= SSL_LAST) {
    LOG(FATAL) << "invalid SSL protocol version specified. Please use a valid "
                  "value for '--ssl.protocol.'";
    FATAL_ERROR_EXIT();
  }

  LOG(DEBUG) << "using SSL protocol version '"
             << protocolName((protocol_e)_sslProtocol) << "'";

  if (!FileUtils::exists(_keyfile)) {
    LOG(FATAL) << "unable to find SSL keyfile '" << _keyfile << "'";
    FATAL_ERROR_EXIT();
  }

  // create context
  _sslContext = ::sslContext(protocol_e(_sslProtocol), _keyfile);

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
  SSL_CTX_set_options(_sslContext, (long)_sslOptions);

  if (!_cipherList.empty()) {
    if (SSL_CTX_set_cipher_list(_sslContext, _cipherList.c_str()) != 1) {
      LOG(FATAL) << "cannot set SSL cipher list '" << _cipherList
                 << "': " << lastSSLError();
      FATAL_ERROR_EXIT();
    }
  }

#if OPENSSL_VERSION_NUMBER >= 0x0090800fL
  int sslEcdhNid;
  EC_KEY* ecdhKey;
  sslEcdhNid = OBJ_sn2nid(_ecdhCurve.c_str());

  if (sslEcdhNid == 0) {
    LOG(FATAL) << "SSL error: " << lastSSLError()
             << " Unknown curve name: " << _ecdhCurve;
    FATAL_ERROR_EXIT();
  }

  // https://www.openssl.org/docs/manmaster/apps/ecparam.html
  ecdhKey = EC_KEY_new_by_curve_name(sslEcdhNid);
  if (ecdhKey == nullptr) {
    LOG(FATAL) << "SSL error: " << lastSSLError()
             << " Unable to create curve by name: " << _ecdhCurve;
    FATAL_ERROR_EXIT();
  }

  SSL_CTX_set_tmp_ecdh(_sslContext, ecdhKey);
  SSL_CTX_set_options(_sslContext, SSL_OP_SINGLE_ECDH_USE);
  EC_KEY_free(ecdhKey);
#endif

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

std::string SslServerFeature::stringifySslOptions(
    uint64_t opts) const {
  std::string result;

#ifdef SSL_OP_MICROSOFT_SESS_ID_BUG
  if (opts & SSL_OP_MICROSOFT_SESS_ID_BUG) {
    result.append(", SSL_OP_MICROSOFT_SESS_ID_BUG");
  }
#endif

#ifdef SSL_OP_NETSCAPE_CHALLENGE_BUG
  if (opts & SSL_OP_NETSCAPE_CHALLENGE_BUG) {
    result.append(", SSL_OP_NETSCAPE_CHALLENGE_BUG");
  }
#endif

#ifdef SSL_OP_LEGACY_SERVER_CONNECT
  if (opts & SSL_OP_LEGACY_SERVER_CONNECT) {
    result.append(", SSL_OP_LEGACY_SERVER_CONNECT");
  }
#endif

#ifdef SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG
  if (opts & SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG) {
    result.append(", SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG");
  }
#endif

#ifdef SSL_OP_TLSEXT_PADDING
  if (opts & SSL_OP_TLSEXT_PADDING) {
    result.append(", SSL_OP_TLSEXT_PADDING");
  }
#endif

#ifdef SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER
  if (opts & SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER) {
    result.append(", SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER");
  }
#endif

#ifdef SSL_OP_SAFARI_ECDHE_ECDSA_BUG
  if (opts & SSL_OP_SAFARI_ECDHE_ECDSA_BUG) {
    result.append(", SSL_OP_SAFARI_ECDHE_ECDSA_BUG");
  }
#endif

#ifdef SSL_OP_SSLEAY_080_CLIENT_DH_BUG
  if (opts & SSL_OP_SSLEAY_080_CLIENT_DH_BUG) {
    result.append(", SSL_OP_SSLEAY_080_CLIENT_DH_BUG");
  }
#endif

#ifdef SSL_OP_TLS_D5_BUG
  if (opts & SSL_OP_TLS_D5_BUG) {
    result.append(", SSL_OP_TLS_D5_BUG");
  }
#endif

#ifdef SSL_OP_TLS_BLOCK_PADDING_BUG
  if (opts & SSL_OP_TLS_BLOCK_PADDING_BUG) {
    result.append(", SSL_OP_TLS_BLOCK_PADDING_BUG");
  }
#endif

#ifdef SSL_OP_MSIE_SSLV2_RSA_PADDING
  if (opts & SSL_OP_MSIE_SSLV2_RSA_PADDING) {
    result.append(", SSL_OP_MSIE_SSLV2_RSA_PADDING");
  }
#endif

#ifdef SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG
  if (opts & SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG) {
    result.append(", SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG");
  }
#endif

#ifdef SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS
  if (opts & SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) {
    result.append(", SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS");
  }
#endif

#ifdef SSL_OP_NO_QUERY_MTU
  if (opts & SSL_OP_NO_QUERY_MTU) {
    result.append(", SSL_OP_NO_QUERY_MTU");
  }
#endif

#ifdef SSL_OP_COOKIE_EXCHANGE
  if (opts & SSL_OP_COOKIE_EXCHANGE) {
    result.append(", SSL_OP_COOKIE_EXCHANGE");
  }
#endif

#ifdef SSL_OP_NO_TICKET
  if (opts & SSL_OP_NO_TICKET) {
    result.append(", SSL_OP_NO_TICKET");
  }
#endif

#ifdef SSL_OP_CISCO_ANYCONNECT
  if (opts & SSL_OP_CISCO_ANYCONNECT) {
    result.append(", SSL_OP_CISCO_ANYCONNECT");
  }
#endif

#ifdef SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION
  if (opts & SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION) {
    result.append(", SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION");
  }
#endif

#ifdef SSL_OP_NO_COMPRESSION
  if (opts & SSL_OP_NO_COMPRESSION) {
    result.append(", SSL_OP_NO_COMPRESSION");
  }
#endif

#ifdef SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION
  if (opts & SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION) {
    result.append(", SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION");
  }
#endif

#ifdef SSL_OP_SINGLE_ECDH_USE
  if (opts & SSL_OP_SINGLE_ECDH_USE) {
    result.append(", SSL_OP_SINGLE_ECDH_USE");
  }
#endif

#ifdef SSL_OP_SINGLE_DH_USE
  if (opts & SSL_OP_SINGLE_DH_USE) {
    result.append(", SSL_OP_SINGLE_DH_USE");
  }
#endif

#ifdef SSL_OP_EPHEMERAL_RSA
  if (opts & SSL_OP_EPHEMERAL_RSA) {
    result.append(", SSL_OP_EPHEMERAL_RSA");
  }
#endif

#ifdef SSL_OP_CIPHER_SERVER_PREFERENCE
  if (opts & SSL_OP_CIPHER_SERVER_PREFERENCE) {
    result.append(", SSL_OP_CIPHER_SERVER_PREFERENCE");
  }
#endif

#ifdef SSL_OP_TLS_ROLLBACK_BUG
  if (opts & SSL_OP_TLS_ROLLBACK_BUG) {
    result.append(", SSL_OP_TLS_ROLLBACK_BUG");
  }
#endif

#ifdef SSL_OP_NO_SSLv2
  if (opts & SSL_OP_NO_SSLv2) {
    result.append(", SSL_OP_NO_SSLv2");
  }
#endif

#ifdef SSL_OP_NO_SSLv3
  if (opts & SSL_OP_NO_SSLv3) {
    result.append(", SSL_OP_NO_SSLv3");
  }
#endif

#ifdef SSL_OP_NO_TLSv1
  if (opts & SSL_OP_NO_TLSv1) {
    result.append(", SSL_OP_NO_TLSv1");
  }
#endif

#ifdef SSL_OP_NO_TLSv1_2
  if (opts & SSL_OP_NO_TLSv1_2) {
    result.append(", SSL_OP_NO_TLSv1_2");
  }
#endif

#ifdef SSL_OP_NO_TLSv1_1
  if (opts & SSL_OP_NO_TLSv1_1) {
    result.append(", SSL_OP_NO_TLSv1_1");
  }
#endif

#ifdef SSL_OP_NO_DTLSv1
  if (opts & SSL_OP_NO_DTLSv1) {
    result.append(", SSL_OP_NO_DTLSv1");
  }
#endif

#ifdef SSL_OP_NO_DTLSv1_2
  if (opts & SSL_OP_NO_DTLSv1_2) {
    result.append(", SSL_OP_NO_DTLSv1_2");
  }
#endif

#ifdef SSL_OP_NO_SSL_MASK
  if (opts & SSL_OP_NO_SSL_MASK) {
    result.append(", SSL_OP_NO_SSL_MASK");
  }
#endif

#ifdef SSL_OP_PKCS1_CHECK_1
  if (opts & SSL_OP_PKCS1_CHECK_1) {
    result.append(", SSL_OP_PKCS1_CHECK_1");
  }
#endif

#ifdef SSL_OP_PKCS1_CHECK_2
  if (opts & SSL_OP_PKCS1_CHECK_2) {
    result.append(", SSL_OP_PKCS1_CHECK_2");
  }
#endif

#ifdef SSL_OP_NETSCAPE_CA_DN_BUG
  if (opts & SSL_OP_NETSCAPE_CA_DN_BUG) {
    result.append(", SSL_OP_NETSCAPE_CA_DN_BUG");
  }
#endif

#ifdef SSL_OP_NETSCAPE_DEMO_CIPHER_CHANGE_BUG
  if (opts & SSL_OP_NETSCAPE_DEMO_CIPHER_CHANGE_BUG) {
    result.append(", SSL_OP_NETSCAPE_DEMO_CIPHER_CHANGE_BUG");
  }
#endif

#ifdef SSL_OP_CRYPTOPRO_TLSEXT_BUG
  if (opts & SSL_OP_CRYPTOPRO_TLSEXT_BUG) {
    result.append(", SSL_OP_CRYPTOPRO_TLSEXT_BUG");
  }
#endif

  if (result.empty()) {
    return result;
  }

  // strip initial comma
  return result.substr(2);
}
