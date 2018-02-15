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

#ifndef ARANGODB_CLUSTER_MAINTENANCE_ACTION_DESCRIPTION_H
#define ARANGODB_CLUSTER_MAINTENANCE_ACTION_DESCRIPTION_H

#include "Basics/VelocyPackHelper.h"

#include <map>
#include <string>

namespace arangodb {
namespace maintenance {

enum Signal { GRACEFUL, IMMEDIATE };

static std::string const KEY;
static std::string const FIELDS;
static std::string const TYPE;
static std::string const INDEXES;
static std::string const SHARDS;
static std::string const DATABASE;
static std::string const COLLECTION;
static std::string const EDGE;
static std::string const NAME;
static std::string const ID;
static std::string const LEADER;
static std::string const GLOB_UID;
static std::string const OBJECT_ID;

/**
 * @brief Action description for mainenance actions
 *
 * This structure holds once initialized constant parameters of a maintenance
 * action. Members are declared const, thus thread safety guards are ommited.
 */
struct ActionDescription {
  
public:
  
  /**
   * @brief Construct with properties
   * @param  desc  Descriminatory properties, which are considered for hash
   * @param  supp  Non discriminatory properties
   */
  ActionDescription(std::map<std::string, std::string> const& desc,
                    VPackBuilder const& suppl = VPackBuilder());

  /**
   * @brief Clean up
   */ 
  virtual ~ActionDescription();

  /**
   * @brief Check equality (only _description considered)
   * @param  other  Other descriptor
   */
  bool operator== (ActionDescription const& other) const noexcept;

  /**
   * @brief Calculate hash of _description as concatenation
   * @param  other  Other descriptor
   */
  std::size_t hash() const noexcept;

  /// @brief Name of action
  std::string const& name() const noexcept;

  /**
   * @brief Check if key exists in discrimantory container
   * @param  key   Key to lookup
   * @return       true if key is found
   */
  bool has(std::string const& keu) const noexcept;

  /**
   * @brief Get a string value from description
   * @param  key   Key to get
   * @exception    std::out_of_range if the we do not have this key in discrimatory container
   * @return       Value to specified key
   */
  std::string get(std::string const& key) const;

  /**
   * @brief Get a string value from description
   *
   * @param  key   Key to get
   * @param  value If key is found the value is assigned to this variable
   * @return       Success (key found?)
   */
  Result get(std::string const& key, std::string& value) const noexcept;

  /**
   * @brief Dump to JSON(vpack)
   * @return       JSON Velocypack of all paramters
   */
  VPackBuilder toVelocyPack() const;

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
  VPackSlice properties() const noexcept;
  
private:

  /// Note: members are const. No thread safety guards needed.
  
  /** @brief discriminatory properties */
  std::map<std::string, std::string> const _description; 

  /** @brief non-discriminatory properties */
  VPackBuilder const _properties;
  
};

}}

std::ostream& operator<< (
  std::ostream& o, arangodb::maintenance::ActionDescription const& d);

namespace std {
/// @brief Hash function used by std::unordered_map<ActionDescription,...>
template<> struct hash<arangodb::maintenance::ActionDescription> {
  typedef arangodb::maintenance::ActionDescription argument_t;
  typedef std::size_t result_t;
  result_t operator()(argument_t const& a) const noexcept;
};

}

#endif

