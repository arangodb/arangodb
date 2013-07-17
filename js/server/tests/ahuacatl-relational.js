////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, relational operators
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

var jsunity = require("jsunity");
var helper = require("org/arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlRelationalTestSuite () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test compare order
////////////////////////////////////////////////////////////////////////////////
    
    testCompareOrder : function () {
      assertEqual([ true ], getQueryResults("RETURN null < false"));
      assertEqual([ true ], getQueryResults("RETURN false < true"));
      assertEqual([ true ], getQueryResults("RETURN true < -5"));
      assertEqual([ true ], getQueryResults("RETURN -5 < -0"));
      assertEqual([ true ], getQueryResults("RETURN 0 < 5"));
      assertEqual([ true ], getQueryResults("RETURN 5 < \"\""));
      assertEqual([ true ], getQueryResults("RETURN \"\" < \"A\""));
      assertEqual([ true ], getQueryResults("RETURN \"A\" < \"a\""));
      assertEqual([ true ], getQueryResults("RETURN \"a\" < [ ]"));
      assertEqual([ true ], getQueryResults("RETURN \"a\" < [ false ]"));
      assertEqual([ true ], getQueryResults("RETURN [ false ] < [ false, false ]"));
      assertEqual([ true ], getQueryResults("RETURN [ false, false ] < { }"));
      assertEqual([ true ], getQueryResults("RETURN { } < { A : false }"));
      assertEqual([ true ], getQueryResults("RETURN { a : false } < { A : false }"));
      assertEqual([ true ], getQueryResults("RETURN { a : false } < { a : true }"));
      assertEqual([ true ], getQueryResults("RETURN { a : true} < { a : true, b : false }"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test equality
////////////////////////////////////////////////////////////////////////////////
    
    testRelationalEq : function () {
      assertEqual([ true ], getQueryResults("RETURN null == null"));
      assertEqual([ true ], getQueryResults("RETURN false == false"));
      assertEqual([ true ], getQueryResults("RETURN true == true"));
      assertEqual([ true ], getQueryResults("RETURN 0 == 0"));
      assertEqual([ true ], getQueryResults("RETURN 1 == 1"));
      assertEqual([ true ], getQueryResults("RETURN -1 == -1"));
      assertEqual([ true ], getQueryResults("RETURN -1.5 == -1.5"));
      assertEqual([ true ], getQueryResults("RETURN \"\" == \"\""));
      assertEqual([ true ], getQueryResults("RETURN \"a\" == \"a\""));
      assertEqual([ true ], getQueryResults("RETURN \" \" == \" \""));
      assertEqual([ true ], getQueryResults("RETURN \"A\" == \"A\""));
      assertEqual([ true ], getQueryResults("RETURN [ ] == [ ]"));
      assertEqual([ true ], getQueryResults("RETURN [ null ] == [ null ]"));
      assertEqual([ true ], getQueryResults("RETURN [ 0 ] == [ 0 ]"));
      assertEqual([ true ], getQueryResults("RETURN [ 1 ] == [ 1 ]"));
      assertEqual([ true ], getQueryResults("RETURN [ [ ] ] == [ [ ] ]"));
      assertEqual([ true ], getQueryResults("RETURN [ [ false ], null ] == [ [ false ], null ]"));
      assertEqual([ true ], getQueryResults("RETURN { } == { }"));
      assertEqual([ true ], getQueryResults("RETURN { a : 1 } == { a : 1 }"));
      assertEqual([ true ], getQueryResults("RETURN { a : 1, b : 1 } == { b : 1, a : 1 }"));
      assertEqual([ true ], getQueryResults("RETURN { a : 3, b : 1 } == { b : 1, a : 3 }"));
      assertEqual([ true ], getQueryResults("RETURN { b : 1 } == { b : 1, a : null }"));
      assertEqual([ true ], getQueryResults("RETURN { a : null, b : 1 } == { b : 1 }"));

      assertEqual([ false ], getQueryResults("RETURN null == false"));
      assertEqual([ false ], getQueryResults("RETURN null == true"));
      assertEqual([ false ], getQueryResults("RETURN null == 0"));
      assertEqual([ false ], getQueryResults("RETURN null == 1"));
      assertEqual([ false ], getQueryResults("RETURN null == -1"));
      assertEqual([ false ], getQueryResults("RETURN null == -1.5"));
      assertEqual([ false ], getQueryResults("RETURN null == \"\""));
      assertEqual([ false ], getQueryResults("RETURN null == \"a\""));
      assertEqual([ false ], getQueryResults("RETURN null == \" \""));
      assertEqual([ false ], getQueryResults("RETURN null == \"A\""));
      assertEqual([ false ], getQueryResults("RETURN null == [ ]"));
      assertEqual([ false ], getQueryResults("RETURN null == { }"));

      assertEqual([ false ], getQueryResults("RETURN false == null"));
      assertEqual([ false ], getQueryResults("RETURN false == true"));
      assertEqual([ false ], getQueryResults("RETURN false == 0"));
      assertEqual([ false ], getQueryResults("RETURN false == 1"));
      assertEqual([ false ], getQueryResults("RETURN false == -1"));
      assertEqual([ false ], getQueryResults("RETURN false == -1.5"));
      assertEqual([ false ], getQueryResults("RETURN false == \"\""));
      assertEqual([ false ], getQueryResults("RETURN false == \"a\""));
      assertEqual([ false ], getQueryResults("RETURN false == \" \""));
      assertEqual([ false ], getQueryResults("RETURN false == \"A\""));
      assertEqual([ false ], getQueryResults("RETURN false == [ ]"));
      assertEqual([ false ], getQueryResults("RETURN false == { }"));

      assertEqual([ false ], getQueryResults("RETURN true == null"));
      assertEqual([ false ], getQueryResults("RETURN true == false"));
      assertEqual([ false ], getQueryResults("RETURN true == 0"));
      assertEqual([ false ], getQueryResults("RETURN true == 1"));
      assertEqual([ false ], getQueryResults("RETURN true == -1"));
      assertEqual([ false ], getQueryResults("RETURN true == -1.5"));
      assertEqual([ false ], getQueryResults("RETURN true == \"\""));
      assertEqual([ false ], getQueryResults("RETURN true == \"a\""));
      assertEqual([ false ], getQueryResults("RETURN true == \" \""));
      assertEqual([ false ], getQueryResults("RETURN true == \"A\""));
      assertEqual([ false ], getQueryResults("RETURN true == [ ]"));
      assertEqual([ false ], getQueryResults("RETURN true == { }"));

      assertEqual([ false ], getQueryResults("RETURN 0 == null"));
      assertEqual([ false ], getQueryResults("RETURN 0 == false"));
      assertEqual([ false ], getQueryResults("RETURN 0 == true"));
      assertEqual([ false ], getQueryResults("RETURN 0 == 1"));
      assertEqual([ false ], getQueryResults("RETURN 0 == -1"));
      assertEqual([ false ], getQueryResults("RETURN 0 == -1.5"));
      assertEqual([ false ], getQueryResults("RETURN 0 == \"\""));
      assertEqual([ false ], getQueryResults("RETURN 0 == \"a\""));
      assertEqual([ false ], getQueryResults("RETURN 0 == \" \""));
      assertEqual([ false ], getQueryResults("RETURN 0 == \"A\""));
      assertEqual([ false ], getQueryResults("RETURN 0 == [ ]"));
      assertEqual([ false ], getQueryResults("RETURN 0 == { }"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unequality
////////////////////////////////////////////////////////////////////////////////
    
    testRelationalNe : function () {
      assertEqual([ true ], getQueryResults("RETURN null != false"));
      assertEqual([ true ], getQueryResults("RETURN null != true"));
      assertEqual([ true ], getQueryResults("RETURN null != 0"));
      assertEqual([ true ], getQueryResults("RETURN null != 1"));
      assertEqual([ true ], getQueryResults("RETURN null != -1"));
      assertEqual([ true ], getQueryResults("RETURN null != \"\""));
      assertEqual([ true ], getQueryResults("RETURN null != \"a\""));
      assertEqual([ true ], getQueryResults("RETURN null != \" \""));
      assertEqual([ true ], getQueryResults("RETURN null != \"A\""));
      assertEqual([ true ], getQueryResults("RETURN null != [ ]"));
      assertEqual([ true ], getQueryResults("RETURN null != { }"));
      
      assertEqual([ false ], getQueryResults("RETURN null != null"));
      assertEqual([ false ], getQueryResults("RETURN false != false"));
      assertEqual([ false ], getQueryResults("RETURN true != true"));
      assertEqual([ false ], getQueryResults("RETURN 0 != 0"));
      assertEqual([ false ], getQueryResults("RETURN 1 != 1"));
      assertEqual([ false ], getQueryResults("RETURN -1 != -1"));
      assertEqual([ false ], getQueryResults("RETURN -1.5 != -1.5"));
      assertEqual([ false ], getQueryResults("RETURN \"\" != \"\""));
      assertEqual([ false ], getQueryResults("RETURN \"a\" != \"a\""));
      assertEqual([ false ], getQueryResults("RETURN \" \" != \" \""));
      assertEqual([ false ], getQueryResults("RETURN \"A\" != \"A\""));
      assertEqual([ false ], getQueryResults("RETURN [ ] != [ ]"));
      assertEqual([ false ], getQueryResults("RETURN [ null ] != [ null ]"));
      assertEqual([ false ], getQueryResults("RETURN [ 0 ] != [ 0 ]"));
      assertEqual([ false ], getQueryResults("RETURN [ 1 ] != [ 1 ]"));
      assertEqual([ false ], getQueryResults("RETURN [ [ ] ] != [ [ ] ]"));
      assertEqual([ false ], getQueryResults("RETURN [ [ false ], null ] != [ [ false ], null ]"));
      assertEqual([ false ], getQueryResults("RETURN { } != { }"));
      assertEqual([ false ], getQueryResults("RETURN { a : 1 } != { a : 1 }"));
      assertEqual([ false ], getQueryResults("RETURN { a : 1, b : 1 } != { b : 1, a : 1 }"));
      assertEqual([ false ], getQueryResults("RETURN { a : 3, b : 1 } != { b : 1, a : 3 }"));
      assertEqual([ false ], getQueryResults("RETURN { b : 1 } != { b : 1, a : null }"));
      assertEqual([ false ], getQueryResults("RETURN { a : null, b : 1 } != { b : 1 }"));
    }


  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlRelationalTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
