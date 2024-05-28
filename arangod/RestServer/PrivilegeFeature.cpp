////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Basics/FileUtils.h"

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
#include "Basics/NumberUtils.h"
#include "Basics/application-exit.h"
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

PrivilegeFeature::PrivilegeFeature(Server& server)
    : ArangodFeature{server, *this} {
  setOptional(true);
  startsAfter<application_features::GreetingsFeaturePhase>();
}

void PrivilegeFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
#ifdef ARANGODB_HAVE_SETUID
  options
      ->addOption(
          "--uid",
          "Switch to this user ID after reading the configuration files.",
          new StringParameter(&_uid),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(The name (identity) of the user to run the
server as.

If you don't specify this option, the server does not attempt to change its UID,
so that the UID used by the server is the same as the UID of the user who
started the server.

If you specify this option, the server changes its UID after opening ports and
reading configuration files, but before accepting connections or opening other
files (such as recovery files). This is useful if the server must be started
with raised privileges (in certain environments) but security considerations
require that these privileges are dropped once the server has started work.

**Note**: You cannot use this option to bypass operating system security.
In general, this option (and the related `--gid`) can lower privileges but not
raise them.)");

  options->addOption(
      "--server.uid",
      "Switch to this user ID after reading configuration files.",
      new StringParameter(&_uid),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));
#endif

#ifdef ARANGODB_HAVE_SETGID
  options
      ->addOption("--gid",
                  "Switch to this group ID after reading configuration files.",
                  new StringParameter(&_gid),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(The name (identity) of the group to run the
server as.

If you don't specify this option, the server does not attempt to change its GID,
so that the GID the server runs as is the primary group of the user who started
the server.

If you specify this option, the server changes its GID after opening ports and
reading configuration files, but before accepting connections or opening other
files (such as recovery files).)");

  options->addOption(
      "--server.gid",
      "Switch to this group ID after reading configuration files.",
      new StringParameter(&_gid),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));
#endif
}

void PrivilegeFeature::prepare() { extractPrivileges(); }

void PrivilegeFeature::extractPrivileges() {
#ifdef ARANGODB_HAVE_SETGID
  if (_gid.empty()) {
    _numericGid = getgid();
  } else {
    bool valid = false;
    int gidNumber = NumberUtils::atoi_positive<int>(
        _gid.data(), _gid.data() + _gid.size(), valid);

    if (valid && gidNumber >= 0) {
#ifdef ARANGODB_HAVE_GETGRGID
      std::optional<gid_t> gid = FileUtils::findGroup(_gid);
      if (!gid) {
        LOG_TOPIC("3d53b", FATAL, arangodb::Logger::FIXME)
            << "unknown numeric gid '" << _gid << "'";
        FATAL_ERROR_EXIT();
      }
#endif
    } else {
#ifdef ARANGODB_HAVE_GETGRNAM
      std::optional<gid_t> gid = FileUtils::findGroup(_gid);
      if (gid) {
        gidNumber = gid.value();
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
    bool valid = false;
    int uidNumber = NumberUtils::atoi_positive<int>(
        _uid.data(), _uid.data() + _uid.size(), valid);

    if (valid) {
#ifdef ARANGODB_HAVE_GETPWUID
      std::optional<uid_t> uid = FileUtils::findUser(_uid);
      if (!uid) {
        LOG_TOPIC("09f8d", FATAL, arangodb::Logger::FIXME)
            << "unknown numeric uid '" << _uid << "'";
        FATAL_ERROR_EXIT();
      }
#endif
    } else {
#ifdef ARANGODB_HAVE_GETPWNAM
      std::optional<uid_t> uid = FileUtils::findUser(_uid);
      if (uid) {
        uidNumber = uid.value();
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
    std::optional<std::string> name = FileUtils::findUserName(_numericUid);

    if (name) {
      FileUtils::initGroups(name.value(), _numericGid);
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
