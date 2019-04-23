////////////////////////////////////////////////////////////////////////////////
/// @brief JSHint wrapper
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Alan Plum
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var fs = require("fs");
fs.readdirSync = function() { return fs.listTree(arguments[0]).filter(file => file != ""); };
fs.existsSync = function() { return false; };
fs.ReadStream = {};
fs.ReadStream.prototype = {};
fs.WriteStream = {};
fs.WriteStream.prototype = {};
fs.realpathSync = fs.makeAbsolute;
fs.existsSync = fs.exists;
fs.closeSync = function () {};
fs.__patched = true;
var os = require("os");
os.homedir = () => process.cwd();
os.type = () => 'Linux';
var process = require("process");
process.version = 'v0.1';
var console = require("console");

var Linter = require("eslint/lib/linter");
var Config = require("eslint/lib/config");
var ConfigFile = require("eslint/lib/config/config-file");
var linter = new Linter();
var config = new Config({}, linter);
config = ConfigFile.load('./js/.eslintrc', config);

////////////////////////////////////////////////////////////////////////////////
/// @brief runs a JSLint test on a file
////////////////////////////////////////////////////////////////////////////////

function RunTest(path, options) {
  var content;

  try {
    content = fs.read(path);
  } catch (err) {
    console.error("cannot load test file '%s'", path);
    return false;
  }
  var result = {};
  try {
    var messages = linter.verify(content, config);
    result.passed = !messages.some(message => message.severity == 2);
    result.messages = messages;
  } catch (e) {
    result.passed = false;
    console.error(e);
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs tests from command-line
////////////////////////////////////////////////////////////////////////////////

function RunCommandLineTests(options) {
  var result = true;
  var tests = internal.unitTests();

  for (var i = 0; i < tests.length; ++i) {
    var file = tests[i];

    try {
      var testResult = RunTest(file, options);
      result = result && testResult && testResult.passed;
      
      for (var j = 0; j < testResult.messages.length; ++j) {
        var err = testResult.messages[j];

        if (!err) {
          continue;
        }
        var level = "UNKNOWN";
        var outFn = console.error.bind(console);
        if (err.severity == 1) {
          level = "WARN";
          outFn = console.warn.bind(console);
        } else if (err.severity == 2) {
          level = "ERROR";
          outFn = console.error.bind(console);
        }
        outFn(`jslint(${level}): ${file}:${err.line},${err.column} (Rule: ${err.ruleId}, Severity: ${err.severity}) ${err.message}`);
      }

      if (result) {
        console.log(`jslint: ${file} passed`);
      } else {
        console.log(`jslint: ${file} failed`);
      }
    } catch (err) {
      console.error(`cannot run test file "${file}": ${err}`);
      console.error(err.stack);
      result = false;
    }
  }

  internal.setUnitTestsResult(result);
}

exports.runTest = RunTest;
exports.runCommandLineTests = RunCommandLineTests;
