////////////////////////////////////////////////////////////////////////////////
/// @brief auto-generated file generated from exitcodes.dat
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "./lib/Basics/exitcodes.h"

void TRI_InitializeExitMessages () {
  REG_EXIT(EXIT_SUCCESS, "success");
  REG_EXIT(EXIT_FAILED, "exit with error");
  REG_EXIT(EXIT_CODE_RESOLVING_FAILED, "exit code resolving failed");
  REG_EXIT(EXIT_BINARY_NOT_FOUND, "binary not found");
  REG_EXIT(EXIT_CONFIG_NOT_FOUND, "config not found");
  REG_EXIT(EXIT_UPGRADE_FAILED, "upgrade failed");
  REG_EXIT(EXIT_UPGRADE_REQUIRED, "db upgrade required");
  REG_EXIT(EXIT_DOWNGRADE_REQUIRED, "db downgrade required");
  REG_EXIT(EXIT_VERSION_CHECK_FAILED, "version check failed");
  REG_EXIT(EXIT_ALREADY_RUNNING, "already running");
  REG_EXIT(EXIT_COULD_NOT_BIND_PORT, "port blocked");
  REG_EXIT(EXIT_COULD_NOT_LOCK, "could not lock - another process could be running");
}
