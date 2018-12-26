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

#ifndef ARANGOD_CLUSTER_SERVER_STATE_H
#define ARANGOD_CLUSTER_SERVER_STATE_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"

namespace arangodb {
class AgencyComm;

class ServerState {
 public:
  /// @brief an enum describing the roles a server can have
  enum RoleEnum : int {
    ROLE_UNDEFINED = 0,  // initial value
    ROLE_SINGLE,         // is set when cluster feature is off
    ROLE_PRIMARY,
    ROLE_SECONDARY,
    ROLE_COORDINATOR,
    ROLE_AGENT
  };

  static const std::vector<std::string> RoleStr;
  static const std::vector<std::string> RoleStrReadable;

  /// @brief an enum describing the possible states a server can have
  enum StateEnum {
    STATE_UNDEFINED = 0,  // initial value
    STATE_STARTUP,        // used by all roles
    STATE_SERVINGASYNC,   // primary only
    STATE_SERVINGSYNC,    // primary only
    STATE_STOPPING,       // primary only
    STATE_STOPPED,        // primary only
    STATE_SYNCING,        // secondary only
    STATE_INSYNC,         // secondary only
    STATE_LOSTPRIMARY,    // secondary only
    STATE_SERVING,        // coordinator only
    STATE_SHUTDOWN        // used by all roles
  };

 public:
  ServerState();

  ~ServerState();

 public:
  /// @brief create the (sole) instance
  static ServerState* instance();

  /// @brief get the string representation of a role
  static std::string roleToString(RoleEnum);

  /// @brief convert a string to a role
  static RoleEnum stringToRole(std::string const&);

  /// @brief get the string representation of a state
  static std::string stateToString(StateEnum);

  /// @brief convert a string representation to a state
  static StateEnum stringToState(std::string const&);

 public:
  /// @brief sets the initialized flag
  void setInitialized() { _initialized = true; }

  /// @brief whether or not the cluster was properly initialized
  bool initialized() const { return _initialized; }

  /// @brief sets the initialized flag
  void setClusterEnabled() { _clusterEnabled = true; }

  /// @brief flush the server state (used for testing)
  void flush();

  bool isSingleServer() { return isSingleServer(loadRole()); }

  static bool isSingleServer(ServerState::RoleEnum role) {
    return (role == ServerState::ROLE_SINGLE);
  }

  /// @brief check whether the server is a coordinator
  bool isCoordinator() { return isCoordinator(loadRole()); }

  /// @brief check whether the server is a coordinator
  static bool isCoordinator(ServerState::RoleEnum role) {
    return (role == ServerState::ROLE_COORDINATOR);
  }

  /// @brief check whether the server is a DB server (primary or secondary)
  /// running in cluster mode.
  bool isDBServer() { return isDBServer(loadRole()); }

  /// @brief check whether the server is a DB server (primary or secondary)
  /// running in cluster mode.
  static bool isDBServer(ServerState::RoleEnum role) {
    return (role == ServerState::ROLE_PRIMARY || role == ServerState::ROLE_SECONDARY);
  }

  /// @brief whether or not the role is a cluster-related role
  static bool isClusterRole(ServerState::RoleEnum role) {
    return (role == ServerState::ROLE_PRIMARY || role == ServerState::ROLE_SECONDARY ||
            role == ServerState::ROLE_COORDINATOR);
  }

  /// @brief check whether the server is an agent
  bool isAgent() { return isAgent(loadRole()); }

  /// @brief check whether the server is an agent
  static bool isAgent(ServerState::RoleEnum role) {
    return (role == ServerState::ROLE_AGENT);
  }

  /// @brief check whether the server is running in a cluster
  bool isRunningInCluster() { return isClusterRole(loadRole()); }

  /// @brief check whether the server is running in a cluster
  static bool isRunningInCluster(ServerState::RoleEnum role) {
    return isClusterRole(role);
  }

  bool isSingleServerOrCoordinator() {
    RoleEnum role = loadRole();
    return isCoordinator(role) || isSingleServer(role);
  }

  /// @brief get the server role
  RoleEnum getRole();

  bool integrateIntoCluster(RoleEnum, std::string const&, std::string const&);

  bool unregister();

  /// @brief set the server role
  void setRole(RoleEnum);

  /// @brief get the server local info
  std::string getLocalInfo();

  /// @brief get the server id
  std::string getId();

  /// @brief for a secondary get the server id of its primary
  std::string getPrimaryId();

  /// @brief get the server description
  std::string getDescription();

  /// @brief set the server local info
  void setLocalInfo(std::string const&);

  /// @brief set the server id
  void setId(std::string const&);

  /// @brief set the server description
  void setDescription(std::string const& description);

  /// @brief get the server address
  std::string getAddress();

  /// @brief set the server address
  void setAddress(std::string const&);

  /// @brief get the current state
  StateEnum getState();

  /// @brief set the current state
  void setState(StateEnum);

  /// @brief gets the JavaScript startup path
  std::string getJavaScriptPath();

  /// @brief sets the JavaScript startup path
  void setJavaScriptPath(std::string const&);

  /// @brief redetermine the server role, we do this after a plan change.
  /// This is needed for automatic failover. This calls determineRole with
  /// previous values of _info and _id. In particular, the _id will usually
  /// already be set. If the current role cannot be determined from the
  /// agency or is not unique, then the system keeps the old role.
  /// Returns true if there is a change and false otherwise.
  bool redetermineRole();

  bool isFoxxmaster();

  std::string const& getFoxxmaster();

  void setFoxxmaster(std::string const&);

  void setFoxxmasterQueueupdate(bool);

  bool getFoxxmasterQueueupdate();

  std::string getPersistedId();
  bool hasPersistedId();
  std::string generatePersistedId(RoleEnum const&);
  bool writePersistedId(std::string const&);

 private:
  /// @brief atomically fetches the server role
  RoleEnum loadRole() {
    return static_cast<RoleEnum>(_role.load(std::memory_order_consume));
  }

  /// @brief determine role and save role blocking
  void findAndSetRoleBlocking();

  /// @brief store the server role
  bool storeRole(RoleEnum role);

  /// @brief determine the server role
  RoleEnum determineRole(std::string const& info, std::string& id);

  /// @brief we are new and need to determine our role from the plan
  RoleEnum takeOnRole(std::string const& id);

  /// @brief lookup the server id by using the local info
  int lookupLocalInfoToId(std::string const& localInfo, std::string& id);

  /// @brief lookup the server role by scanning Plan/Coordinators for our id
  ServerState::RoleEnum checkCoordinatorsList(std::string const&);

  /// @brief lookup the server role by scanning Plan/DBServers for our id
  ServerState::RoleEnum checkServersList(std::string const&);

  /// @brief validate a state transition for a primary server
  bool checkPrimaryState(StateEnum);

  /// @brief validate a state transition for a secondary server
  bool checkSecondaryState(StateEnum);

  /// @brief validate a state transition for a coordinator server
  bool checkCoordinatorState(StateEnum);

  /// @brief register at agency
  bool registerAtAgency(AgencyComm&, const RoleEnum&, std::string const&);
  /// @brief register shortname for an id
  bool registerShortName(std::string const& id, const RoleEnum&);

  /// @brief get the key for a role in the agency
  static std::string roleToAgencyKey(RoleEnum);

  std::string getUuidFilename();

  /// @brief the pointer to the singleton instance
  static ServerState* _theinstance;

  /// @brief the server's local info, can be set just once
  std::string _localInfo;

  /// @brief the server's id, can be set just once
  std::string _id;

  /// @brief the server's description
  std::string _description;

  /// @brief the JavaScript startup path, can be set just once
  std::string _javaScriptStartupPath;

  /// @brief the server's own address, can be set just once
  std::string _address;

  /// @brief r/w lock for state
  arangodb::basics::ReadWriteLock _lock;

  /// @brief the server role
  std::atomic<int> _role;

  /// @brief a secondary stores the ID of its primary here:
  std::string _idOfPrimary;

  /// @brief the current state
  StateEnum _state;

  /// @brief whether or not the cluster was initialized
  bool _initialized;

  /// @brief whether or not we are a cluster member
  bool _clusterEnabled;

  std::string _foxxmaster;

  bool _foxxmasterQueueupdate;
};
}  // namespace arangodb

#endif
