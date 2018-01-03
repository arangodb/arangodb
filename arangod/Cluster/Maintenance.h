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

#ifndef ARANGODB_MAINTENANCE_MAINTENANCE_H
#define ARANGODB_MAINTENANCE_MAINTENANCE_H



namespace arangodb {
namespace maintenance {

/// @brief Main class of maintenance infrastructure
class Maintenance : public Thread {

public:
  
  /// @brief Construct
  Maintenance();

  /// @brief Clean up
  ~Maintenance();

private:

  /// @brief execute snapshot of plan
  arangodb::Result executePlan(Node const plan, Node const current);

  /// @brief execute snapshot of plan for databases
  arangodb::Result executeDatabasePlans(Node const plan, Node const current);

  /// @brief execute snapshot of plan for collections
  arangodb::Result executeCollectionPlans(Node const plan, Node const current);

};

}}

#endif

