////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_CLUSTER_MAINTENANCE_ACTION_DESCRIPTION_H
#define ARANGODB_CLUSTER_MAINTENANCE_ACTION_DESCRIPTION_H

#include "MaintenanceStrings.h"

#include "Basics/VelocyPackHelper.h"

#include <map>
#include <string>

namespace arangodb {
namespace maintenance {

//
// state accessor and set functions
//  (some require time checks and/or combination tests)
//
enum ActionState {
  READY = 1,        // waiting for a worker on the deque
  EXECUTING = 2,    // user or worker thread currently executing
  WAITING = 3,      // initiated a pre-task, waiting for its completion
  WAITINGPRE = 4,   // parent task created, about to execute on parent's thread
  WAITINGPOST = 5,  // parent task created, will execute after parent's success
  PAUSED = 6,       // (not implemented) user paused task
  COMPLETE = 7,     // task completed successfully
  FAILED = 8,       // task failed, no longer executing
};

/**
 * @brief Action description for maintenance actions
 *
 * This structure holds once initialized constant parameters of a maintenance
 * action. Members are declared const, thus thread safety guards are omitted.
 */
struct ActionDescription final {
  /**
   * @brief Construct with properties
   * @param  desc  Descriminatory properties, which are considered for hash
   * @param  properties  Non discriminatory properties
   */
  ActionDescription(
      std::map<std::string, std::string> description,
      int priority,
      bool runEvenIfDuplicate,
      std::shared_ptr<VPackBuilder> properties = std::make_shared<VPackBuilder>());

  /**
   * @brief Clean up
   */
  ~ActionDescription();

  /**
   * @brief Check equality (only _description considered)
   * @param  other  Other descriptor
   */
  bool operator==(ActionDescription const& other) const;

  /**
   * @brief Calculate hash of _description as concatenation
   * @param  other  Other descriptor
   */
  std::size_t hash() const noexcept;
  static std::size_t hash(std::map<std::string, std::string> const& desc) noexcept;

  /// @brief Name of action
  std::string const& name() const;

  /**
   * @brief Check if key exists in discrimantory container
   * @param  key   Key to lookup
   * @return       true if key is found
   */
  bool has(std::string const& key) const;

  /**
   * @brief Get a string value from description
   * @param  key   Key to get
   * @exception    std::out_of_range if the we do not have this key in discrimatory container
   * @return       Value to specified key
   */
  std::string get(std::string const& key) const;

  /**
   * @brief Get a string value from description
   * @param  key   Key to get
   * @exception    std::out_of_range if the we do not have this key in discrimatory container
   * @return       Value to specified key
   */
  std::string operator()(std::string const& key) const;

  /**
   * @brief Get a string value from description
   *
   * @param  key   Key to get
   * @param  value If key is found the value is assigned to this variable
   * @return       Success (key found?)
   */
  Result get(std::string const& key, std::string& value) const;

  /**
   * @brief Dump to JSON(vpack)
   * @return       JSON Velocypack of all paramters
   */
  VPackBuilder toVelocyPack() const;

  /**
   * @brief Dump to JSON(vpack)
   * @return       JSON Velocypack of all paramters
   */
  void toVelocyPack(VPackBuilder&) const;

  /**
   * @brief Dump to JSON(string)
   * @return       JSON string of all paramters
   */
  std::string toJson() const;

  /**
   * @brief Dump to JSON(output stream)
   * @param  os    Output stream reference
   * @return       Output stream reference
   */
  std::ostream& print(std::ostream& os) const;

  /**
   * @brief Get non discrimantory properties
   *            Get non discrimantory properties.
   *            This function does not throw as builder is always when here.
   * @return    Non discriminatory properties
   */
  std::shared_ptr<VPackBuilder> const properties() const;

  /**
   * @brief Get priority, the higher the more priority, 1 is default
   * @return int
   */
  int priority() const {
    return _priority;
  }

  /**
   * @brief Get the fact if it is forced or not. If forced, the MaintenanceFeature
   * will not sort out duplicates by hashing the description. Rather, the action
   * will always be submitted.
   */
  bool isRunEvenIfDuplicate() const {
    return _runEvenIfDuplicate;
  }

 private:
  /** @brief discriminatory properties */
  std::map<std::string, std::string> const _description;

  /** @brief non-discriminatory properties */
  std::shared_ptr<VPackBuilder> const _properties;

  /** @brief priority */
  int _priority;

  /// @brief flag to not sort out duplicates by hashing
  bool _runEvenIfDuplicate;
};

}  // namespace maintenance
}  // namespace arangodb

namespace std {
ostream& operator<<(ostream& o, arangodb::maintenance::ActionDescription const& d);
/// @brief Hash function used by std::unordered_map<ActionDescription,...>
template <>
struct hash<arangodb::maintenance::ActionDescription> {
  typedef arangodb::maintenance::ActionDescription argument_t;
  typedef std::size_t result_t;
  result_t operator()(argument_t const& a) const noexcept;
};

}  // namespace std

#endif
