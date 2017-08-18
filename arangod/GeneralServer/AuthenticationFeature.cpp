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
/// @author Andreas Streichardt <andreas@arangodb.com>
////////////////////////////////////////////////////////////////////////////////

#include "AuthenticationFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomGenerator.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Utils/Authentication.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapAuthenticationHandler.h"
#include "Enterprise/Ldap/LdapFeature.h"
#endif

using namespace arangodb;
using namespace arangodb::options;

AuthenticationFeature* AuthenticationFeature::INSTANCE = nullptr;

AuthenticationFeature::AuthenticationFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Authentication"),
      _authInfo(nullptr),
      _authenticationUnixSockets(true),
      _authenticationSystemOnly(true),
      _jwtSecretProgramOption(""),
      _active(true) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Random");
#ifdef USE_ENTERPRISE
  startsAfter("Ldap");
#endif
}

AuthenticationFeature::~AuthenticationFeature() { delete _authInfo; }

void AuthenticationFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");

  options->addOldOption("server.disable-authentication",
                        "server.authentication");
  options->addOldOption("server.disable-authentication-unix-sockets",
                        "server.authentication-unix-sockets");
  options->addOldOption("server.authenticate-system-only",
                        "server.authentication-system-only");
  options->addOldOption("server.allow-method-override",
                        "http.allow-method-override");
  options->addOldOption("server.hide-product-header",
                        "http.hide-product-header");
  options->addOldOption("server.keep-alive-timeout", "http.keep-alive-timeout");
  options->addOldOption("server.default-api-compatibility", "");
  options->addOldOption("no-server", "server.rest-server");

  options->addOption("--server.authentication",
                     "enable or disable authentication for ALL client requests",
                     new BooleanParameter(&_active));

  options->addOption(
      "--server.authentication-system-only",
      "use HTTP authentication only for requests to /_api and /_admin",
      new BooleanParameter(&_authenticationSystemOnly));

#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
  options->addOption("--server.authentication-unix-sockets",
                     "authentication for requests via UNIX domain sockets",
                     new BooleanParameter(&_authenticationUnixSockets));
#endif

  options->addOption("--server.jwt-secret",
                     "secret to use when doing jwt authentication",
                     new StringParameter(&_jwtSecretProgramOption));
}

void AuthenticationFeature::validateOptions(std::shared_ptr<ProgramOptions>) {
  if (!_jwtSecretProgramOption.empty()) {
    if (_jwtSecretProgramOption.length() > _maxSecretLength) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "Given JWT secret too long. Max length is " << _maxSecretLength;
      FATAL_ERROR_EXIT();
    }
  }
}

std::string AuthenticationFeature::generateNewJwtSecret() {
  std::string jwtSecret = "";
  uint16_t m = 254;

  for (size_t i = 0; i < _maxSecretLength; i++) {
    jwtSecret += (1 + RandomGenerator::interval(m));
  }
  return jwtSecret;
}

void AuthenticationFeature::prepare() {
  if (isEnabled()) {
    std::unique_ptr<AuthenticationHandler> handler;
#if USE_ENTERPRISE
    if (application_features::ApplicationServer::getFeature<LdapFeature>("Ldap")
            ->isEnabled()) {
      handler.reset(new LdapAuthenticationHandler());
    } else {
      handler.reset(new DefaultAuthenticationHandler());
    }
#else
    handler.reset(new DefaultAuthenticationHandler());
#endif
    _authInfo = new AuthInfo(std::move(handler));

    std::string jwtSecret = _jwtSecretProgramOption;
    if (jwtSecret.empty()) {
      jwtSecret = generateNewJwtSecret();
    }
    authInfo()->setJwtSecret(jwtSecret);
  }
}

void AuthenticationFeature::start() {
  INSTANCE = this;

  std::ostringstream out;

  out << "Authentication is turned " << (_active ? "on" : "off");

  if (isEnabled()) {
    auto queryRegistryFeature =
        application_features::ApplicationServer::getFeature<
            QueryRegistryFeature>("QueryRegistry");
    _authInfo->setQueryRegistry(queryRegistryFeature->queryRegistry());

    if (_active && _authenticationSystemOnly) {
      out << " (system only)";
    }

#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
    out << ", authentication for unix sockets is turned "
        << (_authenticationUnixSockets ? "on" : "off");
#endif
  }

  LOG_TOPIC(INFO, arangodb::Logger::FIXME) << out.str();
}

AuthLevel AuthenticationFeature::canUseDatabase(std::string const& username,
                                                std::string const& dbname) {
  if (!isActive()) {
    return AuthLevel::RW;
  }

  return authInfo()->canUseDatabase(username, dbname);
}

AuthLevel AuthenticationFeature::canUseCollection(std::string const& username,
                                                  std::string const& dbname,
                                                  std::string const& coll) {
  if (!isActive()) {
    return AuthLevel::RW;
  }

  return authInfo()->canUseCollection(username, dbname, coll);
}

AuthInfo* AuthenticationFeature::authInfo() {
  TRI_ASSERT(_authInfo != nullptr);  
  return _authInfo;
}

void AuthenticationFeature::unprepare() { INSTANCE = nullptr; }

