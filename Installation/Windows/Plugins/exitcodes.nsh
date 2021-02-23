
!include "LogicLib.nsh"
!define ARANGO_EXIT_SUCCESS 0
!define ARANGO_EXIT_FAILED 1
!define ARANGO_EXIT_CODE_RESOLVING_FAILED 2
!define ARANGO_EXIT_BINARY_NOT_FOUND 5
!define ARANGO_EXIT_CONFIG_NOT_FOUND 6
!define ARANGO_EXIT_UPGRADE_FAILED 10
!define ARANGO_EXIT_UPGRADE_REQUIRED 11
!define ARANGO_EXIT_DOWNGRADE_REQUIRED 12
!define ARANGO_EXIT_VERSION_CHECK_FAILED 13
!define ARANGO_EXIT_ALREADY_RUNNING 20
!define ARANGO_EXIT_COULD_NOT_BIND_PORT 21
!define ARANGO_EXIT_COULD_NOT_LOCK 22
!define ARANGO_EXIT_RECOVERY 23
!define ARANGO_EXIT_DB_NOT_EMPTY 24
!define ARANGO_EXIT_UNSUPPORTED_STORAGE_ENGINE 25
!define ARANGO_EXIT_ICU_INITIALIZATION_FAILED 26
!define ARANGO_EXIT_TZDATA_INITIALIZATION_FAILED 27
!define ARANGO_EXIT_RESOURCES_TOO_LOW 28

!macro printExitCode exitCode Message DetailMessage
  Push "${exitCode}"
  Push "${Message}"
  Push "${DetailMessage}"
  Call printExitCode
!macroend
Function printExitCode
pop $3
pop $2
pop $1
${Switch} $1


  ${Case} 0 # EXIT_SUCCESS
    MessageBox MB_ICONEXCLAMATION '$2:$\r$\n>> success <<$\r$\n"No error has occurred."$\r$\n$3'
  ${Break}

  ${Case} 1 # EXIT_FAILED
    MessageBox MB_ICONEXCLAMATION '$2:$\r$\n>> exit with error <<$\r$\n"Will be returned when a general error occurred."$\r$\n$3'
  ${Break}

  ${Case} 2 # EXIT_CODE_RESOLVING_FAILED
    MessageBox MB_ICONEXCLAMATION '$2:$\r$\n>> exit code resolving failed <<$\r$\n"unspecified exit code"$\r$\n$3'
  ${Break}

  ${Case} 5 # EXIT_BINARY_NOT_FOUND
    MessageBox MB_ICONEXCLAMATION '$2:$\r$\n>> binary not found <<$\r$\n"Will be returned if a referenced binary was not found"$\r$\n$3'
  ${Break}

  ${Case} 6 # EXIT_CONFIG_NOT_FOUND
    MessageBox MB_ICONEXCLAMATION '$2:$\r$\n>> config not found <<$\r$\n"Will be returned if no valid configuration was found"$\r$\n$3'
  ${Break}

  ${Case} 10 # EXIT_UPGRADE_FAILED
    MessageBox MB_ICONEXCLAMATION '$2:$\r$\n>> upgrade failed <<$\r$\n"Will be returned when the database upgrade failed"$\r$\n$3'
  ${Break}

  ${Case} 11 # EXIT_UPGRADE_REQUIRED
    MessageBox MB_ICONEXCLAMATION '$2:$\r$\n>> db upgrade required <<$\r$\n"Will be returned when a database upgrade is required"$\r$\n$3'
  ${Break}

  ${Case} 12 # EXIT_DOWNGRADE_REQUIRED
    MessageBox MB_ICONEXCLAMATION '$2:$\r$\n>> db downgrade required <<$\r$\n"Will be returned when a database upgrade is required"$\r$\n$3'
  ${Break}

  ${Case} 13 # EXIT_VERSION_CHECK_FAILED
    MessageBox MB_ICONEXCLAMATION '$2:$\r$\n>> version check failed <<$\r$\n"Will be returned when there is a version mismatch"$\r$\n$3'
  ${Break}

  ${Case} 20 # EXIT_ALREADY_RUNNING
    MessageBox MB_ICONEXCLAMATION '$2:$\r$\n>> already running <<$\r$\n"Will be returned when arangod is already running according to PID-file"$\r$\n$3'
  ${Break}

  ${Case} 21 # EXIT_COULD_NOT_BIND_PORT
    MessageBox MB_ICONEXCLAMATION '$2:$\r$\n>> port blocked <<$\r$\n"Will be returned when the configured tcp endpoint is already occupied by another process"$\r$\n$3'
  ${Break}

  ${Case} 22 # EXIT_COULD_NOT_LOCK
    MessageBox MB_ICONEXCLAMATION '$2:$\r$\n>> could not lock - another process could be running <<$\r$\n"Will be returned if another ArangoDB process is running, or the state can not be cleared"$\r$\n$3'
  ${Break}

  ${Case} 23 # EXIT_RECOVERY
    MessageBox MB_ICONEXCLAMATION '$2:$\r$\n>> recovery failed <<$\r$\n"Will be returned if the automatic database startup recovery fails"$\r$\n$3'
  ${Break}

  ${Case} 24 # EXIT_DB_NOT_EMPTY
    MessageBox MB_ICONEXCLAMATION '$2:$\r$\n>> database not empty <<$\r$\n"Will be returned when commanding to initialize a non empty directory as database"$\r$\n$3'
  ${Break}

  ${Case} 25 # EXIT_UNSUPPORTED_STORAGE_ENGINE
    MessageBox MB_ICONEXCLAMATION '$2:$\r$\n>> unsupported storage engine <<$\r$\n"Will be returned when trying to start with an unsupported storage engine"$\r$\n$3'
  ${Break}

  ${Case} 26 # EXIT_ICU_INITIALIZATION_FAILED
    MessageBox MB_ICONEXCLAMATION '$2:$\r$\n>> failed to initialize ICU library <<$\r$\n"Will be returned if icudtl.dat is not found, of the wrong version or invalid. Check for an incorrectly set ICU_DATA environment variable"$\r$\n$3'
  ${Break}

  ${Case} 27 # EXIT_TZDATA_INITIALIZATION_FAILED
    MessageBox MB_ICONEXCLAMATION '$2:$\r$\n>> failed to locate tzdata <<$\r$\n"Will be returned if tzdata is not found"$\r$\n$3'
  ${Break}

  ${Case} 28 # EXIT_RESOURCES_TOO_LOW
    MessageBox MB_ICONEXCLAMATION '$2:$\r$\n>> the system restricts resources below what is required to start arangod <<$\r$\n"Will be returned if i.e. ulimit is too restrictive"$\r$\n$3'
  ${Break}

  ${Default}
    MessageBox MB_ICONEXCLAMATION '$2:$\r$\nUnknown exit code $1!'
    ; Will be returned if the recovery fails
  ${Break}

${EndSwitch}
FunctionEnd
