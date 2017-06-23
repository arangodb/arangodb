////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VIEWS_LOGGER_VIEW_H
#define ARANGOD_VIEWS_LOGGER_VIEW_H 1

#include "Basics/Common.h"
#include "Logger/Logger.h"
#include "VocBase/ViewImplementation.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
class PhysicalView;
class Result;

class LoggerView final : public ViewImplementation {
 public:
  static std::string type;
  static std::unique_ptr<ViewImplementation> creator(
      LogicalView*, arangodb::velocypack::Slice const& info, bool isNew);

 private:
  // private struct that does not do anything
  // we require it in the constructor of LoggerView so we
  // can ensure any constructor calls are coming from the
  // LoggerView's creator method
  struct ConstructionGuard {};

 public:
  LoggerView(ConstructionGuard const&, LogicalView* logical,
             arangodb::velocypack::Slice const& info, bool isNew);

  ~LoggerView() = default;

  arangodb::Result updateProperties(arangodb::velocypack::Slice const& slice,
                                    bool partialUpdate, bool doSync) override;

  /// @brief export properties
  void getPropertiesVPack(velocypack::Builder&) const override;

  /// @brief opens an existing view
  void open() override;

  /// @brief drops an existing view
  void drop() override;

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
                                       double& estimatedCost) const { return false; }

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
      size_t& coveredAttributes) const { return false; }

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
      arangodb::aql::Variable const* reference) {}

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
      arangodb::aql::SortCondition const* sortCondition) { return nullptr; }

 private:
  // example data
  arangodb::LogLevel _level;
};

}  // namespace arangodb

#endif
