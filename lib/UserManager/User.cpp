////////////////////////////////////////////////////////////////////////////////
/// @brief a user
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "User.h"

#include <fstream>

#include "Basics/FileUtils.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Logger/Logger.h"
#include "UserManager/Role.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::admin;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief registry lock
////////////////////////////////////////////////////////////////////////////////

static Mutex UserLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief user registry
////////////////////////////////////////////////////////////////////////////////

static map<string, User*> UserRegistry;

////////////////////////////////////////////////////////////////////////////////
/// @brief path to database
////////////////////////////////////////////////////////////////////////////////

static string userDatabase;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new user
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a role by name
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a user
///
/// Remove will also delete the user object.
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a role by name
////////////////////////////////////////////////////////////////////////////////

vector<User*> User::users () {
  MUTEX_LOCKER(UserLock);
  
  vector<User*> result;
  
  for (map<string, User*>::const_iterator i = UserRegistry.begin();  i != UserRegistry.end();  ++i) {
    result.push_back(i->second);
  }
  
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clear the user database
////////////////////////////////////////////////////////////////////////////////

void User::unloadUsers () {
  MUTEX_LOCKER(UserLock);
  
  for (map<string, User*>::const_iterator i = UserRegistry.begin();  i != UserRegistry.end();  ++i) {
    delete i->second;
  }

  UserRegistry.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads the user database
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief saves the user database
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new user
////////////////////////////////////////////////////////////////////////////////

User::User (string const& name, Role* role)
  : _name(name), _password(""), _role(role) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the name of a user
////////////////////////////////////////////////////////////////////////////////

string const& User::getName () const {
  return _name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the role of a user
////////////////////////////////////////////////////////////////////////////////

Role* User::getRole () const {
  return _role;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief changes the password of a user
////////////////////////////////////////////////////////////////////////////////

bool User::changePassword (string const& password) {
  _password = password;
  saveUser();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the password of a user
////////////////////////////////////////////////////////////////////////////////

bool User::checkPassword (string const& password) {
  return _password == password;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief has right
////////////////////////////////////////////////////////////////////////////////

bool User::hasRight (right_t right) {
  if (_role == 0) {
    return false;
  }
  else {
    set<right_t> const& rights = _role->getRights();
    
    return rights.find(right) != rights.end();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
