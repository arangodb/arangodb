////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include "GeneralServer/GeneralServerOptionsProvider.h"

#include "Basics/StringUtils.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

#include <algorithm>

namespace arangodb {

using namespace arangodb::options;

void GeneralServerOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> opts, GeneralServerOptions& options) {
  opts->addOldOption("server.allow-method-override",
                     "http.allow-method-override");
  opts->addOldOption("server.hide-product-header", "http.hide-product-header");
  opts->addOldOption("server.keep-alive-timeout", "http.keep-alive-timeout");
  opts->addOldOption("no-server", "server.rest-server");

  opts->addOption(
      "--server.io-threads", "The number of threads used to handle I/O.",
      new UInt64Parameter(&options.numIoThreads, /*base*/ 1, /*minValue*/ 1),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Dynamic));

  opts->addOption("--server.support-info-api",
                  "The policy for exposing the support info API.",
                  new DiscreteValuesParameter<StringParameter>(
                      &options.supportInfoApiPolicy,
                      std::unordered_set<std::string>{"disabled", "jwt",
                                                      "admin", "public"}))
      .setIntroducedIn(30900);

  opts->addOption("--server.options-api",
                  "The policy for exposing the options API.",
                  new DiscreteValuesParameter<StringParameter>(
                      &options.optionsApiPolicy,
                      std::unordered_set<std::string>{"disabled", "jwt",
                                                      "admin", "public"}))
      .setIntroducedIn(31200);

  opts->addSection("http", "HTTP server features");

  // option was deprecated in 3.8 and removed in 3.12.
  opts->addObsoleteOption("--http.allow-method-override",
                          "Allow HTTP method override using special headers.",
                          true);

  opts->addOption("--http.keep-alive-timeout",
                  "The keep-alive timeout for HTTP connections (in seconds).",
                  new DoubleParameter(&options.keepAliveTimeout))
      .setLongDescription(R"(Idle keep-alive connections are closed by the
server automatically when the timeout is reached. A keep-alive-timeout value of
`0` disables the keep-alive feature entirely.)");

  // option was deprecated in 3.8 and removed in 3.12.
  opts->addObsoleteOption(
      "--http.hide-product-header",
      "Whether to omit the `Server: ArangoDB` header in HTTP responses.", true);

  opts->addOption(
      "--http.trusted-origin",
      "The trusted origin URLs for CORS requests with credentials.",
      new VectorParameter<StringParameter>(&options.accessControlAllowOrigins));

  opts->addOption("--http.redirect-root-to", "Redirect of the root URL.",
                  new StringParameter(&options.redirectRootTo));

  opts->addOption("--http.permanently-redirect-root",
                  "Whether to use a permanent or temporary redirect.",
                  new BooleanParameter(&options.permanentRootRedirect));

  opts->addOption("--http.return-queue-time-header",
                  "Whether to return the `x-arango-queue-time-seconds` header "
                  "in all responses.",
                  new BooleanParameter(&options.returnQueueTimeHeader))
      .setIntroducedIn(30900)
      .setLongDescription(R"(The value contained in this header indicates the
current queueing/dequeuing time for requests in the scheduler (in seconds).
Client applications and drivers can use this value to control the server load
and also react on overload.)");

  opts->addOption("--http.compress-response-threshold",
                  "The HTTP response body size from which on responses are "
                  "transparently compressed in case the client asks for it.",
                  new UInt64Parameter(&options.compressResponseThreshold))
      .setIntroducedIn(31200)
      .setLongDescription(
          R"(Automatically compress outgoing HTTP responses with the
deflate or gzip compression format, in case the client request advertises
support for this. Compression will only happen for HTTP/1.1 and HTTP/2
connections, if the size of the uncompressed response body exceeds
the threshold value controlled by this startup option,
and if the response body size after compression is less than the original
response body size.
Using the value 0 disables the automatic response compression.)");

  opts->addOption("--server.early-connections",
                  "Allow requests to a limited set of APIs early during the "
                  "server startup.",
                  new BooleanParameter(&options.allowEarlyConnections))
      .setIntroducedIn(31000);

  opts->addOldOption("frontend.proxy-request-check",
                     "web-interface.proxy-request-check");

  opts->addOption("--web-interface.proxy-request-check",
                  "Enable proxy request checking.",
                  new BooleanParameter(&options.proxyCheck),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle));

  opts->addOldOption("frontend.trusted-proxy", "web-interface.trusted-proxy");

  opts->addOption("--web-interface.trusted-proxy",
                  "The list of proxies to trust (can be IP or network). Make "
                  "sure `--web-interface.proxy-request-check` is enabled.",
                  new VectorParameter<StringParameter>(&options.trustedProxies),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle));

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  opts->addOption(
      "--server.failure-point",
      "The failure point to set during server startup (requires compilation "
      "with failure points support).",
      new VectorParameter<StringParameter>(&options.failurePoints),
      arangodb::options::makeFlags(arangodb::options::Flags::Default,
                                   arangodb::options::Flags::Uncommon));
#endif

  opts->addOption(
          "--http.handle-content-encoding-for-unauthenticated-requests",
          "Handle Content-Encoding headers for unauthenticated requests.",
          new BooleanParameter(
              &options.handleContentEncodingForUnauthenticatedRequests))
      .setIntroducedIn(31200)
      .setLongDescription(
          R"(If the option is set to `true`, the server will automatically
uncompress incoming HTTP requests with Content-Encodings gzip and deflate
even if the request is not authenticated.)");
}

void GeneralServerOptionsProvider::validateOptions(
    std::shared_ptr<ProgramOptions>, GeneralServerOptions& options) {
  if (!options.accessControlAllowOrigins.empty()) {
    // trim trailing slash from all members
    for (auto& it : options.accessControlAllowOrigins) {
      if (it == "*" || it == "all") {
        // special members "*" or "all" means all origins are allowed
        options.accessControlAllowOrigins.clear();
        options.accessControlAllowOrigins.push_back("*");
        break;
      } else if (it == "none") {
        // "none" means no origins are allowed
        options.accessControlAllowOrigins.clear();
        break;
      } else if (it.ends_with('/')) {
        // strip trailing slash
        it = it.substr(0, it.size() - 1);
      }
    }

    // remove empty members
    options.accessControlAllowOrigins.erase(
        std::remove_if(options.accessControlAllowOrigins.begin(),
                       options.accessControlAllowOrigins.end(),
                       [](std::string const& value) {
                         return basics::StringUtils::trim(value).empty();
                       }),
        options.accessControlAllowOrigins.end());
  }
}

}  // namespace arangodb
