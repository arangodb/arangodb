#ifndef ARANGODB_BASICS_EXIT_CODES_H
#define ARANGODB_BASICS_EXIT_CODES_H 1

#include "Basics/error.h"

/// Exit codes and meanings
/// The following codes might be returned when exiting ArangoDB:

/// 0: EXIT_SUCCESS
/// success
/// No error has occurred.
constexpr int TRI_EXIT_SUCCESS                                                  = 0;

/// 1: EXIT_FAILED
/// exit with error
/// Will be returned when a general error occurred.
constexpr int TRI_EXIT_FAILED                                                   = 1;

/// 2: EXIT_CODE_RESOLVING_FAILED
/// exit code resolving failed
/// fill me
constexpr int TRI_EXIT_CODE_RESOLVING_FAILED                                    = 2;

/// 5: EXIT_BINARY_NOT_FOUND
/// binary not found
/// fill me
constexpr int TRI_EXIT_BINARY_NOT_FOUND                                         = 5;

/// 6: EXIT_CONFIG_NOT_FOUND
/// config not found
/// fill me
constexpr int TRI_EXIT_CONFIG_NOT_FOUND                                         = 6;

/// 10: EXIT_UPGRADE_FAILED
/// upgrade failed
/// Will be returned when the database upgrade failed
constexpr int TRI_EXIT_UPGRADE_FAILED                                           = 10;

/// 11: EXIT_UPGRADE_REQUIRED
/// db upgrade required
/// Will be returned when a database upgrade is required
constexpr int TRI_EXIT_UPGRADE_REQUIRED                                         = 11;

/// 12: EXIT_DOWNGRADE_REQUIRED
/// db downgrade required
/// Will be returned when a database upgrade is required
constexpr int TRI_EXIT_DOWNGRADE_REQUIRED                                       = 12;

/// 13: EXIT_VERSION_CHECK_FAILED
/// version check failed
/// Will be returned when there is a version mismatch
constexpr int TRI_EXIT_VERSION_CHECK_FAILED                                     = 13;

/// 20: EXIT_ALREADY_RUNNING
/// already running
/// Will be returned when arangod is already running according to PID-file
constexpr int TRI_EXIT_ALREADY_RUNNING                                          = 20;

/// 21: EXIT_COULD_NOT_BIND_PORT
/// port blocked
/// Will be returned when endpoint is taken by another process
constexpr int TRI_EXIT_COULD_NOT_BIND_PORT                                      = 21;

/// 22: EXIT_COULD_NOT_LOCK
/// could not lock - another process could be running
/// fill me
constexpr int TRI_EXIT_COULD_NOT_LOCK                                           = 22;

/// 23: EXIT_RECOVERY
/// recovery failed
/// Will be returned if the recovery fails
constexpr int TRI_EXIT_RECOVERY                                                 = 23;


/// register all exit codes for ArangoDB
void TRI_InitializeExitMessages();

#endif

