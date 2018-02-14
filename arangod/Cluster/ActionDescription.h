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

static std::string const KEY("_key");
static std::string const FIELDS("fields");
static std::string const TYPE("type");
static std::string const INDEXES("indexes");
static std::string const SHARDS("shards");
static std::string const DATABASE("database");
static std::string const COLLECTION("collection");
static std::string const EDGE("edge");
static std::string const NAME("name");
static std::string const ID("id");

/**
 * @brief Action description for mainenance actions
 *
 * This structure holds once initialized constant parameters of a maintenance
 * action. Members are declared const, thus thread safety guards are ommited.
 */
struct ActionDescription {
  
public:
  
  /// @brief Construct with properties
  ActionDescription(std::map<std::string, std::string> const&,
                    VPackBuilder const& = VPackBuilder());

  /// @brief Clean up
  virtual ~ActionDescription();

  /// @brief Equality
  bool operator== (ActionDescription const&) const noexcept;

  /// @brief Hash function
  std::size_t hash() const;

  /// @brief Name of action
  std::string name() const;

  /// @brief Check for a description key
  bool has(std::string const& p) const noexcept;

  /**
   * @brief Get a string value from description
   *
   * @param  key   Key to get
   * @exception    std::out_of_range if the we do not have an element
   *               with the specified key
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

  /// @brief Print this action to ostream
  VPackBuilder toVelocyPack() const;

  /// @brief Print this action to ostream
  std::string toJson() const;

  /// @brief Print this action to ostream
  std::ostream& print(std::ostream&) const;

  /**
   * @brief Hand out a pointer to our roperties
   *
   * @return           pointer to our roperties
   */
  VPackSlice properties() const;
  
private:
  /// @brief Action description
  std::map<std::string, std::string> const _description;

  /// @brief Properties passed on to actual action
  VPackBuilder const _properties;
};

inline std::ostream& operator<< (std::ostream& o, ActionDescription const& d) {
  o << d.toJson();
  return o;
}

}}

namespace std {
/// @brief Hash function used by std::unordered_map<ActionDescription,...>
template<> struct hash<arangodb::maintenance::ActionDescription> {
  typedef arangodb::maintenance::ActionDescription argument_t;
  typedef std::size_t result_t;
  result_t operator()(argument_t const& a) const noexcept;
};

}

#endif

