////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <errno.h>
#include <string.h>

#include "PrivilegeFeature.h"

#include "Basics/application-exit.h"
#include "Basics/error.h"

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef ARANGODB_HAVE_GETGRGID
#include <grp.h>
#endif

#ifdef ARANGODB_HAVE_GETPWUID
#include <pwd.h>
#endif

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/conversions.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Option.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

PrivilegeFeature::PrivilegeFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Privilege"), _numericUid(0), _numericGid(0) {
  setOptional(true);
  startsAfter<application_features::GreetingsFeaturePhase>();
}

void PrivilegeFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");

#ifdef ARANGODB_HAVE_SETUID
  options->addOption("--uid", "switch to user-id after reading config files",
                     new StringParameter(&_uid),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));

  options->addOption("--server.uid",
                     "switch to user-id after reading config files",
                     new StringParameter(&_uid),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));
#endif

#ifdef ARANGODB_HAVE_SETGID
  options->addOption("--gid", "switch to group-id after reading config files",
                     new StringParameter(&_gid),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));

  options->addOption("--server.gid",
                     "switch to group-id after reading config files",
                     new StringParameter(&_gid),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));
#endif
}

void PrivilegeFeature::prepare() { extractPrivileges(); }

void PrivilegeFeature::extractPrivileges() {
#ifdef ARANGODB_HAVE_SETGID
  if (_gid.empty()) {
    _numericGid = getgid();
  } else {
    int gidNumber = TRI_Int32String(_gid.c_str());

    if (TRI_errno() == TRI_ERROR_NO_ERROR && gidNumber >= 0) {
#ifdef ARANGODB_HAVE_GETGRGID
      group* g = getgrgid(gidNumber);

      if (g == nullptr) {
        LOG_TOPIC("3d53b", FATAL, arangodb::Logger::FIXME) << "unknown numeric gid '" << _gid << "'";
        FATAL_ERROR_EXIT();
      }
#endif
    } else {
#ifdef ARANGODB_HAVE_GETGRNAM
      std::string name = _gid;
      group* g = getgrnam(name.c_str());

      if (g != nullptr) {
        gidNumber = g->gr_gid;
      } else {
        TRI_set_errno(TRI_ERROR_SYS_ERROR);
        LOG_TOPIC("20096", FATAL, arangodb::Logger::FIXME)
            << "cannot convert groupname '" << _gid
            << "' to numeric gid: " << TRI_last_error();
        FATAL_ERROR_EXIT();
      }
#else
      LOG_TOPIC("ff815", FATAL, arangodb::Logger::FIXME)
          << "cannot convert groupname '" << _gid << "' to numeric gid";
      FATAL_ERROR_EXIT();
#endif
    }

    _numericGid = gidNumber;
  }
#endif

#ifdef ARANGODB_HAVE_SETUID
  if (_uid.empty()) {
    _numericUid = getuid();
  } else {
    int uidNumber = TRI_Int32String(_uid.c_str());

    if (TRI_errno() == TRI_ERROR_NO_ERROR) {
#ifdef ARANGODB_HAVE_GETPWUID
      passwd* p = getpwuid(uidNumber);

      if (p == nullptr) {
        LOG_TOPIC("09f8d", FATAL, arangodb::Logger::FIXME) << "unknown numeric uid '" << _uid << "'";
        FATAL_ERROR_EXIT();
      }
#endif
    } else {
#ifdef ARANGODB_HAVE_GETPWNAM
      std::string name = _uid;
      passwd* p = getpwnam(name.c_str());

      if (p != nullptr) {
        uidNumber = p->pw_uid;
      } else {
        LOG_TOPIC("d54b7", FATAL, arangodb::Logger::FIXME)
            << "cannot convert username '" << _uid << "' to numeric uid";
        FATAL_ERROR_EXIT();
      }
#else
      LOG_TOPIC("1e521", FATAL, arangodb::Logger::FIXME)
          << "cannot convert username '" << _uid << "' to numeric uid";
      FATAL_ERROR_EXIT();
#endif
    }

    _numericUid = uidNumber;
  }
#endif
}

void PrivilegeFeature::dropPrivilegesPermanently() {
#if defined(ARANGODB_HAVE_INITGROUPS) && defined(ARANGODB_HAVE_SETGID) && \
    defined(ARANGODB_HAVE_SETUID)
  // clear all supplementary groups
  if (!_gid.empty() && !_uid.empty()) {
    struct passwd* pwent = getpwuid(_numericUid);

    if (pwent != nullptr) {
      initgroups(pwent->pw_name, _numericGid);
    }
  }
#endif

#ifdef ARANGODB_HAVE_SETGID
  // first GID
  if (!_gid.empty()) {
    LOG_TOPIC("9fb03", DEBUG, arangodb::Logger::FIXME)
        << "permanently changing the gid to " << _numericGid;

    int res = setgid(_numericGid);

    if (res != 0) {
      LOG_TOPIC("4837b", FATAL, arangodb::Logger::FIXME)
          << "cannot set gid " << _numericGid << ": " << strerror(errno);
      FATAL_ERROR_EXIT();
    }
  }
#endif

#ifdef ARANGODB_HAVE_SETUID
  // then UID (because we are dropping)
  if (!_uid.empty()) {
    LOG_TOPIC("4b8b4", DEBUG, arangodb::Logger::FIXME)
        << "permanently changing the uid to " << _numericUid;

    int res = setuid(_numericUid);

    if (res != 0) {
      LOG_TOPIC("ec732", FATAL, arangodb::Logger::FIXME)
          << "cannot set uid '" << _uid << "': " << strerror(errno);
      FATAL_ERROR_EXIT();
    }
  }
#endif
}

}  // namespace arangodb
