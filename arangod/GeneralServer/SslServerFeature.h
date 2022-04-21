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

#pragma once

#include <memory>
#include <string>

#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>

#include "Basics/asio_ns.h"
#include "RestServer/arangod.h"
#include "Ssl/ssl-helper.h"

namespace arangodb {
namespace options {
class ProgramOptions;
}

class SslServerFeature : public ArangodFeature {
 public:
  struct SNIEntry {
    SNIEntry(std::string name, std::string keyfileName)
        : serverName(std::move(name)), keyfileName(std::move(keyfileName)) {}
    std::string serverName;      // empty for default
    std::string keyfileName;     // name of key file
    std::string keyfileContent;  // content of key file
  };

  struct SslConfig {
    SslConfig()
        : context(),
          cafile(),
          cafileContent(),
          keyfile(),
          cipherList("HIGH:!EXPORT:!aNULL@STRENGTH"),
          sslProtocol(TLS_GENERIC),
          sslOptions(asio_ns::ssl::context::default_workarounds |
                     asio_ns::ssl::context::single_dh_use),
          ecdhCurve("prime256v1"),
          sessionCache(false),
          preferHttp11InAlpn(false),
          sslRequirePeerCertificate(false) {}
    std::string context;
    std::string cafile;
    std::string cafileContent;  // the actual cert file
    std::string keyfile;        // name of default keyfile
    std::string cipherList;
    uint64_t sslProtocol;
    uint64_t sslOptions;
    std::string ecdhCurve;
    bool sessionCache;
    bool preferHttp11InAlpn;
    bool sslRequirePeerCertificate;

    // For SNI, we have two maps, one mapping to the filename for a certain
    // server and context. Another map is global defined in the feature for
    // all combinations, to keep the actual keyfile in memory.

    // each config contains an index 'defaultIndex' which points to
    // the default certificate for that context
    std::vector<SNIEntry> sniEntries;
  };

  typedef std::shared_ptr<std::vector<asio_ns::ssl::context>> SslContextList;

  static constexpr std::string_view name() noexcept { return "SslServer"; }

  explicit SslServerFeature(Server& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;
  void prepare() override final;
  void unprepare() override final;
  void verifySslOptions();

  virtual SslContextList createSslContexts(SslConfig&);
  size_t chooseSslContext(std::string const& serverName, std::string const& context) const;

  // Dump all SSL related data into a builder, private keys
  // are hashed.
  virtual Result dumpTLSData(VPackBuilder& builder) const;

 protected:
  virtual void verifySslOptions(SslConfig&);

  std::unordered_map<std::string, std::string> _cafiles;
  std::unordered_map<std::string, std::string> _keyfiles;
  std::unordered_map<std::string, std::string> _cipherLists;
  std::unordered_map<std::string, uint64_t> _sslProtocols;
  std::unordered_map<std::string, uint64_t> _sslOptionss;
  std::unordered_map<std::string, std::string> _ecdhCurves;
  std::unordered_map<std::string, bool> _sessionCaches;
  std::unordered_map<std::string, bool> _preferHttp11InAlpns;

  std::unordered_map<std::string, SslConfig> _sslConfigs;

  // map server names to indices in _sniEntries
  std::unordered_map<std::string, size_t> sniServerIndex;

 private:
  std::string stringifySslOptions(uint64_t opts) const;

  asio_ns::ssl::context createSslContextInternal(SslConfig&, size_t idx);

  std::string _rctx;
};

}  // namespace arangodb
