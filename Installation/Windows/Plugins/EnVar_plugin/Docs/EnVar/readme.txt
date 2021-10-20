EnVar plugin for NSIS

Coded by Jason Ross aka JasonFriday13 on the forums.

Copyright (C) 2014-2016, 2018, 2020-2021 MouseHelmet Software


Introduction
------------

This plugin manages environment variables. It allows checking for environment
variables, checking for paths in those variables, adding paths to variables,
removing paths from variables, removing variables from the environment, and
updating the installer enviroment from the registry. This plugin is provided
with 32bit ansi and unicode versions, as well as a 64bit unicode version.

Functions
---------

There are eight functions in this plugin. Returns an error code on the 
stack (unless noted otherwise), see below for the codes.

ERR_SUCCESS     0   Function completed successfully.
ERR_NOMEMALLOC  1   Function can't allocate memory required.
ERR_NOREAD      2   Function couldn't read from the environment.
ERR_NOVARIABLE  3   Variable doesn't exist in the current environment.
ERR_NOTYPE      4   Variable exists but is the wrong type.
ERR_NOVALUE     5   Value doesn't exist in the Variable.
ERR_NOWRITE     6   Function can't write to the current environment.

EnVar::SetHKCU
EnVar::SetHKLM

  SetHKCU sets the environment access to the current user. This is the default.
  SetHKLM sets the environment access to the local machine.
  These functions do not return a value on the stack.

EnVar::Check         VariableName Path

  Checks for a Path in the specified VariableName. Passing "null" as a Path makes
  it check for the existance of VariableName. Passing "null" for both makes it
  check for write access to the current environment.

EnVar::AddValue      VariableName Path
EnVar::AddValueEx    VariableName Path

  Adds a Path to the specified VariableName. AddValueEx is for expandable paths,
  ie %tempdir%. Both functions respect expandable variables, so if the variable
  already exists, they try to leave it intact. AddValueEx converts the variable to
  its expandable version. If the path already exists, it returns a success error code.

EnVar::DeleteValue   VariableName Path

  Deletes a path from a variable. The delete is also recursive, so if it finds multiple
  paths, all of them are deleted as well.

EnVar::Delete        VariableName

  Deletes a variable from the environment. Note that "path" cannot be deleted.

EnVar::Update        RegRoot VariableName

  Updates the installer environment by reading VariableName from the registry
  and can use RegRoot to specify from which root it reads from: HKCU for the
  current user, and HKLM for the local machine. Passing "null" for RegRoot
  causes VariableName to be removed from the installer environment. Anything
  else (including an empty string) for RegRoot means it reads from both roots,
  appends the paths together, and updates the installer environment. This
  function is not affected by EnVar::SetHKCU and EnVar::SetHKLM, and does not
  write to the registry.
