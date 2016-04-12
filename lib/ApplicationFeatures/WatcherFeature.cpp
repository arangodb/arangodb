  options["General Options:help-admin"]
#ifdef ARANGODB_HAVE_GETPPID
      ("exit-on-parent-death", &_exitOnParentDeath, "exit if parent dies")
#endif
          ("watch-process", &_watchParent,
           "exit if process with given PID dies");

#endif



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

