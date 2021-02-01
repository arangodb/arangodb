/*jshint maxlen: 240 */
/*global require */

/// auto-generated file generated from exitcodes.dat

(function () {
  "use strict";
  var internal = require("internal");

  internal.exitCodes = {
    "EXIT_SUCCESS"                 : { "code" : 0, "message" : "success" },
    "EXIT_FAILED"                  : { "code" : 1, "message" : "exit with error" },
    "EXIT_CODE_RESOLVING_FAILED"   : { "code" : 2, "message" : "exit code resolving failed" },
    "EXIT_BINARY_NOT_FOUND"        : { "code" : 5, "message" : "binary not found" },
    "EXIT_CONFIG_NOT_FOUND"        : { "code" : 6, "message" : "config not found" },
    "EXIT_UPGRADE_FAILED"          : { "code" : 10, "message" : "upgrade failed" },
    "EXIT_UPGRADE_REQUIRED"        : { "code" : 11, "message" : "db upgrade required" },
    "EXIT_DOWNGRADE_REQUIRED"      : { "code" : 12, "message" : "db downgrade required" },
    "EXIT_VERSION_CHECK_FAILED"    : { "code" : 13, "message" : "version check failed" },
    "EXIT_ALREADY_RUNNING"         : { "code" : 20, "message" : "already running" },
    "EXIT_COULD_NOT_BIND_PORT"     : { "code" : 21, "message" : "port blocked" },
    "EXIT_COULD_NOT_LOCK"          : { "code" : 22, "message" : "could not lock - another process could be running" },
    "EXIT_RECOVERY"                : { "code" : 23, "message" : "recovery failed" },
    "EXIT_DB_NOT_EMPTY"            : { "code" : 24, "message" : "database not empty" },
    "EXIT_UNSUPPORTED_STORAGE_ENGINE" : { "code" : 25, "message" : "unsupported storage engine" },
    "EXIT_ICU_INITIALIZATION_FAILED" : { "code" : 26, "message" : "failed to initialize ICU library" },
    "EXIT_TZDATA_INITIALIZATION_FAILED" : { "code" : 27, "message" : "failed to locate tzdata" }
  };
}());

