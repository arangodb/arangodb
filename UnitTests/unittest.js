/*jshint globalstrict:false, unused:false */
/*global print, start_pretty_print, ARGUMENTS */
'use strict';

const yaml = require("js-yaml");
const _ = require("lodash");

const UnitTest = require("@arangodb/testing");

const internalMembers = UnitTest.internalMembers;
const fs = require("fs");
const internal = require("internal"); // js/common/bootstrap/modules/internal.js
const inspect = internal.inspect;

let testOutputDirectory;

function makePathGeneric(path) {
  return path.split(fs.pathSeparator);
}

function xmlEscape(s) {
  return s.replace(/[<>&"]/g, function(c) {
    return "&" + {
      "<": "lt",
      ">": "gt",
      "&": "amp",
      "\"": "quot"
    }[c] + ";";
  });
}

function buildXml() {
  let xml = ["<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"];

  xml.text = function(s) {
    Array.prototype.push.call(this, s);
    return this;
  };

  xml.elem = function(tagName, attrs, close) {
    this.text("<").text(tagName);
    attrs = attrs || {};

    for (let a in attrs) {
      if (attrs.hasOwnProperty(a)) {
        this.text(" ").text(a).text("=\"")
          .text(xmlEscape(String(attrs[a]))).text("\"");
      }
    }

    if (close) {
      this.text("/");
    }

    this.text(">\n");

    return this;
  };

  return xml;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts results to XML representation
////////////////////////////////////////////////////////////////////////////////

function resultsToXml(results, baseName, cluster) {
  let clprefix = '';

  if (cluster) {
    clprefix = 'CL_';
  }

  let cleanedResults = UnitTest.unwurst(results);
  print(JSON.stringify(cleanedResults));
  cleanedResults.forEach(suite => {
    print(suite.suiteName);
    let xml = buildXml();
    xml.elem('testsuite', {
      errors: suite.tests.filter(test => test.hasOwnProperty('error')).length,
      failures: suite.tests.filter(test => test.hasOwnProperty('failure')).length,
      tests: suite.tests.length,
      name: suite.suiteName,
    });

    suite.tests.forEach(test => {
      xml.elem('testcase', {
        name: test.testName,
      });
      if (test.error) {
        xml.elem('error');
        xml.text('<![CDATA[' + test.error + ']]>\n');
        xml.elem('/error');
      }
      if (test.failure) {
        xml.elem('failure');
        xml.text('<![CDATA[' + test.failure + ']]>\n');
        xml.elem('/failure');
      }
      xml.elem('/testcase');
    });
    xml.elem('/testsuite');

    const fn = makePathGeneric(baseName + suite.suiteName + ".xml").join('_');
    fs.write(testOutputDirectory + fn, xml.join(""));
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the test using testing.js
////////////////////////////////////////////////////////////////////////////////

function main(argv) {
  start_pretty_print();

  // parse arguments
  let testSuits = []; // e.g all, http_server, recovery, ...
  let options = {};

  while (argv.length >= 1) {
    if (argv[0].slice(0, 1) === '{') { // stop parsing if there is a json document
      break;
    }

    if (argv[0].slice(0, 1) === '-') { // break parsing if we hit some -option
      break;
    }

    testSuits.push(argv[0]); // append first arg to test suits
    argv = argv.slice(1);    // and remove first arg (c++:pop_front/bash:shift)
  }

  // convert arguments
  if (argv.length >= 1) {
    try {
      if (argv[0].slice(0, 1) === '{') {
        options = JSON.parse(argv[0]); // parse options form json
      } else {
        options = internal.parseArgv(argv, 0); // parse option with parseArgv function
      }
    } catch (x) {
      print("failed to parse the json options: " + x.message + "\n" + String(x.stack));
      print("argv: ", argv);
      return -1;
    }
  }

  if (options.hasOwnProperty('testOutput')) {
    testOutputDirectory = options.testOutput + '/';
  } else {
    testOutputDirectory = 'out/';
  }

  options.testOutputDirectory = testOutputDirectory;
  
  // force json reply
  options.jsonReply = true;

  // create output directory
  fs.makeDirectoryRecursive(testOutputDirectory);

  // run the test and store the result
  let r = {}; // result
  try {
    // run tests
    r = UnitTest.unitTest(testSuits, options, testOutputDirectory) || {};
  } catch (x) {
    print("caught exception during test execution!");

    if (x.message !== undefined) {
      print(x.message);
    }

    if (x.stack !== undefined) {
      print(x.stack);
    } else {
      print(x);

    }

    print(JSON.stringify(r));
  }

  _.defaults(r, {
    status: false,
    crashed: true
  });

  // whether or not there was an error 
  fs.write(testOutputDirectory + "/UNITTEST_RESULT_EXECUTIVE_SUMMARY.json", String(r.status));

  if (options.writeXmlReport) {
    let j;

    try {
      j = JSON.stringify(r);
    } catch (err) {
      j = inspect(r);
    }

    fs.write(testOutputDirectory + "/UNITTEST_RESULT.json", j);
    fs.write(testOutputDirectory + "/UNITTEST_RESULT_CRASHED.json", String(r.crashed));

    try {
      resultsToXml(r,
        "UNITTEST_RESULT_", (options.hasOwnProperty('cluster') && options.cluster));
    } catch (x) {
      print("exception while serializing status xml!");
      print(x.message);
      print(x.stack);
      print(inspect(r));
    }
  }

  // creates yaml like dump at the end
  UnitTest.unitTestPrettyPrintResults(r, testOutputDirectory, options);

  return r.status;
}

let result = main(ARGUMENTS);
if (!result) {
  // force an error in the console
  throw 'peng!';
}
