////////////////////////////////////////////////////////////////////////////////
/// @brief auto-generated file generated from exitcodes.dat
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "./lib/Basics/exitcodes.h"

void TRI_InitializeExitMessages () {
  REG_EXIT(EXIT_SUCCESS, "success");
  REG_EXIT(EXIT_FAILED, "failed");
  REG_EXIT(EXIT_UPGRADE_FAILED, "upgrade failed");
  REG_EXIT(EXIT_UPGRADE_REQUIRED, "db upgrade required");
  REG_EXIT(EXIT_DOWNGRADE_REQUIRED, "db downgrade required");
  REG_EXIT(EXIT_VERSION_CHECK_FAILED, "version check failed");
  REG_EXIT(EXIT_ALREADY_RUNNING, "already running");
  REG_EXIT(EXIT_PORT_BLOCKED, "port blocked");
}
