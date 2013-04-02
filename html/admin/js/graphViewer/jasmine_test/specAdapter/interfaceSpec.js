/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global it, expect */


////////////////////////////////////////////////////////////////////////////////
/// @brief Graph functionality
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
/// @author Michael Hackstein
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


var describeInterface = function (testee) {
  "use strict";
    
    it('should comply to the Adapter Interface', function() {
      this.addMatchers({
        toHaveFunction: function(func, argCounter) {
          var obj = this.actual;
          this.message = function(){
            return testee.constructor.name
              + " should react to function "
              + func
              + " with at least "
              + argCounter
              + " arguments.";
          };
          if (typeof(obj[func]) !== "function") {
            return false;
          }
          if (obj[func].length < argCounter) {
            return false;
          }
          return true;
        }
      });
      
      // Add functions to load here:
      expect(testee).toHaveFunction("loadNodeFromTreeById", 2);
      expect(testee).toHaveFunction("requestCentralityChildren", 2);
      expect(testee).toHaveFunction("loadNodeFromTreeByAttributeValue", 3);
      expect(testee).toHaveFunction("createEdge", 2);
      expect(testee).toHaveFunction("deleteEdge", 2);
      expect(testee).toHaveFunction("patchEdge", 3);
      expect(testee).toHaveFunction("createNode", 2);
      expect(testee).toHaveFunction("deleteNode", 2);
      expect(testee).toHaveFunction("patchNode", 3);
    });
    
};
