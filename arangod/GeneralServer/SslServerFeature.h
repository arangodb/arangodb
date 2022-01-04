////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationFeature.h"

#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/asio_ns.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace options {
class ProgramOptions;
}

class SslServerFeature : public application_features::ApplicationFeature {
 public:
  typedef std::shared_ptr<std::vector<asio_ns::ssl::context>> SslContextList;

  explicit SslServerFeature(application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;
  void prepare() override final;
  void unprepare() override final;
  virtual void verifySslOptions();

  virtual SslContextList createSslContexts();
  size_t chooseSslContext(std::string const& serverName) const;

  // Dump all SSL related data into a builder, private keys
  // are hashed.
  virtual Result dumpTLSData(VPackBuilder& builder) const;

 protected:

  struct SNIEntry {
    std::string serverName;      // empty for default
    std::string keyfileName;     // name of key file
    std::string keyfileContent;  // content of key file
    SNIEntry(std::string name, std::string keyfileName)
      : serverName(std::move(name)), keyfileName(std::move(keyfileName)) {}
  };

  std::string _cafile;
  std::string _cafileContent;  // the actual cert file
  std::string _keyfile;        // name of default keyfile
  // For SNI, we have two maps, one mapping to the filename for a certain
  // server, another, to keep the actual keyfile in memory.
  std::vector<SNIEntry> _sniEntries;   // the first entry is the default server keyfile
  std::unordered_map<std::string, size_t> _sniServerIndex;  // map server names to indices in _sniEntries
  std::string _cipherList;
  uint64_t _sslProtocol;
  uint64_t _sslOptions;
  std::string _ecdhCurve;
  bool _sessionCache;
  bool _preferHttp11InAlpn;

 private:
  asio_ns::ssl::context createSslContextInternal(std::string keyfileName,
                                                 std::string& content);

  std::string stringifySslOptions(uint64_t opts) const;

  std::string _rctx;
};

}  // namespace arangodb

