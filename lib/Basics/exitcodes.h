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
/// unspecified exit code
constexpr int TRI_EXIT_CODE_RESOLVING_FAILED                                    = 2;

/// 5: EXIT_BINARY_NOT_FOUND
/// binary not found
/// Will be returned if a referenced binary was not found
constexpr int TRI_EXIT_BINARY_NOT_FOUND                                         = 5;

/// 6: EXIT_CONFIG_NOT_FOUND
/// config not found
/// Will be returned if no valid configuration was found
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
/// Will be returned when the configured tcp endpoint is already occupied by
/// another process
constexpr int TRI_EXIT_COULD_NOT_BIND_PORT                                      = 21;

/// 22: EXIT_COULD_NOT_LOCK
/// could not lock - another process could be running
/// Will be returned if another ArangoDB process is running, or the state can
/// not be cleared
constexpr int TRI_EXIT_COULD_NOT_LOCK                                           = 22;

/// 23: EXIT_RECOVERY
/// recovery failed
/// Will be returned if the automatic database startup recovery fails
constexpr int TRI_EXIT_RECOVERY                                                 = 23;

/// 24: EXIT_DB_NOT_EMPTY
/// database not empty
/// Will be returned when commanding to initialize a non empty directory as
/// database
constexpr int TRI_EXIT_DB_NOT_EMPTY                                             = 24;

/// 25: EXIT_UNSUPPORTED_STORAGE_ENGINE
/// unsupported storage engine
/// Will be returned when trying to start with an unsupported storage engine
constexpr int TRI_EXIT_UNSUPPORTED_STORAGE_ENGINE                               = 25;

/// 26: EXIT_ICU_INITIALIZATION_FAILED
/// failed to initialize ICU library
/// Will be returned if icudtl.dat is not found, of the wrong version or
/// invalid. Check for an incorrectly set ICU_DATA environment variable
constexpr int TRI_EXIT_ICU_INITIALIZATION_FAILED                                = 26;


/// register all exit codes for ArangoDB
void TRI_InitializeExitMessages();

#endif

