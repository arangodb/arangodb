/*jshint globalstrict:false, unused:false */
/*global print, start_pretty_print, ARGUMENTS */
'use strict';

const yaml = require("js-yaml");

const UnitTest = require("@arangodb/testing");

const internalMembers = UnitTest.internalMembers;
const fs = require("fs");
const internal = require("internal");

function makePathGeneric(path) {
  return path.split(fs.pathSeparator);
}

function resultsToXml(results, baseName, cluster) {
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

  let clprefix = '';

  if (cluster) {
    clprefix = 'CL_';
  }

  for (let testrun in results) {
    if ((internalMembers.indexOf(testrun) === -1) && (results.hasOwnProperty(testrun))) {
      for (let test in results[testrun]) {
        if ((internalMembers.indexOf(test) === -1) &&
          results[testrun].hasOwnProperty(test) &&
          !results[testrun][test].hasOwnProperty('skipped')) {

          let xml = buildXml();
          let failuresFound = "";

          if (results[testrun][test].hasOwnProperty('failed')) {
            failuresFound = results[testrun][test].failed;
          }

          xml.elem("testsuite", {
            errors: 0,
            failures: failuresFound,
            name: clprefix + test,
            tests: results[testrun][test].total,
            time: results[testrun][test].duration
          });

          for (let oneTest in results[testrun][test]) {
            if (internalMembers.indexOf(oneTest) === -1) {
              const result = results[testrun][test][oneTest].status;
              const success = (typeof(result) === 'boolean') ? result : false;

              xml.elem("testcase", {
                name: clprefix + oneTest,
                time: results[testrun][test][oneTest].duration
              }, success);

              if (!success) {
                xml.elem("failure");
                xml.text('<![CDATA[' + results[testrun][test][oneTest].message + ']]>\n');
                xml.elem("/failure");
                xml.elem("/testcase");
              }
            }
          }

          if ((!results[testrun][test].status) &&
            results[testrun][test].hasOwnProperty('message')) {

            xml.elem("testcase", {
              name: 'all tests in ' + clprefix + test,
              time: results[testrun][test].duration
            }, false);

            xml.elem("failure");
            xml.text('<![CDATA[' + JSON.stringify(results[testrun][test].message) + ']]>\n');
            xml.elem("/failure");
            xml.elem("/testcase");
          }

          xml.elem("/testsuite");

          const fn = makePathGeneric(baseName + clprefix +
            testrun + '_' + test + ".xml").join('_');

          fs.write("out/" + fn, xml.join(""));
        }
      }
    }
  }
}

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
    }

    print(JSON.stringify(r));
  }

  // whether or not there was an error 
  fs.write("out/UNITTEST_RESULT_EXECUTIVE_SUMMARY.json", JSON.stringify(r.all_ok));

  if (options.writeXmlReport) {
    fs.write("out/UNITTEST_RESULT.json", JSON.stringify(r));

    // should be renamed to UNITTEST_RESULT_CRASHED, because that's what it actually contains
    fs.write("out/UNITTEST_RESULT_SUMMARY.txt", JSON.stringify(!r.crashed));

    try {
      resultsToXml(r, "UNITTEST_RESULT_", (options.hasOwnProperty('cluster') && options.cluster));
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
