////////////////////////////////////////////////////////////////////////////////
/// @brief setup stuff for import tests
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

(function () {
  var db = require("org/arangodb").db;

  db._drop("UnitTestsImportJson1");
  db._drop("UnitTestsImportJson2");
  db._drop("UnitTestsImportJson3");
  db._drop("UnitTestsImportJson4");
  db._drop("UnitTestsImportCsv1");
  db._drop("UnitTestsImportCsv2");
  db._drop("UnitTestsImportTsv1");
  db._drop("UnitTestsImportTsv2");
  db._drop("UnitTestsImportVertex");
  db._drop("UnitTestsImportEdge");
})();

return true;

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
