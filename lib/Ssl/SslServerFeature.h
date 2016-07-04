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

#ifndef ARANGODB_APPLICATION_FEATURES_SSL_SERVER_FEATURE_H
#define ARANGODB_APPLICATION_FEATURES_SSL_SERVER_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

#include <openssl/ssl.h>

namespace arangodb {
class SslServerFeature final : public application_features::ApplicationFeature {
 public:
  explicit SslServerFeature(application_features::ApplicationServer* server);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void unprepare() override final;

 public:
  SSL_CTX* sslContext() const { return _sslContext; }

 public:
  std::string _cafile;
  std::string _keyfile;
  bool _sessionCache;
  std::string _cipherList;
  uint64_t _sslProtocol;
  uint64_t _sslOptions;
  std::string _ecdhCurve;

 private:
  void createSslContext();
  std::string stringifySslOptions(uint64_t opts) const;

 private:
  SSL_CTX* _sslContext;
  std::string _rctx;
};
}

#endif
