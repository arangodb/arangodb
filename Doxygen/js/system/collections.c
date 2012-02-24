////////////////////////////////////////////////////////////////////////////////
/// @brief system actions for collections
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


void dummy_30 ();

// -----------------------------------------------------------------------------
// --SECTION--                                            administration actions
// -----------------------------------------------------------------------------

void dummy_34 ();

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ActionsAdmin
/// @{
////////////////////////////////////////////////////////////////////////////////

void dummy_39 ();

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about all collections
///
/// @REST{GET /_system/collections}
///
/// Returns information about all collections of the database. The returned
/// array contains the following entries.
///
/// - path: The server directory containing the database.
/// - collections : An associative array of all collections.
///
/// An entry of collections is again an associative array containing the
/// following entries.
///
/// - name: The name of the collection.
/// - status: The status of the collection. 1 = new born, 2 = unloaded,
///     3 = loaded, 4 = corrupted.
///
/// @verbinclude rest15
////////////////////////////////////////////////////////////////////////////////






void dummy_87 ();

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a collection
///
/// @REST{GET /_system/collection/load?collection=@FA{identifier}}
///
/// Loads a collection into memory.
///
/// @verbinclude restX
////////////////////////////////////////////////////////////////////////////////



void dummy_115 ();

////////////////////////////////////////////////////////////////////////////////
/// @brief information about a collection
///
/// @REST{GET /_system/collection/info?collection=@FA{identifier}}
///
/// Returns information about a collection
///
/// @verbinclude rest16
////////////////////////////////////////////////////////////////////////////////



void dummy_147 ();

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about all documents
///
/// @REST{GET /_system/documents}
////////////////////////////////////////////////////////////////////////////////


void dummy_166 ();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

void dummy_170 ();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
