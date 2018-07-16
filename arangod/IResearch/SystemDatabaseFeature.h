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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__SYSTEM_DATABASE_FEATURE_H
#define ARANGOD_IRESEARCH__SYSTEM_DATABASE_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

struct TRI_vocbase_t; // forward declaration

namespace arangodb {
namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @brief a flexible way to get at the system vocbase
///        can be used for for persisting configuration
////////////////////////////////////////////////////////////////////////////////
class SystemDatabaseFeature final:
  public arangodb::application_features::ApplicationFeature {
 public:
  struct VocbaseReleaser {
    void operator()(TRI_vocbase_t* ptr);
  };
  typedef std::unique_ptr<TRI_vocbase_t, VocbaseReleaser> ptr;

  SystemDatabaseFeature(
    application_features::ApplicationServer* server,
    TRI_vocbase_t* vocbase = nullptr
  );
  static std::string const& name() noexcept;
  void start() override;
  void stop() override;
  ptr use() const;

 private:
  std::atomic<TRI_vocbase_t*> _vocbase; // cached pointer to the system database
};

} // iresearch
} // arangodb

#endif