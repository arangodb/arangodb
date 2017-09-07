
!include "LogicLib.nsh"
!macro printExitCode exitCode Message
  Push "${exitCode}"
  Push "${Message}"
  Call printExitCode
!macroend
Function printExitCode
pop $1
pop $2
${Switch} $0


  ${Case} 0 # EXIT_SUCCESS
    MessageBox MB_ICONEXCLAMATION '$1:$\r$\nsuccess'
    ; No error has occurred.
  ${Break}

  ${Case} 1 # EXIT_FAILED
    MessageBox MB_ICONEXCLAMATION '$1:$\r$\nexit with error'
    ; Will be returned when a general error occurred.
  ${Break}

  ${Case} 2 # EXIT_CODE_RESOLVING_FAILED
    MessageBox MB_ICONEXCLAMATION '$1:$\r$\nexit code resolving failed'
    ; fill me
  ${Break}

  ${Case} 5 # EXIT_BINARY_NOT_FOUND
    MessageBox MB_ICONEXCLAMATION '$1:$\r$\nbinary not found'
    ; fill me
  ${Break}

  ${Case} 6 # EXIT_CONFIG_NOT_FOUND
    MessageBox MB_ICONEXCLAMATION '$1:$\r$\nconfig not found'
    ; fill me
  ${Break}

  ${Case} 10 # EXIT_UPGRADE_FAILED
    MessageBox MB_ICONEXCLAMATION '$1:$\r$\nupgrade failed'
    ; Will be returned when the database upgrade failed
  ${Break}

  ${Case} 11 # EXIT_UPGRADE_REQUIRED
    MessageBox MB_ICONEXCLAMATION '$1:$\r$\ndb upgrade required'
    ; Will be returned when a database upgrade is required
  ${Break}

  ${Case} 12 # EXIT_DOWNGRADE_REQUIRED
    MessageBox MB_ICONEXCLAMATION '$1:$\r$\ndb downgrade required'
    ; Will be returned when a database upgrade is required
  ${Break}

  ${Case} 13 # EXIT_VERSION_CHECK_FAILED
    MessageBox MB_ICONEXCLAMATION '$1:$\r$\nversion check failed'
    ; Will be returned when there is a version mismatch
  ${Break}

  ${Case} 20 # EXIT_ALREADY_RUNNING
    MessageBox MB_ICONEXCLAMATION '$1:$\r$\nalready running'
    ; Will be returned when arangod is already running according to PID-file
  ${Break}

  ${Case} 21 # EXIT_COULD_NOT_BIND_PORT
    MessageBox MB_ICONEXCLAMATION '$1:$\r$\nport blocked'
    ; Will be returned when endpoint is taken by another process
  ${Break}

  ${Case} 22 # EXIT_COULD_NOT_LOCK
    MessageBox MB_ICONEXCLAMATION '$1:$\r$\ncould not lock - another process could be running'
    ; fill me
  ${Break}

${EndSwitch}
FunctionEnd
