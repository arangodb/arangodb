////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "PrivilegeFeature.h"

//YYY #warning FRANK TODO
#if 0

SslFeature::SslFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Privilege") {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
}

void SslFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
#ifdef ARANGODB_HAVE_SETUID
  options->addHiddenOption("--uid",
                           "switch to user-id after reading config files",
                           new UInt64Parameter(&_uid));
#endif

#ifdef ARANGODB_HAVE_SETGID
  options->addHiddenOption("--gid",
                           "switch to group-id after reading config files",
                           new UInt64Parameter(&_gid));
#endif
}

void ApplicationServer::extractPrivileges() {
#ifdef ARANGODB_HAVE_SETGID
  if (_gid.empty()) {
    _numericGid = getgid();
  } else {
    int gidNumber = TRI_Int32String(_gid.c_str());

    if (TRI_errno() == TRI_ERROR_NO_ERROR && gidNumber >= 0) {
#ifdef ARANGODB_HAVE_GETGRGID
      group* g = getgrgid(gidNumber);

      if (g == 0) {
        LOG(FATAL) << "unknown numeric gid '" << _gid << "'";
        FATAL_ERROR_EXIT();
      }
#endif
    } else {
#ifdef ARANGODB_HAVE_GETGRNAM
      std::string name = _gid;
      group* g = getgrnam(name.c_str());

      if (g != 0) {
        gidNumber = g->gr_gid;
      } else {
        LOG(FATAL) << "cannot convert groupname '" << _gid
                   << "' to numeric gid";
        FATAL_ERROR_EXIT();
      }
#else
      LOG(FATAL) << "cannot convert groupname '" << _gid << "' to numeric gid";
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

      if (p == 0) {
        LOG(FATAL) << "unknown numeric uid '" << _uid << "'";
        FATAL_ERROR_EXIT();
      }
#endif
    } else {
#ifdef ARANGODB_HAVE_GETPWNAM
      std::string name = _uid;
      passwd* p = getpwnam(name.c_str());

      if (p != 0) {
        uidNumber = p->pw_uid;
      } else {
        LOG(FATAL) << "cannot convert username '" << _uid << "' to numeric uid";
        FATAL_ERROR_EXIT();
      }
#else
      LOG(FATAL) << "cannot convert username '" << _uid << "' to numeric uid";
      FATAL_ERROR_EXIT();
#endif
    }

    _numericUid = uidNumber;
  }
#endif
}

void ApplicationServer::dropPrivilegesPermanently() {
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
    LOG(DEBUG) << "permanently changing the gid to " << _numericGid;

    int res = setgid(_numericGid);

    if (res != 0) {
      LOG(FATAL) << "cannot set gid " << _numericGid << ": " << strerror(errno);
      FATAL_ERROR_EXIT();
    }
  }
#endif

#ifdef ARANGODB_HAVE_SETUID
  // then UID (because we are dropping)
  if (!_uid.empty()) {
    LOG(DEBUG) << "permanently changing the uid to " << _numericUid;

    int res = setuid(_numericUid);

    if (res != 0) {
      LOG(FATAL) << "cannot set uid '" << _uid << "': " << strerror(errno);
      FATAL_ERROR_EXIT();
    }
  }
#endif
}

#endif
