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

// -----------------------------------------------------------------------------
// --SECTION--                                            administration actions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ActionsAdmin
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_GET_system_status
/// @brief returns system status information for the server
///
/// @REST{GET /_system/status}
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
  url : "_system/status",
  context : "admin",

  callback : function (req, res) {
    try {
      result = {};
      result.system = SYS_PROCESS_STAT();

      actions.resultOk(req, res, 200, result);
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
