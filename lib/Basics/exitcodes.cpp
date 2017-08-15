////////////////////////////////////////////////////////////////////////////////
/// @brief auto-generated file generated from exitcodes.dat
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "./lib/Basics/exitcodes.h"

void TRI_InitializeExitMessages () {
  REG_EXIT(EXIT_SUCCESS, "success");
  REG_EXIT(EXIT_FAILED, "failed");
  REG_EXIT(EXIT_UPGRADE, "upgrade failed");
  REG_EXIT(EXIT_ALREADY_RUNNING, "already running");
  REG_EXIT(EXIT_PORT_BLOCKED, "port blocked");
}
