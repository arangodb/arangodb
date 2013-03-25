////////////////////////////////////////////////////////////////////////////////
/// @brief JSUnity wrapper
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

var internal = require("internal");
var console = require("console");

var TOTAL = 0;
var PASSED = 0;
var FAILED = 0;
var DURATION = 0;

var realJsUnityPath = "/jsunity/jsunity";
var realJsUnity = internal.loadModuleFile(realJsUnityPath).content;
eval(realJsUnity);

jsUnity.results.begin = function (total, suiteName) {
  print("Running " + (suiteName || "unnamed test suite"));
  print(" " + total + " test(s) found");
  print();
};

jsUnity.results.pass = function (index, testName) {
  print(internal.COLORS.COLOR_GREEN + " [PASSED] " + testName + internal.COLORS.COLOR_RESET);
};

jsUnity.results.fail = function (index, testName, message) {
  print(internal.COLORS.COLOR_RED + " [FAILED] " + testName + ": " + message + internal.COLORS.COLOR_RESET);
};

jsUnity.results.end = function (passed, failed, duration) {
  print(" " + passed + " test(s) passed");
  print(" " + ((failed > 0) ? internal.COLORS.COLOR_RED : internal.COLORS.COLOR_RESET) + failed + " test(s) failed" + internal.COLORS.COLOR_RESET);
  print(" " + duration + " millisecond(s) elapsed");
  print();
};

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief pad the given string to the maximum width provided
///
/// From: http://www.bennadel.com/blog/1927-Faking-Context-In-Javascript-s-Function-Constructor.htm
////////////////////////////////////////////////////////////////////////////////

function FunctionContext (func) {
  var body = "  for (var __i in context) {"
           + "    eval('var ' + __i + ' = context[__i];');"
           + "  }"
           + "  return " + func + ";";

  return new Function("context", body);
}

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Expresso
/// @{
///
/// based on:
///
/// Expresso
/// Copyright(c) TJ Holowaychuk <tj@vision-media.ca>
/// (MIT Licensed)
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief pad the given string to the maximum width provided
////////////////////////////////////////////////////////////////////////////////

function lpad (str, width) {
  str = String(str);

  var n = width - str.length;

  if (n < 1) {
    return str;
  }

  while (n--) {
    str = ' ' + str;
  }

  return str;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pad the given string to the maximum width provided
////////////////////////////////////////////////////////////////////////////////

function rpad (str, width) {
  str = String(str);

  var n = width - str.length;

  if (n < 1) {
    return str;
  }

  while (n--) {
    str = str + ' ';
  }

  return str;
}

////////////////////////////////////////////////////////////////////////////////
/// Total coverage for the given file data.
////////////////////////////////////////////////////////////////////////////////

function coverage (data, val) {
    var n = 0;

    for (var i = 0, len = data.length;  i < len;  ++i) {
      if (data[i] !== undefined && data[i] == val) {
        ++n;
      }
    }

    return n;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief populate code coverage data
////////////////////////////////////////////////////////////////////////////////

function populateCoverage (cov) {
  var boring = false;

  cov.LOC = 0;
  cov.SLOC = 0;
  cov.totalFiles = 0;
  cov.totalHits = 0;
  cov.totalMisses = 0;
  cov.coverage = 0;

  for (var name in cov) {
    var file = cov[name];

    if (Array.isArray(file)) {
      ++cov.totalFiles;

      if (! file.source) {
        file.source = [];
      }

      cov.totalHits += file.totalHits = coverage(file, true);
      cov.totalMisses += file.totalMisses = coverage(file, false);
      file.totalLines = file.totalHits + file.totalMisses;
      cov.SLOC += file.SLOC = file.totalLines;
      cov.LOC += file.LOC = file.source.length;
      file.coverage = (file.totalHits / file.totalLines) * 100;

      var width = file.source.length.toString().length;

      file.sourceLines = file.source.map(function(line, i) {
        ++i;
        var hits = lpad(file[i] === 0 ? 0 : (file[i] || ' '), 3);
                                           
        if (! boring) {
          if (file[i] === 0) {
            hits = '\x1b[31m' + hits + '\x1b[0m';
            line = '\x1b[41m' + line + '\x1b[0m';
          }
          else {
            hits = '\x1b[32m' + hits + '\x1b[0m';
          }
        }
                                           
        return '\n     ' + lpad(i, width) + ' | ' + hits + ' | ' + line;
      }).join('');
    }
  }

  cov.coverage = (cov.totalHits / cov.SLOC) * 100;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief report test coverage in tabular format
////////////////////////////////////////////////////////////////////////////////

function reportCoverage (cov, files) {
  print('\n   Test Coverage\n');

  var sep =     '   +------------------------------------------+----------+------+------+--------+';
  var lastSep = '                                              +----------+------+------+--------+';
  var line;

  print(sep);
  print('   | filename                                 | coverage | LOC  | SLOC | missed |');
  print(sep);

  for (var name in cov) {
    var file = cov[name];

    if (Array.isArray(file)) {
      line  = '';
      line += '   | ' + rpad(name, 40);
      line += ' | ' + lpad(file.coverage.toFixed(2), 8);
      line += ' | ' + lpad(file.LOC, 4);
      line += ' | ' + lpad(file.SLOC, 4);
      line += ' | ' + lpad(file.totalMisses, 6);
      line += ' |';

      print(line);
    }
  }

  print(sep);

  line  = '';
  line += '     ' + rpad('', 40);
  line += ' | ' + lpad(cov.coverage.toFixed(2), 8);
  line += ' | ' + lpad(cov.LOC, 4);
  line += ' | ' + lpad(cov.SLOC, 4);
  line += ' | ' + lpad(cov.totalMisses, 6);
  line += ' |';

  print(line);

  print(lastSep);

  var match = null;

  if (files == true) {
    match = function(name) {
      return name.match(/\.js$/);
    }
  }
  else if (files != null) {
    var fl = {};

    if (typeof files == "string") {
      fl[files] = true;
    }
    else {
      for (var i = 0;  i < files.length; ++i) {
        fl[files[i]] = true;
      }
    }

    match = function(name) {
      return name in fl;
    }
  }

  if (match != null) {
    for (var name in cov) {
      if (match(name)) {
        var file = cov[name];

        if (file.coverage < 100) {
          print('\n   ' + name + ':');
          print(file.sourceLines);
          print('\n');
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup JSUnity
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief runs a test with context
////////////////////////////////////////////////////////////////////////////////

function Run (tests) {
  var suite = jsUnity.compile(tests);
  var definition = tests();

  var tests = [];
  var setUp = undefined;
  var tearDown = undefined;

  if (definition.hasOwnProperty("setUp")) {
    setUp = definition.setUp;
  }

  if (definition.hasOwnProperty("tearDown")) {
    tearDown = definition.tearDown;
  }
  
  var scope = {};
  scope.setUp = setUp;
  scope.tearDown = tearDown;

  for (var key in definition) {
    if (key.indexOf("test") == 0) {
      var test = { name : key, fn : definition[key] };

      tests.push(test);
    }
    else if (key != "tearDown" && key != "setUp") {
      console.error("unknown function: %s", key);
    }
  }

  suite = new jsUnity.TestSuite(suite.suiteName, scope);

  suite.tests = tests;
  suite.setUp = setUp;
  suite.tearDown = tearDown;
  
  var result = jsUnity.run(suite);

  TOTAL += result.total;
  PASSED += result.passed;
  FAILED += result.failed;
  DURATION += result.duration;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief done with all tests
////////////////////////////////////////////////////////////////////////////////

function Done () {
//  console.log("%d total, %d passed, %d failed, %d ms", TOTAL, PASSED, FAILED, DURATION);
  internal.printf("%d total, %d passed, %d failed, %d ms", TOTAL, PASSED, FAILED, DURATION);
  print();

  var ok = FAILED == 0;

  TOTAL = 0;
  PASSED = 0;
  FAILED = 0;
  DURATION = 0;

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs a JSUnity test file
////////////////////////////////////////////////////////////////////////////////

function RunTest (path) {
  var content;
  var f;

  content = internal.read(path);

  content = "(function(){require('jsunity').jsUnity.attachAssertions();" + content + "})";
  f = internal.executeScript(content, undefined, path);
  
  if (f == undefined) {
    throw "cannot create context function";
  }

  return f();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs a JSUnity test file with coverage
////////////////////////////////////////////////////////////////////////////////

function RunCoverage (path, files) {
  RunTest(path);

  populateCoverage(_$jscoverage);
  reportCoverage(_$jscoverage, files);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs tests from command-line
////////////////////////////////////////////////////////////////////////////////

function RunCommandLineTests () {
  var result = true;
  var unitTests = internal.unitTests();

  for (var i = 0;  i < unitTests.length;  ++i) {
    var file = unitTests[i];
    print();
    print("running tests from file '" + file + "'");

    try {
      var ok = RunTest(file);

      result = result && ok;
    }
    catch (err) {
      print("cannot run test file '" + file + "': " + err);
      print(err.stack);
      result = false;
    }

    internal.wait(0); // force GC
  }

  internal.setUnitTestsResult(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup JSUnity
/// @{
////////////////////////////////////////////////////////////////////////////////

exports.jsUnity = jsUnity;
exports.run = Run;
exports.done = Done;
exports.runTest = RunTest; 
exports.runCoverage = RunCoverage; 
exports.runCommandLineTests = RunCommandLineTests;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
