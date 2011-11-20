////////////////////////////////////////////////////////////////////////////////
/// @brief role of a user
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Role.h"

#include <Basics/Logger.h>
#include <Basics/Mutex.h>
#include <Basics/MutexLocker.h>

using namespace std;
using namespace triagens::basics;
using namespace triagens::admin;

// -----------------------------------------------------------------------------
// role regestry
// -----------------------------------------------------------------------------

namespace {
  Mutex RoleLock;
  map<string, Role*> RoleRegistry;
}

namespace triagens {
  namespace admin {

    // -----------------------------------------------------------------------------
    // static public methods
    // -----------------------------------------------------------------------------

    Role* Role::create (string const& name, right_t manage) {
      MUTEX_LOCKER(RoleLock);

      map<string, Role*>::const_iterator i = RoleRegistry.find(name);

      if (i != RoleRegistry.end()) {
        LOGGER_DEBUG << "user '" << name << "' already exists";
        return 0;
      }

      Role* user = new Role(name, manage);

      RoleRegistry[name] = user;

      return user;
    }



    Role* Role::lookup (string const& name) {
      MUTEX_LOCKER(RoleLock);

      map<string, Role*>::const_iterator i = RoleRegistry.find(name);

      if (i == RoleRegistry.end()) {
        return 0;
      }
      else {
        return i->second;
      }
    }

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    Role::Role (string const& name, right_t manage)
      : _name(name), _rightToManage(manage) {
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    void Role::addRight (right_t right) {
      _rights.insert(right);
    }



    void Role::setRights (vector<right_t> const& rights) {
      _rights.clear();
      _rights.insert(rights.begin(), rights.end());
    }



    void Role::removeRight (right_t right) {
      _rights.erase(right);
    }



    void Role::clearRights () {
      _rights.clear();
    }
  }
}
