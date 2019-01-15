////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_APPLICATION_FEATURES_JEMALLOC_FEATURE_H
#define ARANGODB_APPLICATION_FEATURES_JEMALLOC_FEATURE_H 1

#undef ARANGODB_MMAP_JEMALLOC

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
class JemallocFeature final : public application_features::ApplicationFeature {
 public:
  explicit JemallocFeature(application_features::ApplicationServer* server);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;

 public:
  void setDefaultPath(std::string const&);

 private:
#if ARANGODB_MMAP_JEMALLOC
  int64_t _residentLimit = 0;
  std::string _path;
#endif
  std::string _defaultPath;

#if ARANGODB_MMAP_JEMALLOC
  static char _staticPath[PATH_MAX + 1];
#endif
};
}  // namespace arangodb

#endif
