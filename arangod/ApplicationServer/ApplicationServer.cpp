namespace arangodb {
extern ConfigFeature CONFIG_FEATURE;
extern LoggerFeature LOGGER_FEATURE;
extern SslFeature SSL_FEATURE;

#if 0
  options["Hidden Options"]
#ifdef ARANGODB_HAVE_SETUID
    ("uid", &_uid, "switch to user-id after reading config files")
#endif
#ifdef ARANGODB_HAVE_SETGID
    ("gid", &_gid, "switch to group-id after reading config files")
#endif
  ;

#if defined(ARANGODB_HAVE_SETUID) || defined(ARANGODB_HAVE_SETGID)

  options["General Options:help-admin"]
#ifdef ARANGODB_HAVE_GETPPID
      ("exit-on-parent-death", &_exitOnParentDeath, "exit if parent dies")
#endif
          ("watch-process", &_watchParent,
           "exit if process with given PID dies");

#endif

#endif

#warning TODO
#if 0
  // .............................................................................
  // UID and GID
  // .............................................................................

  extractPrivileges();
  dropPrivilegesPermanently();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the parent is still alive
////////////////////////////////////////////////////////////////////////////////

bool ApplicationServer::checkParent() {
// check our parent, if it died given up
#ifdef ARANGODB_HAVE_GETPPID
  if (_exitOnParentDeath && getppid() == 1) {
    LOG(INFO) << "parent has died";
    return false;
  }
#endif

// unfortunately even though windows has <signal.h>, there is no
// kill method defined. Notice that the kill below is not to terminate
// the process.
#ifdef TRI_HAVE_SIGNAL_H
  if (_watchParent != 0) {
#ifdef TRI_HAVE_POSIX
    int res = kill(_watchParent, 0);
#else
    int res = -1;
#endif
    if (res != 0) {
      LOG(INFO) << "parent " << _watchParent << " has died";
      return false;
    }
  }
#endif

  return true;
}

#endif
