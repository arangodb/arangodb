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

/// @brief Maintenance operation description card
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

  /// @brief Get a description key
  std::string get(std::string const& key) const;

  /// @brief Set a description key
  void set(std::string const& key, std::string const& value);

  /// @brief Set a description key
  void set(std::pair<std::string, std::string> const& kvpair);

  /// @brief Print this action to ostream
  VPackBuilder toVelocyPack() const;

  /// @brief Print this action to ostream
  std::ostream& print(std::ostream&) const;

private:
  /// @brief Action description
  std::map<std::string, std::string> _description;

  /// @bries Properties passed on to actual action
  VPackBuilder _properties;
};

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

