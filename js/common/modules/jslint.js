////////////////////////////////////////////////////////////////////////////////
/// @brief JSLint wrapper
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

var internal = require("internal");
var console = require("console");

var realJsLintPath = "/jslint/jslint";
var realJsLint = internal.loadModuleFile(realJsLintPath).content;
eval(realJsLint);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup JSLint
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief runs a JSLint test on a file
////////////////////////////////////////////////////////////////////////////////

function RunTest (path, options) {
  var content;

  try {
    content = internal.read(path);
  }
  catch (err) {
    console.error("cannot load test file '%s'", path);
    return;
  }

  var result = { };
  result["passed"] = JSLINT(content, options);

  if (JSLINT.errors) {
    result["errors"] = JSLINT.errors;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs tests from command-line
////////////////////////////////////////////////////////////////////////////////

function RunCommandLineTests (options) {
  var result = true;

  for (var i = 0;  i < SYS_UNIT_TESTS.length;  ++i) {
    var file = SYS_UNIT_TESTS[i];

    try {
      var testResult = RunTest(file, options);
      result = result && testResult && testResult.passed;
      if (testResult && (! testResult.passed && testResult.errors)) {
        for (var i = 0; i < testResult.errors.length; ++i) {
          var err = testResult.errors[i];
          if (! err) {
            continue;
          }

          var position = file + ":" + err.line + ", " + err.character;
          var reason = err.reason;
          console.error("jslint: %s : %s", position, reason);
        }
      }
      else {
        console.info("jslint: %s passed", file);
      }
    }
    catch (err) {
      print("cannot run test file '" + file + "': " + err);
      print(err.stack);
      result = false;
    }
  }


  SYS_UNIT_TESTS_RESULT = result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup JSLint
/// @{
////////////////////////////////////////////////////////////////////////////////

exports.runTest = RunTest;
exports.runCommandLineTests = RunCommandLineTests;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
