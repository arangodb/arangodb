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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef REST_SERVER_FEATURE_CACHE_FEATURE_H
#define REST_SERVER_FEATURE_CACHE_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
class AuthenticationFeature;
class DatabaseFeature;

class FeatureCacheFeature final : public application_features::ApplicationFeature {
 public:
  explicit FeatureCacheFeature(application_features::ApplicationServer*);
  ~FeatureCacheFeature();

 public:
  void prepare() override final;
  void unprepare() override final;

  static inline FeatureCacheFeature* instance() {
    TRI_ASSERT(Instance != nullptr);
    return Instance;
  }

  inline AuthenticationFeature* authenticationFeature() const { 
    TRI_ASSERT(_authenticationFeature != nullptr);
    return _authenticationFeature;
  }

  inline DatabaseFeature* databaseFeature() const {
    TRI_ASSERT(_databaseFeature != nullptr);
    return _databaseFeature;
  }

 private:
  static FeatureCacheFeature* Instance;

  AuthenticationFeature* _authenticationFeature;
  DatabaseFeature* _databaseFeature;
};
}

#endif
