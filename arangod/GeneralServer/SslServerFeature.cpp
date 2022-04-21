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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <exception>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#include <boost/asio/ssl/context_base.hpp>
#include <boost/asio/ssl/impl/context.ipp>

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/ec.h>
#include <openssl/objects.h>
#include <openssl/opensslv.h>
#include <openssl/ossl_typ.h>
#include <openssl/safestack.h>
#include <openssl/ssl3.h>
#include <openssl/x509.h>

#include "SslServerFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/application-exit.h"
#include "Basics/files.h"
#include "Logger/LogLevel.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Option.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Random/UniformCharacter.h"

#include <nghttp2/nghttp2.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

SslServerFeature::SslServerFeature(Server& server)
    : ArangodFeature{server, *this} {
  setOptional(true);
  startsAfter<application_features::AqlFeaturePhase>();
}

void SslServerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOldOption("server.cafile", "ssl.cafile");
  options->addOldOption("server.keyfile", "ssl.keyfile");
  options->addOldOption("server.ssl-cache", "ssl.session-cache");
  options->addOldOption("server.ssl-cipher-list", "ssl.cipher-list");
  options->addOldOption("server.ssl-options", "ssl.options");
  options->addOldOption("server.ssl-protocol", "ssl.protocol");

  options->addSection("ssl", "SSL communication");

  std::unordered_set<uint64_t> const sslProtocols = availableSslProtocols();
  SslConfig defaultConfig{};

  options->addOption(
      "--ssl.cafile", "ca file used for secure connections",
      new ContextParameter<StringParameter>(&_cafiles, defaultConfig.cafile));

  options->addOption(
      "--ssl.keyfile", "key-file used for secure connections",
      new ContextParameter<StringParameter>(&_keyfiles, defaultConfig.keyfile));

  options->addOption("--ssl.session-cache",
                     "enable the session cache for connections",
                     new ContextParameter<BooleanParameter>(
                         &_sessionCaches, defaultConfig.sessionCache));

  options->addOption("--ssl.cipher-list",
                     "ssl ciphers to use, see OpenSSL documentation",
                     new ContextParameter<StringParameter>(
                         &_cipherLists, defaultConfig.cipherList));

  options->addOption(
      "--ssl.protocol", availableSslProtocolsDescription(),
      new ContextParameter<DiscreteValuesParameter<UInt64Parameter>>(
          &_sslProtocols, sslProtocols, defaultConfig.sslProtocol));

  options->addOption(
      "--ssl.options", "ssl connection options, see OpenSSL documentation",
      new ContextParameter<UInt64Parameter>(&_sslOptionss,
                                            defaultConfig.sslOptions),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption(
      "--ssl.ecdh-curve",
      "SSL ECDH Curve, see the output of \"openssl ecparam -list_curves\"",
      new ContextParameter<StringParameter>(&_ecdhCurves,
                                            defaultConfig.ecdhCurve));

  options->addOption(
      "--ssl.prefer-http1-in-alpn",
      "Allows to let the server prefer HTTP/1.1 over HTTP/2 in "
      "ALPN protocol negotiations",
      new ContextParameter<BooleanParameter>(&_preferHttp11InAlpns,
                                             defaultConfig.preferHttp11InAlpn));
}

void SslServerFeature::validateOptions(
    std::shared_ptr<ProgramOptions> options) {
  // build ssl config structures
  for (auto data : _cafiles) {
    _sslConfigs[data.first].context = data.first;
    _sslConfigs[data.first].cafile = data.second;
  }

  for (auto data : _keyfiles) {
    _sslConfigs[data.first].context = data.first;
    _sslConfigs[data.first].keyfile = data.second;
  }

  for (auto data : _cipherLists) {
    _sslConfigs[data.first].context = data.first;
    _sslConfigs[data.first].cipherList = data.second;
  }

  for (auto data : _sslProtocols) {
    _sslConfigs[data.first].context = data.first;
    _sslConfigs[data.first].sslProtocol = data.second;
  }

  for (auto data : _sslOptionss) {

    // check for SSLv2
    if (data.second == SslProtocol::SSL_V2) {
      LOG_TOPIC("b7890", FATAL, arangodb::Logger::SSL)
        << "SSLv2 is not supported any longer because of security "
	   "vulnerabilities in this protocol";
      FATAL_ERROR_EXIT();
    }

    _sslConfigs[data.first].context = data.first;
    _sslConfigs[data.first].sslOptions = data.second;
  }

  for (auto data : _ecdhCurves) {
    _sslConfigs[data.first].context = data.first;
    _sslConfigs[data.first].ecdhCurve = data.second;
  }

  for (auto data : _sessionCaches) {
    _sslConfigs[data.first].context = data.first;
    _sslConfigs[data.first].sessionCache = data.second;
  }

  for (auto data : _preferHttp11InAlpns) {
    _sslConfigs[data.first].context = data.first;
    _sslConfigs[data.first].preferHttp11InAlpn = data.second;
  }
}

void SslServerFeature::prepare() {
  for (auto configPair : _sslConfigs) {
    auto config = configPair.second;

    LOG_TOPIC("afcd3", INFO, arangodb::Logger::SSL)
	<< "using SSL options for '" << config.context,
	<< "': " << stringifySslOptions(config.sslOptions);

    if (!config.cipherList.empty()) {
      LOG_TOPIC("9b126", INFO, arangodb::Logger::SSL)
	  << "using SSL cipher-list for '" << config.context << "': '" << config.cipherList
	  << "'";
    }
  }

  UniformCharacter r(
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
  _rctx = r.random(SSL_MAX_SSL_SESSION_ID_LENGTH);
}

void SslServerFeature::unprepare() {
  for (auto config : _sslConfigs) {
    LOG_TOPIC("7093e", TRACE, arangodb::Logger::SSL)
        << "unpreparing ssl '" << config.first << "'";
  }
}

void SslServerFeature::verifySslOptions(SslConfig& config) {
  // check keyfile
  if (config.keyfile.empty()) {
    LOG_TOPIC("f0dca", FATAL, arangodb::Logger::SSL)
        << "no value specified for '--ssl.keyfile' for '" << config.context
        << "'";
    FATAL_ERROR_EXIT();
  }

  // validate protocol
  // cppcheck-suppress unsignedLessThanZero
  if (config.sslProtocol <= SSL_UNKNOWN || config.sslProtocol >= SSL_LAST) {
    LOG_TOPIC("1f48b", FATAL, arangodb::Logger::SSL)
        << "invalid SSL protocol version specified. Please use a valid "
           "value for '--ssl.protocol' for '"
        << config.context << "'";
    FATAL_ERROR_EXIT();
  }

  LOG_TOPIC("47161", DEBUG, arangodb::Logger::SSL)
      << "using SSL protocol version '"
      << protocolName(SslProtocol(config.sslProtocol)) << "' for '"
      << config.context << "'";

  if (!FileUtils::exists(config.keyfile)) {
    LOG_TOPIC("51cf0", FATAL, arangodb::Logger::SSL)
        << "unable to find SSL keyfile '" << config.keyfile << "' for '"
        << config.context << "'";
    FATAL_ERROR_EXIT();
  }
}

void SslServerFeature::verifySslOptions() {
  for (auto config : _sslConfigs) {
    verifySslOptions(config.second);
  }

  // just to test if everything works
  try {
    createSslContexts();
  } catch (...) {
    LOG_TOPIC("997d2", FATAL, arangodb::Logger::SSL)
        << "cannot create SSL context for '" << config.context << "'";
    FATAL_ERROR_EXIT();
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
}  // namespace

static inline bool searchForProtocol(const unsigned char** out,
                                     unsigned char* outlen,
                                     const unsigned char* in,
                                     unsigned int inlen, const char* proto) {
  size_t len = strlen(proto);
  size_t i = 0;
  while (i + len <= inlen) {
    if (memcmp(in + i, proto, len) == 0) {
      *out = (const unsigned char*)(in + i + 1);
      *outlen = proto[0];
      return true;
    }
    i += in[i] + 1;
  }
  return false;
}

static int alpn_select_proto_cb(SSL* ssl, const unsigned char** out,
                                unsigned char* outlen, const unsigned char* in,
                                unsigned int inlen, void* arg) {
  int rv = 0;
  bool const* preferHttp11InAlpn = (bool*)arg;
  if (*preferHttp11InAlpn) {
    if (!searchForProtocol(out, outlen, in, inlen, "\x8http/1.1")) {
      if (!searchForProtocol(out, outlen, in, inlen, "\x2h2")) {
        rv = -1;
      }
    }
  } else {
    rv = nghttp2_select_next_protocol((unsigned char**)out, outlen, in, inlen);
  }

  if (rv < 0) {
    return SSL_TLSEXT_ERR_NOACK;
  }

  return SSL_TLSEXT_ERR_OK;
}

asio_ns::ssl::context SslServerFeature::createSslContextInternal(
    SslConfig& config, size_t idx) {
  std::string name = config.context;
  std::string keyfilename = config.sniEntries[idx].keyfileName;
  std::string& content = config.sniEntries[idx].keyfileContent;

  // This method creates an SSL context using the keyfile in `keyfilename`
  // It is used internally if the public method `createSslContext`
  // is called and if the hello callback happens and a non-default
  // servername extension is detected, then with a non-empty servername.
  // If all goes well, the string `content` is set to the content of the
  // keyfile.
  try {
    std::string keyfileContent = FileUtils::slurp(keyfilename);
    // create context
    asio_ns::ssl::context sslContext =
        ::sslContext(SslProtocol(config.sslProtocol), keyfilename);
    content = std::move(keyfileContent);

    // and use this native handle
    asio_ns::ssl::context::native_handle_type nativeContext =
        sslContext.native_handle();

    // set cache mode
    SSL_CTX_set_session_cache_mode(nativeContext, config.sessionCache
                                                      ? SSL_SESS_CACHE_SERVER
                                                      : SSL_SESS_CACHE_OFF);

    if (config.sessionCache) {
      LOG_TOPIC("af2f4", TRACE, arangodb::Logger::SSL)
          << "using SSL session caching";
    }

    // set options
    sslContext.set_options(static_cast<long>(config.sslOptions));

    if (!config.cipherList.empty()) {
      if (SSL_CTX_set_cipher_list(nativeContext, config.cipherList.c_str()) !=
          1) {
        LOG_TOPIC("c6981", ERR, arangodb::Logger::SSL)
            << "cannot set SSL cipher list '" << config.cipherList
            << "': " << lastSSLError();
        throw std::runtime_error("cannot create SSL context");
      }
    }

#if OPENSSL_VERSION_NUMBER >= 0x0090800fL
    if (!config.ecdhCurve.empty()) {
      int sslEcdhNid = OBJ_sn2nid(config.ecdhCurve.c_str());

      if (sslEcdhNid == 0) {
        LOG_TOPIC("40292", ERR, arangodb::Logger::SSL)
            << "SSL error: " << lastSSLError()
            << " Unknown curve name: " << config.ecdhCurve;
        throw std::runtime_error("cannot create SSL context");
      }

      // https://www.openssl.org/docs/manmaster/apps/ecparam.html
      EC_KEY* ecdhKey = EC_KEY_new_by_curve_name(sslEcdhNid);
      if (ecdhKey == nullptr) {
        LOG_TOPIC("471f2", ERR, arangodb::Logger::SSL)
            << "SSL error: " << lastSSLError()
            << ". unable to create curve by name: " << config.ecdhCurve;
        throw std::runtime_error("cannot create SSL context");
      }

      if (SSL_CTX_set_tmp_ecdh(nativeContext, ecdhKey) != 1) {
        EC_KEY_free(ecdhKey);
        LOG_TOPIC("05d06", ERR, arangodb::Logger::SSL)
            << "cannot set ECDH option" << lastSSLError();
        throw std::runtime_error("cannot create SSL context");
      }

      EC_KEY_free(ecdhKey);
      SSL_CTX_set_options(nativeContext, SSL_OP_SINGLE_ECDH_USE);
    }
#endif

    // set ssl context
    int res = SSL_CTX_set_session_id_context(
        nativeContext, (unsigned char const*)_rctx.c_str(), (int)_rctx.size());

    if (res != 1) {
      LOG_TOPIC("72e4e", ERR, arangodb::Logger::SSL)
          << "cannot set SSL session id context '" << _rctx
          << "': " << lastSSLError();
      throw std::runtime_error("cannot create SSL context");
    }

    // check CA
    if (!config.cafile.empty()) {
      LOG_TOPIC("cdaf2", TRACE, arangodb::Logger::SSL)
          << "trying to load CA certificates from '" << config.cafile << "'";

      res = SSL_CTX_load_verify_locations(nativeContext, config.cafile.c_str(),
                                          nullptr);

      if (res == 0) {
        LOG_TOPIC("30289", ERR, arangodb::Logger::SSL)
            << "cannot load CA certificates from '" << config.cafile
            << "': " << lastSSLError();
        throw std::runtime_error("cannot create SSL context");
      }

      STACK_OF(X509_NAME) * certNames;

      std::string cafileContent = FileUtils::slurp(config.cafile);
      certNames = SSL_load_client_CA_file(config.cafile.c_str());
      config.cafileContent = cafileContent;

      if (certNames == nullptr) {
        LOG_TOPIC("30363", ERR, arangodb::Logger::SSL)
            << "cannot load CA certificates from '" << config.cafile
            << "': " << lastSSLError();
        throw std::runtime_error("cannot create SSL context");
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

            LOG_TOPIC("b8ebd", TRACE, arangodb::Logger::SSL)
                << "name: " << std::string(r, len);
          }
        }
      }

      SSL_CTX_set_client_CA_list(nativeContext, certNames);
    }

    sslContext.set_verify_mode(SSL_VERIFY_NONE);

    SSL_CTX_set_alpn_select_cb(sslContext.native_handle(), alpn_select_proto_cb,
                               (void*)(&config.preferHttp11InAlpn));

    return sslContext;
  } catch (std::exception const& ex) {
    LOG_TOPIC("bd0ba", ERR, arangodb::Logger::SSL)
        << "failed to create SSL context: " << ex.what();
    throw std::runtime_error("cannot create SSL context");
  } catch (...) {
    LOG_TOPIC("1217f", ERR, arangodb::Logger::SSL)
        << "failed to create SSL context, cannot create HTTPS server";
    throw std::runtime_error("cannot create SSL context");
  }
}

SslServerFeature::SslContextList SslServerFeature::createSslContexts() {
  auto result = std::make_shared<std::vector<asio_ns::ssl::context>>();

  for (size_t i = 0; i < config.sniEntries.size(); ++i) {
    auto res = createSslContextInternal(config, i);
    result->emplace_back(std::move(res));
  }

  return result;
}

size_t SslServerFeature::chooseSslContext(std::string const& serverName, std::string const& context) const {
  auto itr = _sslConfigs.find(context);

  if (itr != _sslConfigs.end()) {
    SslConfig const& config = itr->second;
    // Note that the map _sniServerIndex is basically immutable after the
    // startup phase, since the number of SNI entries cannot be changed
    // at runtime. Therefore, we do not need any protection here.
    auto it = config.sniServerIndex.find(serverName);
    if (it == config.sniServerIndex.end()) {
      return 0;
    } else {
      return it->second;
    }
  }

  return 0;
}

std::string SslServerFeature::stringifySslOptions(uint64_t opts) const {
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
  if (SSL_OP_PKCS1_CHECK_1) {
    if (opts & SSL_OP_PKCS1_CHECK_1) {
      result.append(", SSL_OP_PKCS1_CHECK_1");
    }
  }
#endif

#ifdef SSL_OP_PKCS1_CHECK_2
  if (SSL_OP_PKCS1_CHECK_1) {
    if (opts & SSL_OP_PKCS1_CHECK_2) {
      result.append(", SSL_OP_PKCS1_CHECK_2");
    }
  }
#endif

#ifdef SSL_OP_NETSCAPE_CA_DN_BUG
  if (SSL_OP_NETSCAPE_CA_DN_BUG) {
    if (opts & SSL_OP_NETSCAPE_CA_DN_BUG) {
      result.append(", SSL_OP_NETSCAPE_CA_DN_BUG");
    }
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

static void splitPEM(std::string const& pem, std::vector<std::string>& certs,
                     std::vector<std::string>& keys) {
  std::vector<std::string> result;
  size_t pos = 0;
  while (pos < pem.size()) {
    pos = pem.find("-----", pos);
    if (pos == std::string::npos) {
      return;
    }
    if (pem.compare(pos, 11, "-----BEGIN ") != 0) {
      return;
    }
    size_t posEndHeader = pem.find('\n', pos);
    if (posEndHeader == std::string::npos) {
      return;
    }
    size_t posStartFooter = pem.find("-----END ", posEndHeader);
    if (posStartFooter == std::string::npos) {
      return;
    }
    size_t posEndFooter = pem.find("-----", posStartFooter + 9);
    if (posEndFooter == std::string::npos) {
      return;
    }
    posEndFooter += 5;  // Point to line end, typically or end of file
    size_t p = posEndHeader;
    while (p > pos + 11 && (pem[p] == '\n' || pem[p] == '-' || pem[p] == '\r' ||
                            pem[p] == ' ')) {
      --p;
    }
    std::string type(pem.c_str() + pos + 11, (p + 1) - (pos + 11));
    if (type == "CERTIFICATE") {
      certs.emplace_back(pem.c_str() + pos, posEndFooter - pos);
    } else if (type.find("PRIVATE KEY") != std::string::npos) {
      keys.emplace_back(pem.c_str() + pos, posEndFooter - pos);
    } else {
      LOG_TOPIC("54271", INFO, Logger::SSL)
          << "Found part of type " << type << " in PEM file, ignoring it...";
    }
    pos = posEndFooter;
  }
}

static void dumpPEM(std::string const& pem, VPackBuilder& builder,
                    std::string attrName) {
  if (pem.empty()) {
    {
      VPackObjectBuilder guard(&builder, attrName);
      return;
    }
  }
  // Compute a SHA256 of the whole file:
  TRI_SHA256Functor func;
  func(pem.c_str(), pem.size());

  // Now split into certs and key:
  std::vector<std::string> certs;
  std::vector<std::string> keys;
  splitPEM(pem, certs, keys);

  // Now dump the certs and the hash of the key:
  {
    VPackObjectBuilder guard2(&builder, attrName);
    auto sha256 = func.finalize();
    builder.add("sha256", VPackValue(sha256));
    builder.add("SHA256", VPackValue(sha256));  // deprecated in 3.7 GA
    {
      VPackArrayBuilder guard3(&builder, "certificates");
      for (auto const& c : certs) {
        builder.add(VPackValue(c));
      }
    }
    if (!keys.empty()) {
      TRI_SHA256Functor func2;
      func2(keys[0].c_str(), keys[0].size());
      sha256 = func2.finalize();
      builder.add("privateKeySha256", VPackValue(sha256));
      builder.add("privateKeySHA256",
                  VPackValue(sha256));  // deprecated in 3.7 GA
    }
  }
}

// Dump all SSL related data into a builder, private keys
// are hashed.
Result SslServerFeature::dumpTLSData(VPackBuilder& builder) const {
  for (auto const& itr : _sslConfigs) {
    VPackObjectBuilder guard(&builder);

    SslConfig const& config = itr.second;
    std::string prefix = config.context;

    if (!prefix.empty()) {
      prefix = prefix + ":";
    }

    if (!config.sniEntries.empty()) {
      dumpPEM(config.sniEntries[0].keyfileContent, builder, prefix + "keyfile");
      dumpPEM(config.cafileContent, builder, prefix + "clientCA");
      if (config.sniEntries.size() > 1) {
        VPackObjectBuilder guard2(&builder, prefix + "SNI");
        for (size_t i = 1; i < config.sniEntries.size(); ++i) {
          dumpPEM(config.sniEntries[i].keyfileContent, builder,
                  config.sniEntries[i].serverName);
        }
      }
    }
  }

  return Result(TRI_ERROR_NO_ERROR);
}
