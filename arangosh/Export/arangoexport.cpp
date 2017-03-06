////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#define LDAP_DEPRECATED 1

#include "Basics/Common.h"
#include "Basics/directories.h"

#include "ApplicationFeatures/ConfigFeature.h"
#include "ApplicationFeatures/ShutdownFeature.h"
#include "ApplicationFeatures/TempFeature.h"
#include "ApplicationFeatures/VersionFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Export/ExportFeature.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomFeature.h"
#include "Shell/ClientFeature.h"
#include "Ssl/SslFeature.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <lber.h>
#include <ldap.h>

using namespace arangodb;
using namespace arangodb::application_features;

int main(int argc, char* argv[]) {


# if USE_LDAP_AUTH
  std::cout << "USE_LDAP_AUTH" << std::endl;
# endif


  LDAP *ld;
  int  result;
  int  auth_method = LDAP_AUTH_SIMPLE;
  int desired_version = LDAP_VERSION3;
  std::string ldap_host = "ldap.forumsys.com";
  std::string root_dn = "uid=euler,dc=example,dc=com";
  std::string root_pw = "password";

/*
OPTS = {
  server: {
    url: 'ldap://ldap.forumsys.com:389',
    bindDn: 'cn=read-only-admin,dc=example,dc=com',
    bindCredentials: 'password',
    searchBase: 'dc=example,dc=com',
    searchFilter: '(uid={{username}})'
  }
};
*/

  if ((ld = ldap_init(ldap_host.c_str(), LDAP_PORT)) == NULL ) {
    perror( "ldap_init failed" );
    exit( EXIT_FAILURE );
  }

  /* set the LDAP version to be 3 */
  if (ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &desired_version) != LDAP_OPT_SUCCESS)
   {
      ldap_perror(ld, "ldap_set_option");
      exit(EXIT_FAILURE);
   }
   
  if (ldap_bind_s(ld, root_dn.c_str(), root_pw.c_str(), auth_method) != LDAP_SUCCESS ) {
    ldap_perror( ld, "ldap_bind" );
    exit( EXIT_FAILURE );
  }

  result = ldap_unbind_s(ld);
  
  if (result != 0) {
    fprintf(stderr, "ldap_unbind_s: %s\n", ldap_err2string(result));
    exit( EXIT_FAILURE );
  }

  ArangoGlobalContext context(argc, argv, BIN_DIRECTORY);
  context.installHup();

  std::shared_ptr<options::ProgramOptions> options(new options::ProgramOptions(
      argv[0], "Usage: arangoexport [<options>]", "For more information use:", BIN_DIRECTORY));

  ApplicationServer server(options, BIN_DIRECTORY);

  int ret;

  server.addFeature(new ClientFeature(&server));
  server.addFeature(new ConfigFeature(&server, "arangoexport"));
  server.addFeature(new ExportFeature(&server, &ret));
  server.addFeature(new LoggerFeature(&server, false));
  server.addFeature(new RandomFeature(&server));
  server.addFeature(new ShutdownFeature(&server, {"Export"}));
  server.addFeature(new SslFeature(&server));
  server.addFeature(new TempFeature(&server, "arangoexport"));
  server.addFeature(new VersionFeature(&server));

  try {
    server.run(argc, argv);
    if (server.helpShown()) {
      // --help was displayed
      ret = EXIT_SUCCESS;
    }
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, Logger::STARTUP) << "arangoexport terminated because of an unhandled exception: "
             << ex.what();
    ret = EXIT_FAILURE;
  } catch (...) {
    LOG_TOPIC(ERR, Logger::STARTUP) << "arangoexport terminated because of an unhandled exception of "
                "unknown type";
    ret = EXIT_FAILURE;
  }

  return context.exit(ret);
}
