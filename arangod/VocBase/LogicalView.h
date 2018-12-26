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

#include "Basics/Common.h"
#include "Basics/Result.h"
#include "VocBase/ViewImplementation.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Buffer.h>

namespace arangodb {

namespace velocypack {
class Slice;
}

class PhysicalView;

class LogicalView {
  friend struct ::TRI_vocbase_t;

 public:
  LogicalView(TRI_vocbase_t*, velocypack::Slice const&);
  ~LogicalView();

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

  inline TRI_voc_cid_t id() const { return _id; }

  TRI_voc_cid_t planId() const;

  std::string type() const { return _type; }
  std::string name() const;
  std::string dbName() const;

  bool deleted() const;
  void setDeleted(bool);

  PhysicalView* getPhysical() const { return _physical.get(); }
  ViewImplementation* getImplementation() const {
    return _implementation.get();
  }

  void drop();

  // SECTION: Serialization
  VPackBuilder toVelocyPack(bool includeProperties = false, bool includeSystem = false) const;

  void toVelocyPack(velocypack::Builder&, bool includeProperties = false,
                    bool includeSystem = false) const;

  inline TRI_vocbase_t* vocbase() const { return _vocbase; }

  // Update this view.
  arangodb::Result updateProperties(velocypack::Slice const&, bool, bool);

  /// @brief Persist the connected physical view.
  ///        This should be called AFTER the view is successfully
  ///        created and only on Sinlge/DBServer
  void persistPhysicalView();

  /// @brief Create implementation object using factory method
  void spawnImplementation(ViewCreator creator,
                           arangodb::velocypack::Slice const& parameters, bool isNew);

  static bool IsAllowedName(velocypack::Slice parameters);
  static bool IsAllowedName(std::string const& name);

 private:
  // SECTION: Meta Information
  //
  // @brief Local view id
  TRI_voc_cid_t const _id;

  // @brief Global view id
  TRI_voc_cid_t const _planId;

  // @brief view type
  std::string const _type;

  // @brief view Name
  std::string _name;

  bool _isDeleted;

  TRI_vocbase_t* _vocbase;

  std::unique_ptr<PhysicalView> _physical;
  std::unique_ptr<ViewImplementation> _implementation;

  mutable basics::ReadWriteLock _lock;  // lock protecting the status and name
  mutable basics::ReadWriteLock _infoLock;  // lock protecting the properties
};

}  // namespace arangodb

#endif
