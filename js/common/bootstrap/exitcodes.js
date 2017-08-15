/*jshint maxlen: 240 */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief auto-generated file generated from exitcodes.dat
////////////////////////////////////////////////////////////////////////////////

(function () {
  "use strict";
  var internal = require("internal");

  internal.exitCodes = {
    "EXIT_SUCCESS"                 : { "code" : 0, "message" : "success" },
    "EXIT_FAILED"                  : { "code" : 1, "message" : "failed" },
    "EXIT_UPGRADE_FAILED"          : { "code" : 10, "message" : "upgrade failed" },
    "EXIT_UPGRADE_REQUIRED"        : { "code" : 11, "message" : "db upgrade required" },
    "EXIT_DOWNGRADE_REQUIRED"      : { "code" : 12, "message" : "db downgrade required" },
    "EXIT_VERSION_CHECK_FAILED"    : { "code" : 13, "message" : "version check failed" },
    "EXIT_ALREADY_RUNNING"         : { "code" :  20, "message" : "already running" },
    "EXIT_PORT_BLOCKED"            : { "code" :  21, "message" : "port blocked" }
  };
}());

