////////////////////////////////////////////////////////////////////////////////
/// @brief a user
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

#include "User.h"

#include <fstream>

#include <Basics/StringUtils.h>
#include <Basics/FileUtils.h>
#include <Basics/Logger.h>
#include <Basics/Mutex.h>
#include <Basics/MutexLocker.h>

#include "UserManager/Role.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::admin;

// -----------------------------------------------------------------------------
// user registry
// -----------------------------------------------------------------------------

namespace {
  Mutex UserLock;
  map<string, User*> UserRegistry;
  string userDatabase;
}

namespace triagens {
  namespace admin {

    // -----------------------------------------------------------------------------
    // static public methods
    // -----------------------------------------------------------------------------

    User* User::create (string const& name, Role* role) {
      MUTEX_LOCKER(UserLock);

      map<string, User*>::const_iterator i = UserRegistry.find(name);

      if (i != UserRegistry.end()) {
        LOGGER_DEBUG << "user '" << name << "' already exists";
        return 0;
      }

      User* user = new User(name, role);

      UserRegistry[name] = user;
      saveUser();

      return user;
    }



    User* User::lookup (string const& name) {
      MUTEX_LOCKER(UserLock);

      map<string, User*>::const_iterator i = UserRegistry.find(name);

      if (i == UserRegistry.end()) {
        return 0;
      }
      else {
        return i->second;
      }
    }



    bool User::remove (User* user) {
      MUTEX_LOCKER(UserLock);

      map<string, User*>::iterator i = UserRegistry.find(user->_name);

      if (i == UserRegistry.end()) {
        LOGGER_DEBUG << "user '" << user->_name << "' is unknown";
        return false;
      }

      if (i->second != user) {
        LOGGER_WARNING << "user '" << user->_name << "' does not matched stored profile";
        return false;
      }

      if (! user->hasRight(RIGHT_TO_BE_DELETED)) {
        LOGGER_WARNING << "user '" << user->_name << "' cannot be deleted";
        return false;
      }

      UserRegistry.erase(i);
      saveUser();

      // This is a simple user manager, we are not deleting any old users.
      // Otherwise we would need to create locks outside the "lookup" function
      // to avoid deleting the current user, or we would need to use smart
      // pointer or objects.

      // delete user;

      return true;
    }



    vector<User*> User::users () {
      MUTEX_LOCKER(UserLock);

      vector<User*> result;

      for (map<string, User*>::const_iterator i = UserRegistry.begin();  i != UserRegistry.end();  ++i) {
        result.push_back(i->second);
      }

      return result;
    }



    bool User::loadUser (string const& path) {
      userDatabase = path;
      bool found = true;

      if (userDatabase.empty()) {
        return false;
      }

      if (! FileUtils::exists(userDatabase)) {
        ofstream* of = FileUtils::createOutput(userDatabase);

        if (of == 0) {
          LOGGER_FATAL << "cannot create user database '" << userDatabase << "'";
          exit(EXIT_FAILURE);
        }

        *of << "# EMPTY USER DATABASE\n";

        if (of->fail()) {
          LOGGER_FATAL << "cannot create user database '" << userDatabase << "'";
          exit(EXIT_FAILURE);
        }

        delete of;
        found = false;
      }

      ifstream* is = FileUtils::createInput(userDatabase);

      if (is == 0) {
        LOGGER_FATAL << "cannot read user database '" << userDatabase << "'";
        return false;
      }

      while (true) {
        string line;
        getline(*is, line);

        if (is->eof()) {
          break;
        }

        if (is->fail()) {
          LOGGER_FATAL << "cannot read user database '" << userDatabase << "'";
          return false;
        }

        if (line.empty() || line[0] == '#') {
          continue;
        }

        vector<string> split = StringUtils::split(line, ';');

        if (split.size() != 3) {
          LOGGER_FATAL << "corrupted user data '" << userDatabase << "'";
          return false;
        }

        string const& name = split[2];
        Role* role = Role::lookup(name);

        if (role == 0) {
          LOGGER_ERROR << "unknown role '" << name << "' in user database";
          continue;
        }

        User* user = new User(split[0], role);
        user->changePassword(split[1]);

        UserRegistry[name] = user;
      }

      return found;
    }



    void User::saveUser () {
      if (userDatabase.empty()) {
        LOGGER_DEBUG << "no user database defined, cannot save data";
        return;
      }

      string tmp = userDatabase + ".tmp";

      if (FileUtils::exists(tmp)) {
        if (! FileUtils::remove(tmp)) {
          LOGGER_FATAL << "cannot remove the temporary user database '" << tmp << "'";
          return;
        }
      }

      ofstream* of = FileUtils::createOutput(tmp);

      if (of == 0) {
        LOGGER_FATAL << "cannot create the temporary user data '" << tmp << "'";
        return;
      }

      for (map<string, User*>::const_iterator i = UserRegistry.begin();  i != UserRegistry.end();  ++i) {
        User* user = i->second;

        *of << StringUtils::escape(user->_name, ";") << ";"
            << StringUtils::escape(user->_password, ";") << ";"
            << StringUtils::escape(user->_role->getName(), ";")
            << "\n";

        if (of->fail()) {
          LOGGER_FATAL << "cannot write the temporary user data '" << tmp << "'";
          delete of;
          FileUtils::remove(tmp);
          return;
        }
      }

      delete of;

      FileUtils::remove(userDatabase);

      if (! FileUtils::rename(tmp, userDatabase)) {
        LOGGER_FATAL << "could not rename the temporary user database '" << tmp << "'";
        return;
      }

      LOGGER_DEBUG << "wrote user database '" << userDatabase << "'";
    }

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    User::User (string const& name, Role* role)
      : _name(name), _password(""), _role(role) {
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    bool User::changePassword (string const& password) {
      _password = password;
      saveUser();
      return true;
    }



    bool User::checkPassword (string const& password) {
      return _password == password;
    }



    bool User::hasRight (right_t right) {
      if (_role == 0) {
        return false;
      }
      else {
        set<right_t> const& rights = _role->getRights();

        return rights.find(right) != rights.end();
      }
    }
  }
}
