#include "ClientFeatureOptions.h"

#include "Basics/StaticStrings.h"
#include "Endpoint/Endpoint.h"
#include "Ssl/ssl-helper.h"

namespace arangodb {

ClientFeatureOptions::ClientFeatureOptions()
    : endpoints{{Endpoint::defaultEndpoint()}},
      databaseName{StaticStrings::SystemDatabase},
      sslProtocol{TLS_V12} {}

}  // namespace arangodb
