/*jshint globalstrict:false, unused:false */
/*global start_pretty_print */

var yaml = require("js-yaml");

var UnitTest = require("org/arangodb/testing");

var internalMembers = UnitTest.internalMembers;
var fs = require("fs");
var internal = require("internal");
var print = internal.print;

function makePathGeneric (path) {
  "use strict";
  return path.split(fs.pathSeparator);
}

function resultsToXml(results, baseName, cluster) {
  "use strict";

  function xmlEscape(s) {
    return s.replace(/[<>&"]/g, function (c) {
      return "&"
        + { "<": "lt", ">": "gt", "&": "amp", "\"": "quot" }[c]
        + ";";
    });
  }

  function buildXml () {
    var xml = [ "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" ];

    xml.text = function (s) {
      Array.prototype.push.call(this, s);
      return this;
    };
      
    xml.elem = function (tagName, attrs, close) {
      this.text("<").text(tagName);
      attrs = attrs || { }; 
            
      for (var a in attrs) {
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

  var clprefix = '';
  if (cluster) {
    clprefix = 'CL_';
  }

  for (var testrun in results) {
    if ((internalMembers.indexOf(testrun) === -1) && (results.hasOwnProperty(testrun))) { 
      for (var test in results[testrun]) {

        if ((internalMembers.indexOf(test) === -1) && 
            results[testrun].hasOwnProperty(test) && 
            !results[testrun][test].hasOwnProperty('skipped')) { 

          var xml = buildXml();

          var failuresFound = "";
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

          for (var oneTest in  results[testrun][test]) {
            if (internalMembers.indexOf(oneTest) === -1) {
              var result = results[testrun][test][oneTest].status;
              var success = (typeof(result) === 'boolean')? result : false;
          
              xml.elem("testcase", {
                name: clprefix + oneTest,
                time: results[testrun][test][oneTest].duration
              }, success);
          
              if (! success) {
                xml.elem("failure");
                xml.text('<![CDATA[' + results[testrun][test][oneTest].message + ']]>\n');
                xml.elem("/failure");
                xml.elem("/testcase");
              }
            }
          }

          if ((! results[testrun][test].status)                 && 
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
          var fn = makePathGeneric(baseName + clprefix + testrun + '_' + test + ".xml").join('_');
          fs.write("out/" + fn, xml.join(""));
        }
      }
    }
  }
}

function main (argv) {
  "use strict";
  var test = argv[1];
  var options = {};
  if (argv.length >= 3) {
    try {
      if (argv[2].slice(0,1) === '{') {
        options = JSON.parse(argv[2]);
      }
      else {
        options = internal.parseArgv(argv, 2);
      }
    }
    catch (x) {
      print("failed to parse the json options: " + x.message);
      return -1;
    }
  }
  options.jsonReply = true;
  start_pretty_print();
  var r = { };

  try {
    r = UnitTest.UnitTest(test, options) || { };
  }
  catch (x) {
    print("caught exception during test execution!");
    print(x.message);
    print(x.stack);
    print(JSON.stringify(r));
  }

  fs.write("out/UNITTEST_RESULT.json", JSON.stringify(r));
  fs.write("out/UNITTEST_RESULT_SUMMARY.txt", JSON.stringify(! r.crashed));

  try {
    resultsToXml(r, "UNITTEST_RESULT_", (options.hasOwnProperty('cluster') && options.cluster));
  }
  catch (x) {
    print("exception while serializing status xml!");
    print(x.message);
    print(JSON.stringify(r));
  }

  UnitTest.unitTestPrettyPrintResults(r);

  if (r.hasOwnProperty("crashed") && r.crashed) {
    return -1;
  }
  return 0;
}
