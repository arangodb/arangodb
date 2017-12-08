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
  void getPropertiesVPack(velocypack::Builder&, bool) const override;

  /// @brief opens an existing view
  void open() override;

  /// @brief drops an existing view
  void drop() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief this method will be called at query execution, when the AQL query
  /// engine requests data from the view.
  ///
  /// It will get the specialized filter condition and the sort condition from
  /// the previous calls. It must return a ViewIterator which the AQL query
  /// engine will use for fetching results from the view.
  //////////////////////////////////////////////////////////////////////////////
  virtual ViewIterator* iteratorForCondition(
      transaction::Methods* trx,
      arangodb::aql::ExecutionPlan* plan,
      arangodb::aql::ExpressionContext* ctx,
      arangodb::aql::Variable const* reference,
      arangodb::aql::AstNode const* filterCondition,
      arangodb::aql::SortCondition const* sortCondition) {
    return nullptr;
  }

 private:
  // example data
  arangodb::LogLevel _level;
};

}  // namespace arangodb

#endif
