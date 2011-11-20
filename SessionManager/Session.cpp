////////////////////////////////////////////////////////////////////////////////
/// @brief session management
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

#include "Session.h"

#include <Basics/Logger.h>
#include <Basics/MutexLocker.h>
#include <Basics/Random.h>

#include "UserManager/User.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::admin;

// -----------------------------------------------------------------------------
// session registry
// -----------------------------------------------------------------------------

namespace {
  map<string, Session*> SessionRegistry;
  list<Session*> Sessions;
}

namespace triagens {
  namespace admin {

    // -----------------------------------------------------------------------------
    // constants
    // -----------------------------------------------------------------------------

    string const Session::SID_CHARACTERS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    Mutex Session::lock;
    set<right_t> Session::_rights;

    // -----------------------------------------------------------------------------
    // static public methods
    // -----------------------------------------------------------------------------

    Session* Session::lookup (string const& sid) {
      map<string, Session*>::const_iterator i = SessionRegistry.find(sid);

      if (i == SessionRegistry.end()) {
        return 0;
      }
      else {
        Session* session = i->second;

        list<Session*>::iterator j = find(Sessions.begin(), Sessions.end(), session);

        if (j != Sessions.end()) {
          Sessions.erase(j);
        }

        Sessions.push_back(session);

        return session;
      }
    }



    Session* Session::create () {
      if (Sessions.size() > MAXIMAL_OPEN_SESSION) {
        Session* session = Sessions.front();

        LOGGER_DEBUG << "too many admin sessions, throwing away '" << session->getSid() << "'";
        Sessions.pop_front();

        remove(session);
      }

      Random::UniformCharacter generator(SID_LENGTH, SID_CHARACTERS);

      string sid = generator.random();

      while (SessionRegistry.find(sid) != SessionRegistry.end()) {
        sid = generator.random();
      }

      Session* session = new Session(sid);

      SessionRegistry[sid] = session;
      Sessions.push_back(session);

      LOGGER_DEBUG << "created admin session '" << session->getSid() << "'";

      return session;
    }



    bool Session::remove (Session* session) {
      LOGGER_DEBUG << "removing admin session '" << session->getSid() << "'";

      SessionRegistry.erase(session->getSid());

      list<Session*>::iterator j = find(Sessions.begin(), Sessions.end(), session);

      if (j != Sessions.end()) {
        Sessions.erase(j);
      }

      delete session;

      return true;
    }

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------


    Session::Session (string const& sid)
      : _sid(sid), _user(0) {
    }


    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------


    bool Session::login (string const& username, string const& password, string& reason) {
      User* user = User::lookup(username);

      if (user == 0) {
        reason = "unknown user '" + username + "'";
        return false;
      }

      bool ok = user->checkPassword(password);

      if (! ok) {
        reason = "wrong password for user '" + username + "'";
        return false;
      }

      _user = user;

      return true;
    }



    bool Session::logout () {
      if (_user != 0) {
        _user = 0;
      }

      return true;
    }



    bool Session::hasRight (right_t right) {
      if (_user == 0) {
        return _rights.find(right) != _rights.end();
      }
      else {
        return _user->hasRight(right);
      }
    }




    void Session::setAnonymousRights (vector<right_t> const& rights) {
      _rights.insert(rights.begin(), rights.end());
    }



    set<right_t> const& Session::anonymousRights () {
      return _rights;
    }
  }
}
