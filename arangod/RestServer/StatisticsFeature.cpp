  bool disableStatistics = false;

  additional["Server Options:help-admin"]("server.disable-statistics",
                                          &disableStatistics,
                                          "turn off statistics gathering");

  if (disableStatistics) {
    TRI_ENABLE_STATISTICS = false;
  }

  TRI_InitializeStatistics();

  TRI_ShutdownStatistics();
