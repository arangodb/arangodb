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

#ifndef ARANGOD_VOCBASE_LOGICAL_VIEW_H
#define ARANGOD_VOCBASE_LOGICAL_VIEW_H 1

#include "LogicalDataSource.h"
#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "VocBase/ViewImplementation.h"
#include "VocBase/voc-types.h"

#include <velocypack/Buffer.h>

namespace arangodb {

namespace velocypack {
class Slice;
}

namespace aql {
class ExecutionPlan;
struct ExecutionContext;
}

class PhysicalView;

class LogicalView final: public LogicalDataSource {
  friend struct ::TRI_vocbase_t;

 public:
  LogicalView(TRI_vocbase_t*, velocypack::Slice const&);
  ~LogicalView();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the category representing a logical view
  //////////////////////////////////////////////////////////////////////////////
  static Category const& category() noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief invoke visitor on all collections that a view will return
  /// @return visitation was successful
  //////////////////////////////////////////////////////////////////////////////
  bool visitCollections(
      std::function<bool(TRI_voc_cid_t)> const& visitor
  ) const {
    return _implementation && _implementation->visitCollections(visitor);
  }

 protected:  // If you need a copy outside the class, use clone below.
  explicit LogicalView(LogicalView const&);

 private:
  LogicalView& operator=(LogicalView const&) = delete;

 public:
  LogicalView() = delete;

  std::unique_ptr<LogicalView> clone() {
    auto p = new LogicalView(*this);
    return std::unique_ptr<LogicalView>(p);
  }

  PhysicalView* getPhysical() const { return _physical.get(); }
  ViewImplementation* getImplementation() const {
    return _implementation.get();
  }

  virtual void drop() override;
  virtual Result rename(std::string&& newName, bool doSync) override;

  // SECTION: Serialization
  velocypack::Builder toVelocyPack(bool includeProperties = false,
                                   bool includeSystem = false) const;

  void toVelocyPack(velocypack::Builder&, bool includeProperties = false,
                    bool includeSystem = false) const;

  // Update this view.
  arangodb::Result updateProperties(velocypack::Slice const&, bool, bool);

  /// @brief Persist the connected physical view.
  ///        This should be called AFTER the view is successfully
  ///        created and only on Sinlge/DBServer
  void persistPhysicalView();

  /// @brief Create implementation object using factory method
  void spawnImplementation(ViewCreator creator,
                           arangodb::velocypack::Slice const& parameters,
                           bool isNew);

  static bool IsAllowedName(velocypack::Slice parameters);
  static bool IsAllowedName(std::string const& name);

 private:
  // SECTION: Meta Information
  std::unique_ptr<PhysicalView> _physical;
  std::unique_ptr<ViewImplementation> _implementation;

  mutable basics::ReadWriteLock _lock;  // lock protecting the status and name
  mutable basics::ReadWriteLock _infoLock;  // lock protecting the properties
};

}  // namespace arangodb

#endif