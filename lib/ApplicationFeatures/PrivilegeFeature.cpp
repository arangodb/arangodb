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
      LOG(FATAL) << "cannot convert groupname '" << _gid
                 << "' to numeric gid";
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
        LOG(FATAL) << "cannot convert username '" << _uid
                   << "' to numeric uid";
        FATAL_ERROR_EXIT();
      }
#else
      LOG(FATAL) << "cannot convert username '" << _uid
                 << "' to numeric uid";
      FATAL_ERROR_EXIT();
#endif
    }

    _numericUid = uidNumber;
  }
#endif
}

void ApplicationServer::dropPrivilegesPermanently() {
// clear all supplementary groups
#if defined(ARANGODB_HAVE_INITGROUPS) && defined(ARANGODB_HAVE_SETGID) && \
    defined(ARANGODB_HAVE_SETUID)

  if (!_gid.empty() && !_uid.empty()) {
    struct passwd* pwent = getpwuid(_numericUid);

    if (pwent != nullptr) {
      initgroups(pwent->pw_name, _numericGid);
    }
  }

#endif

// first GID
#ifdef ARANGODB_HAVE_SETGID

  if (!_gid.empty()) {
    LOG(DEBUG) << "permanently changing the gid to " << _numericGid;

    int res = setgid(_numericGid);

    if (res != 0) {
      LOG(FATAL) << "cannot set gid " << _numericGid << ": " << strerror(errno);
      FATAL_ERROR_EXIT();
    }
  }

#endif

// then UID (because we are dropping)
#ifdef ARANGODB_HAVE_SETUID

  if (!_uid.empty()) {
    LOG(DEBUG) << "permanently changing the uid to " << _numericUid;

    int res = setuid(_numericUid);

    if (res != 0) {
      LOG(FATAL) << "cannot set uid '" << _uid
                 << "': " << strerror(errno);
      FATAL_ERROR_EXIT();
    }
  }

#endif
}

