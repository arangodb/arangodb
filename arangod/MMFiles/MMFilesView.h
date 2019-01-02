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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_MMFILES_MMFILES_VIEW_H
#define ARANGOD_MMFILES_MMFILES_VIEW_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "VocBase/LogicalView.h"
#include "VocBase/PhysicalView.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
class LogicalView;
class Result;

class MMFilesView final : public PhysicalView {
 public:
  static inline MMFilesView* toMMFilesView(PhysicalView* physical) {
    auto rv = static_cast<MMFilesView*>(physical);
    TRI_ASSERT(rv != nullptr);
    return rv;
  }

  static inline MMFilesView* toMMFilesView(LogicalView* logical) {
    auto phys = logical->getPhysical();
    TRI_ASSERT(phys != nullptr);
    return toMMFilesView(phys);
  }

 public:
  explicit MMFilesView(LogicalView*, VPackSlice const& info);
  explicit MMFilesView(LogicalView*, PhysicalView*);  // use in cluster
                                                      // only!!!!!

  ~MMFilesView();

  std::string const& path() const override { return _path; }

  void setPath(std::string const& path) override { _path = path; }

  arangodb::Result updateProperties(VPackSlice const& slice, bool doSync) override;
  virtual arangodb::Result persistProperties() override;

  virtual PhysicalView* clone(LogicalView*, PhysicalView*) override;

  void getPropertiesVPack(velocypack::Builder&, bool includeSystem = false) const override;

  /// @brief opens an existing view
  void open() override;

  void drop() override;

 private:
  std::string _path;
};
}  // namespace arangodb

#endif
