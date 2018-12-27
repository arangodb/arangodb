////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IRESEARCH__IRESEARCH_VIEW_SINGLE_SERVER_H
#define ARANGODB_IRESEARCH__IRESEARCH_VIEW_SINGLE_SERVER_H 1

#include "IResearchView.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
namespace iresearch {

///////////////////////////////////////////////////////////////////////////////
/// @class IResearchViewSingleServer
/// @brief an abstraction over the distributed IResearch index implementing the
///        LogicalView interface
///////////////////////////////////////////////////////////////////////////////
class IResearchViewSingleServer final {
 public:
  ///////////////////////////////////////////////////////////////////////////////
  /// @brief view factory
  /// @returns initialized view object
  ///////////////////////////////////////////////////////////////////////////////
  static std::shared_ptr<LogicalView> make(TRI_vocbase_t& vocbase,
                                           velocypack::Slice const& info,
                                           bool isNew, uint64_t planVersion,
                                           LogicalView::PreCommitCallback const& preCommit);
};

}  // namespace iresearch
}  // namespace arangodb

#endif  // ARANGODB_IRESEARCH__IRESEARCH_VIEW_SINGLE_SERVER_H
