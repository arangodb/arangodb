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

#ifndef ARANGOD_VOCBASE_VIEW_IMPLEMENTATION_H
#define ARANGOD_VOCBASE_VIEW_IMPLEMENTATION_H 1

#include "Basics/Common.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
class LogicalView;
class PhysicalView;
class Result;

/// @brief interface for view implementation
class ViewImplementation {
 protected:
  ViewImplementation(LogicalView* logical, arangodb::velocypack::Slice const& info)
      : _logicalView(logical) {}

 public:
  virtual ~ViewImplementation() = default;

  /// @brief called when a view's properties are updated
  virtual arangodb::Result updateProperties(arangodb::velocypack::Slice const& slice,
                                            bool partialUpdate, bool doSync) = 0;

  /// @brief callend when a view's properties are materialized into
  /// the VelocyPack Builder passed into the method. the implementation
  /// is supposed to fill in all its specific properties. The Builder
  /// points into an open VelocyPack object. The method is supposed to
  /// add all its own property attributes with their values, and must
  /// not close the Builder
  virtual void getPropertiesVPack(velocypack::Builder&) const = 0;

  /// @brief opens an existing view when the server is restarted
  virtual void open() = 0;

  /// @brief drops an existing view
  virtual void drop() = 0;

 protected:
  LogicalView* _logicalView;
};

/// @brief typedef for a ViewImplementation creator function
/// this typedef is used when registering the creator function for
/// any view type. the creator function is called when a view is first
/// created or re-opened after a server restart.
/// the VelocyPack Slice will contain all information about the
/// view's general and implementation-specific properties. the isNew
/// flag will be true if the view is first created, and false if a
/// view is re-opened on a server restart.
typedef std::function<std::unique_ptr<ViewImplementation>(LogicalView*, arangodb::velocypack::Slice const&, bool isNew)> ViewCreator;

}  // namespace arangodb

#endif
