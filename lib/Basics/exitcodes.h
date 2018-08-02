
#ifndef TRIAGENS_BASICS_EXIT_CODES_H
#define TRIAGENS_BASICS_EXIT_CODES_H 1

////////////////////////////////////////////////////////////////////////////////
/// Exit codes and meanings
///
/// The following codes might be retured when exiting ArangoDB:
///
#include "Basics/error.h"
/// - 0: @LIT{success}
///   No error has occurred.
/// - 1: @LIT{exit with error}
///   Will be returned when a general error occurred.
/// - 2: @LIT{exit code resolving failed}
///   fill me
/// - 5: @LIT{binary not found}
///   fill me
/// - 6: @LIT{config not found}
///   fill me
/// - 10: @LIT{upgrade failed}
///   Will be returned when the database upgrade failed
/// - 11: @LIT{db upgrade required}
///   Will be returned when a database upgrade is required
/// - 12: @LIT{db downgrade required}
///   Will be returned when a database upgrade is required
/// - 13: @LIT{version check failed}
///   Will be returned when there is a version mismatch
/// - 20: @LIT{already running}
///   Will be returned when arangod is already running according to PID-file
/// - 21: @LIT{port blocked}
///   Will be returned when endpoint is taken by another process
/// - 22: @LIT{could not lock - another process could be running}
///   fill me
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief helper macro to define an exit code string
////////////////////////////////////////////////////////////////////////////////

#define REG_EXIT(id, label) TRI_set_exitno_string(TRI_ ## id, label);

////////////////////////////////////////////////////////////////////////////////
/// @brief register all exit codes for ArangoDB
////////////////////////////////////////////////////////////////////////////////

void TRI_InitializeExitMessages ();

////////////////////////////////////////////////////////////////////////////////
/// @brief 0: EXIT_SUCCESS
///
/// success
///
/// No error has occurred.
////////////////////////////////////////////////////////////////////////////////

#define TRI_EXIT_SUCCESS                                                  (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1: EXIT_FAILED
///
/// exit with error
///
/// Will be returned when a general error occurred.
////////////////////////////////////////////////////////////////////////////////

#define TRI_EXIT_FAILED                                                   (1)

////////////////////////////////////////////////////////////////////////////////
/// @brief 2: EXIT_CODE_RESOLVING_FAILED
///
/// exit code resolving failed
/// unspecified exit code
constexpr int TRI_EXIT_CODE_RESOLVING_FAILED                                    = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief 5: EXIT_BINARY_NOT_FOUND
///
/// binary not found
/// Will be returned if a referenced binary was not found
constexpr int TRI_EXIT_BINARY_NOT_FOUND                                         = 5;

#define TRI_EXIT_BINARY_NOT_FOUND                                         (5)

////////////////////////////////////////////////////////////////////////////////
/// @brief 6: EXIT_CONFIG_NOT_FOUND
///
/// config not found
/// Will be returned if no valid configuration was found
constexpr int TRI_EXIT_CONFIG_NOT_FOUND                                         = 6;

////////////////////////////////////////////////////////////////////////////////
/// @brief 10: EXIT_UPGRADE_FAILED
///
/// upgrade failed
///
/// Will be returned when the database upgrade failed
////////////////////////////////////////////////////////////////////////////////

#define TRI_EXIT_UPGRADE_FAILED                                           (10)

////////////////////////////////////////////////////////////////////////////////
/// @brief 11: EXIT_UPGRADE_REQUIRED
///
/// db upgrade required
///
/// Will be returned when a database upgrade is required
////////////////////////////////////////////////////////////////////////////////

#define TRI_EXIT_UPGRADE_REQUIRED                                         (11)

////////////////////////////////////////////////////////////////////////////////
/// @brief 12: EXIT_DOWNGRADE_REQUIRED
///
/// db downgrade required
///
/// Will be returned when a database upgrade is required
////////////////////////////////////////////////////////////////////////////////

#define TRI_EXIT_DOWNGRADE_REQUIRED                                       (12)

////////////////////////////////////////////////////////////////////////////////
/// @brief 13: EXIT_VERSION_CHECK_FAILED
///
/// version check failed
///
/// Will be returned when there is a version mismatch
////////////////////////////////////////////////////////////////////////////////

#define TRI_EXIT_VERSION_CHECK_FAILED                                     (13)

////////////////////////////////////////////////////////////////////////////////
/// @brief 20: EXIT_ALREADY_RUNNING
///
/// already running
///
/// Will be returned when arangod is already running according to PID-file
////////////////////////////////////////////////////////////////////////////////

#define TRI_EXIT_ALREADY_RUNNING                                          (20)

////////////////////////////////////////////////////////////////////////////////
/// @brief 21: EXIT_COULD_NOT_BIND_PORT
///
/// port blocked
/// Will be returned when the configured tcp endpoint is already occupied by
/// another process
constexpr int TRI_EXIT_COULD_NOT_BIND_PORT                                      = 21;

////////////////////////////////////////////////////////////////////////////////
/// @brief 22: EXIT_COULD_NOT_LOCK
///
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

#define TRI_EXIT_COULD_NOT_LOCK                                           (22)

#endif

