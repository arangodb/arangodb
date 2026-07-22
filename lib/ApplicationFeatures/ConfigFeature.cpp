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

#include "ApplicationFeatures/ConfigFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/VersionFeature.h"
#include "ProgramOptions/ProgramOptions.h"

#ifdef __linux__
#ifdef __GLIBC__
#include <nss.h>
#endif
#endif

using namespace arangodb::options;

namespace arangodb {

ConfigFeature::ConfigFeature(application_features::ApplicationServer& server,
                             std::string const& progname,
                             std::string const& configFilename)
    : ConfigFeature(server, progname, [&] {
        ConfigFeatureOptions opts{};
        opts.file = configFilename;
        return opts;
      }()) {}

ConfigFeature::ConfigFeature(application_features::ApplicationServer& server,
                             std::string const& progname,
                             ConfigFeatureOptions options)
    : application_features::ApplicationFeature{server, *this},
      _version{[&server]() {
        return server.hasFeature<VersionFeature>()
                   ? &server.getFeature<VersionFeature>()
                   : nullptr;
      }()},
      _options(std::move(options)) {
  ADB_PROD_ASSERT(_version != nullptr);
  if (_options.progname.empty()) {
    _options.progname = progname;
  }

  setOptional(false);
  startsAfter<LoggerFeature>();
  startsAfter<ShellColorsFeature>();
}

void ConfigFeature::prepare() {
#ifdef __linux__
#ifdef __GLIBC__
  // This code deserves and explanation.
  // Our release builds use Ubuntu 24.04 with glibc 2.39.0 (at the time of this
  // writing) and build static executables. This is all nice and convenient but
  // it has one disadvantage: If host name lookups or user name lookups happen,
  // the glibc uses the configuration file /etc/nsswitch.conf to decide how to
  // do these lookups. This is a runtime configuration option of glibc.
  // Unfortunately, glibc implements some of the options via dynamically loaded
  // modules (notably mdns4_minimal via libnss_mdns4_minimal.so) and does not
  // do versioned symbols for this.
  // If this happens on a system with a different version of glibc installed
  // (like for example an older Ubuntu system or a Debian or RedHat system),
  // then glibc tries to dynamically load a module which does not fit and the
  // process crashes with a high likelihood. To prevent this, we use the
  // (undocumented) override function below. This has the consequence that the
  // host name lookup will always just use /etc/hosts and normal DNS lookup. And
  // username lookup will always just use /etc/passwd, regardless of the system
  // configuration. There is an opt-out for this in form of the configuration
  // option --honor-nsswitch. Use this only if you are running on a system
  // without glibc installed, or with glibc version 2.39.0.
  //
  // Note: until ConfigFeature is constructed with
  // getOptions<ConfigOptionsProvider>(), _options.honorNsswitch is only the
  // ctor default (false), not the CLI/config value. The provider owns the live
  // option; wire that up when moving construction to
  // addFeaturesWithOptionProvider.
  if (!_options.honorNsswitch) {
    __nss_configure_lookup("hosts", "files dns");
    __nss_configure_lookup("passwd", "files");
    __nss_configure_lookup("group", "files");
  }
#endif
#endif
}
}  // namespace arangodb
