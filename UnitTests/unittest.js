/*jshint globalstrict:false, unused:false */
/*global print, start_pretty_print, ARGUMENTS */
'use strict';

const yaml = require("js-yaml");
const _ = require("lodash");

const UnitTest = require("@arangodb/testing");

const internalMembers = UnitTest.internalMembers;
const fs = require("fs");
const internal = require("internal");
const inspect = internal.inspect;

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

  const isSignificant = function(a, b) {
    return (internalMembers.indexOf(b) === -1) && a.hasOwnProperty(b);
  };

  for (let resultName in results) {
    if (isSignificant(results, resultName)) {
      let run = results[resultName];

      for (let runName in run) {
        if (isSignificant(run, runName)) {
          const current = run[runName];

          if (current.skipped) {
            continue;
          }

          let xml = buildXml();
          let failuresFound = "";

          if (current.hasOwnProperty('failed')) {
            failuresFound = current.failed;
          }

          xml.elem("testsuite", {
            errors: 0,
            failures: failuresFound,
            name: clprefix + runName,
            tests: current.total,
            time: current.duration
          });

          for (let oneTestName in current) {
            if (isSignificant(current, oneTestName)) {
              const oneTest = current[oneTestName];
              const result = oneTest.status || false;
              const success = (typeof(result) === 'boolean') ? result : false;

              xml.elem("testcase", {
                name: clprefix + oneTestName,
                time: oneTest.duration
              }, success);

              if (!success) {
                xml.elem("failure");
                xml.text('<![CDATA[' + oneTest.message + ']]>\n');
                xml.elem("/failure");
                xml.elem("/testcase");
              }
            }
          }

          if (!current.status) {
            xml.elem("testcase", {
              name: 'all tests in ' + clprefix + runName,
              time: current.duration
            }, false);

            xml.elem("failure");
            xml.text('<![CDATA[' +
              JSON.stringify(current.message || "unknown failure reason") +
              ']]>\n');
            xml.elem("/failure");
            xml.elem("/testcase");
          }

          xml.elem("/testsuite");

          const fn = makePathGeneric(baseName + clprefix +
            resultName + '_' + runName + ".xml").join('_');

          fs.write("out/" + fn, xml.join(""));
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the test using testing.js
////////////////////////////////////////////////////////////////////////////////

function main(argv) {
  const test = argv[0];
  let options = {};

  if (argv.length >= 2) {
    try {
      if (argv[1].slice(0, 1) === '{') {
        options = JSON.parse(argv[1]);
      } else {
        options = internal.parseArgv(argv, 1);
      }
    } catch (x) {
      print("failed to parse the json options: " + x.message);
      return -1;
    }
  }

  options.jsonReply = true;

  fs.makeDirectoryRecursive("out");

  start_pretty_print();

  // run the test and store the result
  let r = {};

  try {
    r = UnitTest.UnitTest(test, options) || {};
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
    all_ok: false,
    crashed: true
  });

  // whether or not there was an error 
  fs.write("out/UNITTEST_RESULT_EXECUTIVE_SUMMARY.json", String(r.all_ok));

  if (options.writeXmlReport) {
    fs.write("out/UNITTEST_RESULT.json", inspect(r));
    fs.write("out/UNITTEST_RESULT_CRASHED.txt", String(r.crashed));

    try {
      resultsToXml(r,
        "UNITTEST_RESULT_", (options.hasOwnProperty('cluster') && options.cluster));
    } catch (x) {
      print("exception while serializing status xml!");
      print(x.message);
      print(x.stack);
      print(JSON.stringify(r));
    }
  }

  UnitTest.unitTestPrettyPrintResults(r);

  if (r.hasOwnProperty("crashed") && r.crashed) {
    return -1;
  }

  return 0;
}

main(ARGUMENTS);
