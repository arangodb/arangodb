start:


  // otherwise openthe log file for writing
  if (!wal::LogfileManager::instance()->open()) {
    LOG(FATAL) << "Unable to finish WAL recovery procedure";
    FATAL_ERROR_EXIT();
  }

  // upgrade the database
  upgradeDatabase();
