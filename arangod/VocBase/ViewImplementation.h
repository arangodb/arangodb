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
class ViewIterator;

namespace aql {
  class Ast;
  class AstNode;
  class SortCondition;
  struct Variable;
};

namespace transaction {
  class Methods;
};

/// @brief interface for view implementation
class ViewImplementation {
 protected:
  ViewImplementation(LogicalView* logical,
                     arangodb::velocypack::Slice const& info)
      : _logicalView(logical) {}

 public:
  virtual ~ViewImplementation() = default;

  /// @brief called when a view's properties are updated
  virtual arangodb::Result updateProperties(
      arangodb::velocypack::Slice const& slice, bool partialUpdate,
      bool doSync) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief called when a view's properties are materialized into
  /// the VelocyPack Builder passed into the method. the implementation
  /// is supposed to fill in all its specific properties. The Builder
  /// points into an open VelocyPack object. The method is supposed to
  /// add all its own property attributes with their values, and must
  /// not close the Builder
  //////////////////////////////////////////////////////////////////////////////
  virtual void getPropertiesVPack(velocypack::Builder&) const = 0;

  /// @brief opens an existing view when the server is restarted
  virtual void open() = 0;

  /// @brief drops an existing view
  virtual void drop() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief this method may be called by the AQL optimizer to check if the view
  /// supports a filter condition (as identified by the AST node `node`) at
  /// least partially. The AQL variable `reference` is the variable that was
  /// used for accessing the view in the AQL query.
  ///
  /// For example, in the AQL query
  ///
  ///   FOR doc IN VIEW myView
  ///     FILTER doc.foo == 'bar'
  ///     RETURN doc
  ///
  /// the filter condition would be `doc.foo == 'bar'` (represented in an AST)
  /// and the variable would be `doc`.
  ///
  /// The view can return an estimate for the number of items to return and can
  /// provide an estimated cost value. Note that these return values are
  /// informational only (e.g. for displaying them in the AQL explain output).
  //////////////////////////////////////////////////////////////////////////////
  virtual bool supportsFilterCondition(arangodb::aql::AstNode const* node,
                                       arangodb::aql::Variable const* reference,
                                       size_t& estimatedItems,
                                       double& estimatedCost) const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief this method may be called by the AQL optimizer to check if the
  /// view supports a particular sort condition (as identified by the
  /// `sortCondition` passed) at least partially. The AQL variable `reference`
  /// identifies the variable in the AQL query that is used to access the view.
  ///
  /// For example, in the AQL query
  ///
  ///   FOR doc IN VIEW myView
  ///     SORT doc.foo, doc.bar
  ///     RETURN doc
  ///
  /// the sort condition would be `[ doc.foo, doc.bar ]` (represented in an
  /// AST) and the variable would be `doc`.
  ///
  /// The view can return an estimated cost value. Note that this value is
  /// informational only  (e.g. for displaying them in the AQL explain output).
  /// It must also return the number of sort condition parts that are covered
  /// by the view, from left to right, starting at the first sort condition
  /// part. If the first sort condition part is not covered by the view, then
  /// `coveredAttributes` must be `0`.
  //////////////////////////////////////////////////////////////////////////////
  virtual bool supportsSortCondition(
      arangodb::aql::SortCondition const* sortCondition,
      arangodb::aql::Variable const* reference, double& estimatedCost,
      size_t& coveredAttributes) const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief this is called for a filter condition that was previously handed
  /// to the view in `supportsFilterCondition`, and for which the view has
  /// claimed to support it (at least partially).
  ///
  /// This call gives the view the chance to filter out all parts of the filter
  /// condition that it cannot handle itself.
  ///
  /// If the view does not support the entire filter condition (or needs to
  /// rewrite the filter condition somehow), it may return a new AstNode. It
  /// not allowed to modify the AstNode.
  //////////////////////////////////////////////////////////////////////////////
  virtual arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::Ast* ast, arangodb::aql::AstNode const* node,
      arangodb::aql::Variable const* reference) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief this method will be called at query execution, when the AQL query
  /// engine requests data from the view.
  ///
  /// It will get the specialized filter condition and the sort condition from
  /// the previous calls. It must return a ViewIterator which the AQL query
  /// engine will use for fetching results from the view.
  //////////////////////////////////////////////////////////////////////////////
  virtual ViewIterator* iteratorForCondition(
      transaction::Methods* trx, arangodb::aql::AstNode const* node,
      arangodb::aql::Variable const* reference,
      arangodb::aql::SortCondition const* sortCondition) = 0;

 protected:
  LogicalView* _logicalView;
};

//////////////////////////////////////////////////////////////////////////////
/// @brief typedef for a ViewImplementation creator function
/// This typedef is used when registering the creator function for any view
/// type. the creator function is called when a view is first created or
/// re-opened after a server restart. the VelocyPack Slice will contain all
/// information about the view's general and implementation-specific properties.
/// the isNew flag will be true if the view is first created, and false if a
/// view is re-opened on a server restart.
//////////////////////////////////////////////////////////////////////////////
typedef std::function<std::unique_ptr<ViewImplementation>(
    LogicalView*, arangodb::velocypack::Slice const&, bool isNew)>
    ViewCreator;

}  // namespace arangodb

#endif
