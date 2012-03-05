////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript server functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2011-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page JSModuleAvocadoTOC
///
/// <ol>
///   <li>@ref JSModuleAvocadoDefineHttpSystemAction "avocado.defineHttpSystemAction"</li>
/// </ol>
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page JSModuleAvocado Module "avocado"
///
/// The following functions are used avocadoly.
///
/// <hr>
/// @copydoc JSModuleAvocadoTOC
/// <hr>
///
/// @anchor JSModuleAvocadoDefineHttpSystemAction
/// @copydetails JS_DefineSystemAction
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 Module "internal"
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8ModuleInternal
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief internal module
////////////////////////////////////////////////////////////////////////////////

ModuleCache["/internal"].exports.db = db;
ModuleCache["/internal"].exports.edges = edges;
ModuleCache["/internal"].exports.AvocadoCollection = AvocadoCollection;
ModuleCache["/internal"].exports.AvocadoEdgesCollection = AvocadoEdgesCollection;

if (typeof SYS_DEFINE_ACTION === "undefined") {
  ModuleCache["/internal"].exports.defineAction = function() {
    console.error("SYS_DEFINE_ACTION not available");
  }
}
else {
  ModuleCache["/internal"].exports.defineAction = SYS_DEFINE_ACTION;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  Module "avocado"
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8ModuleAvocado
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief avocado module
////////////////////////////////////////////////////////////////////////////////

ModuleCache["/avocado"] = new Module("/avocado");

if (typeof defineSystemAction == "function") {
  ModuleCache["/avocado"].exports.defineHttpSystemAction = defineSystemAction;
}

avocado = ModuleCache["/avocado"].exports;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             Module "simple-query"
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8ModuleSimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

try {
  require("simple-query");
}
catch (err) {
  console.error("while loading 'simple-query' module: %s", err);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
