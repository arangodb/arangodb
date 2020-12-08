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

#include "Auth/Common.h"
#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Meta/utility.h"
#include "VocBase/LogicalDataSource.h"
#include "VocBase/voc-types.h"

#include <velocypack/Buffer.h>

namespace arangodb {
namespace velocypack {

class Slice;
class Builder;

}  // namespace velocypack
} // arangodb

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @class LogicalView
////////////////////////////////////////////////////////////////////////////////
class LogicalView : public LogicalDataSource {
 public:
  typedef std::shared_ptr<LogicalView> ptr;
  typedef std::function<bool(TRI_voc_cid_t)> CollectionVisitor;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief casts a specified 'LogicalView' to a provided Target type
  //////////////////////////////////////////////////////////////////////////////
  template <typename Target, typename Source>
  inline static typename meta::adjustConst<Source, Target>::reference cast(Source& view) noexcept {
    typedef typename meta::adjustConst<
        Source, typename std::enable_if<std::is_base_of<LogicalView, Target>::value &&
                                            std::is_same<typename std::remove_const<Source>::type, LogicalView>::value,
                                        Target>::type>
        target_type_t;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // do not use dynamic_cast<typename target_type_t::reference>(view)
    // to explicitly expose our intention to fail in 'noexcept' function
    // in case of wrong type
    auto impl = dynamic_cast<typename target_type_t::pointer>(&view);

    if (!impl) {
      LOG_TOPIC("62e7f", ERR, Logger::VIEWS)
          << "invalid convertion attempt from '" << typeid(Source).name() << "'"
          << " to '" << typeid(typename target_type_t::value_type).name() << "'";
      TRI_ASSERT(false);
    }

    return *impl;
#else
    return static_cast<typename target_type_t::reference>(view);
#endif
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief casts a specified 'LogicalView' to a provided Target type
  //////////////////////////////////////////////////////////////////////////////
  template <typename Target, typename Source>
  inline static typename meta::adjustConst<Source, Target>::pointer cast(Source* view) noexcept {
    typedef typename meta::adjustConst<
        Source, typename std::enable_if<std::is_base_of<LogicalView, Target>::value &&
                                            std::is_same<typename std::remove_const<Source>::type, LogicalView>::value,
                                        Target>::type>::pointer target_type_t;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    return dynamic_cast<target_type_t>(view);
#else
    return static_cast<target_type_t>(view);
#endif
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief queries properties of an existing view
  //////////////////////////////////////////////////////////////////////////////
  virtual Result appendVelocyPack(velocypack::Builder& builder,
                                  Serialization context) const override final;

  //////////////////////////////////////////////////////////////////////////////
  /// @return the current view is granted 'level' access
  //////////////////////////////////////////////////////////////////////////////
  bool canUse(arangodb::auth::Level const& level);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the category representing a logical view
  //////////////////////////////////////////////////////////////////////////////
  static Category const& category() noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a new view according to a definition
  /// @param view out-param for created view on success
  ///        on success non-null, on failure undefined
  /// @param vocbase database where the view resides
  /// @param definition the view definition
  /// @return success and sets 'view' or failure
  //////////////////////////////////////////////////////////////////////////////
  static Result create(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                       velocypack::Slice definition);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief drop an existing view
  //////////////////////////////////////////////////////////////////////////////
  virtual Result drop() override final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief calls the callback on every view found for the specified vocbase
  /// @param callback if false is returned then enumiration stops
  /// @return full enumeration finished successfully
  //////////////////////////////////////////////////////////////////////////////
  static bool enumerate(TRI_vocbase_t& vocbase,
                        std::function<bool(std::shared_ptr<LogicalView> const&)> const& callback);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief instantiates an existing view according to a definition
  /// @param vocbase database where the view resides
  /// @param definition the view definition
  /// @return view instance or nullptr on error
  //////////////////////////////////////////////////////////////////////////////
  static Result instantiate(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                            velocypack::Slice definition);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief opens an existing view when the server is restarted
  //////////////////////////////////////////////////////////////////////////////
  virtual void open() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief renames an existing view
  //////////////////////////////////////////////////////////////////////////////
  virtual Result rename(std::string&& newName) override final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief invoke visitor on all collections that a view will return
  /// @return visitation was successful
  //////////////////////////////////////////////////////////////////////////////
  virtual bool visitCollections(CollectionVisitor const& visitor) const = 0;

 protected:
  LogicalView(TRI_vocbase_t& vocbase, velocypack::Slice const& definition, uint64_t planVersion);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief queries properties of an existing view
  //////////////////////////////////////////////////////////////////////////////
  virtual Result appendVelocyPackImpl(velocypack::Builder& builder,
                                     Serialization context) const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief drop implementation-specific parts of an existing view
  ///        including persisted properties
  //////////////////////////////////////////////////////////////////////////////
  virtual Result dropImpl() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief renames implementation-specific parts of an existing view
  ///        including persistance of properties
  //////////////////////////////////////////////////////////////////////////////
  virtual Result renameImpl(std::string const& oldName) = 0;

 private:
  // FIXME seems to be ugly
  friend struct ::TRI_vocbase_t;

  // ensure LogicalDataSource members (e.g. _deleted/_name) are not modified
  // asynchronously
  mutable basics::ReadWriteLock _lock;
};  // LogicalView

////////////////////////////////////////////////////////////////////////////////
/// @brief a helper for ClusterInfo View operations
////////////////////////////////////////////////////////////////////////////////
struct LogicalViewHelperClusterInfo {
  static Result construct(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                          velocypack::Slice const& definition) noexcept;

  static Result drop(LogicalView const& view) noexcept;

  static Result properties(velocypack::Builder& builder, LogicalView const& view) noexcept;

  static Result properties(LogicalView const& view) noexcept;

  static Result rename(LogicalView const& view, std::string const& oldName) noexcept;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief a helper for StorageEngine View operations
////////////////////////////////////////////////////////////////////////////////
struct LogicalViewHelperStorageEngine {
  static Result construct(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                          velocypack::Slice const& definition) noexcept;

  static Result destruct(LogicalView const& view) noexcept;
  static Result drop(LogicalView const& view) noexcept;

  static Result properties(velocypack::Builder& builder, LogicalView const& view) noexcept;

  static Result properties(LogicalView const& view) noexcept;

  static Result rename(LogicalView const& view, std::string const& oldName) noexcept;
};

}  // namespace arangodb

#endif
