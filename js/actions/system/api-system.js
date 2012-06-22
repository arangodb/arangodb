////////////////////////////////////////////////////////////////////////////////
/// @brief administration actions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var actions = require("actions");
var internal = require("internal");
var console = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                            administration actions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ActionsAdmin
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns system time
////////////////////////////////////////////////////////////////////////////////

function GET_time (req, res) {
  actions.resultOk(req, res, actions.HTTP_OK, { time : internal.time() });
}

actions.defineHttp({
  url : "_api/time",
  context : "api",
  prefix : false,
  callback : GET_time
});

actions.defineHttp({
  url : "_admin/time",
  context : "admin",
  prefix : false,
  callback : GET_time
});

////////////////////////////////////////////////////////////////////////////////
/// @brief returns V8 version
////////////////////////////////////////////////////////////////////////////////

function GET_v8_version (req, res) {
  actions.resultOk(req, res, actions.HTTP_OK, { version : "V8" });
}

actions.defineHttp({
  url : "_admin/v8-version",
  context : "admin",
  prefix : false,
  callback : GET_v8_version
});

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the request
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/echo",
  context : "admin",
  prefix : true,
  callback : function (req, res) {
    res.responseCode = actions.HTTP_OK;
    res.contentType = "application/json";
    res.body = JSON.stringify(req);
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief returns system status information for the server
////////////////////////////////////////////////////////////////////////////////

function AdminRedirect (req, res) {
  var dest = "/_admin/html/index.html";

  res.responseCode = actions.HTTP_MOVED_PERMANENTLY;
  res.contentType = "text/html";

  res.body = "<html><head><title>Moved</title></head><body><h1>Moved</h1><p>This page has moved to <a href=\""
    + dest
    + "\">"
    + dest
    + "</a>.</p></body></html>";

  res.headers = { location : dest };
}

actions.defineHttp({
  url : "",
  context : "admin",
  prefix : false,
  callback : AdminRedirect
});

actions.defineHttp({
  url : "_admin",
  context : "admin",
  prefix : false,
  callback : AdminRedirect
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_GET_admin_status
/// @brief returns system status information for the server
///
/// @RESTHEADER{GET /_admin/status,reads the system status}
///
/// @REST{GET /_admin/status}
///
/// The call returns an object with the following attributes:
///
/// - @LIT{system.userTime}: Amount of time that this process has been
///   scheduled in user mode, measured in clock ticks divided by
///   sysconf(_SC_CLK_TCK) aka seconds.
///
/// - @LIT{system.systemTime}: mount of time that this process has
///   been scheduled in kernel mode, measured in clock ticks divided by
///   sysconf(_SC_CLK_TCK) aka seconds.
///
/// - @LIT{system.numberOfThreads}: Number of threads in this process.
///
/// - @LIT{system.residentSize}: Resident Set Size: number of pages
///   the process has in real memory.  This is just the pages which
///   count toward text, data, or stack space.  This does not include
///   pages which have not been demand-loaded in, or which are swapped
///   out.
///
/// - @LIT{system.virtualSize}: Virtual memory size in bytes.
///
/// - @LIT{system.minorPageFaults}: The number of minor faults the process has
///   made which have not required loading a memory page from disk.
///
/// - @LIT{system.majorPageFaults}: The number of major faults the process has
///   made which have required loading a memory page from disk.
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/status",
  context : "admin",

  callback : function (req, res) {
    try {
      result = {};
      result.system = SYS_PROCESS_STAT();

      actions.resultOk(req, res, actions.HTTP_OK, result);
    }
    catch (err) {
      actions.resultError(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_GET_admin_config_description
/// @brief returns configuration description
///
/// @RESTHEADER{GET /_admin/config/description,reads the configuration desciption}
///
/// @REST{GET /_admin/config/desciption}
///
/// The call returns an object describing the configuration.
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/config/description",
  context : "admin",

  callback : function (req, res) {
    try {
      result = {
        database : {
          name : "Database",
          type : "section",

          path : {
            name : "Path",
            type : "string",
            readonly : true
          },

          access : {
            name : "Combined Access",
            type : "string",
            readonly : true
          }
        },

        logging : {
          name : "Logging",
          type : "section",

          level : {
            name : "Log Level",
            type : "pull-down",
            values : [ "fatal", "error", "warning", "info", "debug", "trace" ]
          },

          syslog : {
            name : "Use Syslog",
            type : "boolean"
          },

          bufferSize : {
            name : "Log Buffer Size",
            type : "integer"
          },

          output : {
            name : "Output",
            type : "section",

            file : {
              name : "Log File",
              type : "string",
              readonly : true
            }
          }
        }
      };

      actions.resultOk(req, res, actions.HTTP_OK, result);
    }
    catch (err) {
      actions.resultError(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_GET_admin_config_configuration
/// @brief returns configuration description
///
/// @RESTHEADER{GET /_admin/config/configuration,reads the configuration}
///
/// @REST{GET /_admin/config/configuration}
///
/// The call returns an object containing configuration.
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/config/configuration",
  context : "admin",

  callback : function (req, res) {
    try {
      result = {
        database : {
          path : {
            value : "/tmp/emil/vocbase"
          },

          access : {
            value : "localhost:8529"
          }
        },

        logging : {
          level : {
            value : "info"
          },

          syslog : {
            value : true
          },

          bufferSize : {
            value : 100
          },

          output : {
            file : {
              value : "/var/log/message/arango.log"
            }
          }
        }
      };

      actions.resultOk(req, res, actions.HTTP_OK, result);
    }
    catch (err) {
      actions.resultError(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
