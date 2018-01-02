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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CLUSTER_MAINTENANCE_ACTION_H
#define ARANGODB_CLUSTER_MAINTENANCE_ACTION_H

#include "ActionDescription.h"

#include "lib/Basics/Result.h"

#include <chrono>

namespace arangodb {
namespace maintenance {

class ActionBase;

class Action {

public:

  /// @brief construct with description
  Action(ActionDescription const&);
  
  /// @brief clean up
  virtual ~Action();
  
  /// @brief run for some time and tell, if need more time or done
  arangodb::Result run(
    std::chrono::duration<double> const&, bool& finished);

  /// @brief kill action with signal
  arangodb::Result kill(Signal const& signal);
  
  /// @brief check progress
  arangodb::Result progress(double& progress);

  /// @brief describe action
  ActionDescription describe() const;
  
private:
  
  ActionBase* _action;
};

}}
#endif
