var internalMembers = ["code", "error", "status", "duration", "failed", "total", "message"];
var fs = require("fs");
var print = require("internal").print;


function resultsToXml(results, baseName) {
  "use strict";
  function xmlEscape(s) {
    return s.replace(/[<>&"]/g, function (c) {
      return "&"
        + { "<": "lt", ">": "gt", "&": "amp", "\"": "quot" }[c]
        + ";";
    });
  }

  for (var testrun in results) {
    
    for (var test in  results[testrun]) {
      var xml = [ "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" ];

      xml.text = function (s) {
        Array.prototype.push.call(this, s);
        return this;
      };
      
      xml.elem = function (tagName, attrs, close) {
        this.text("<").text(tagName);
        
        for (var a in attrs || {}) {
          this.text(" ").text(a).text("=\"")
            .text(xmlEscape(String(attrs[a]))).text("\"");
        }

        if (close) {
          this.text("/");
        }
        this.text(">\n");

        return this;
      };

      xml.elem("testsuite", {
        errors: 0,
        failures: results[testrun][test].failed,
        name: test,
        tests: results[testrun][test].total,
        time: results[testrun][test].duration
      });

      for (var oneTest in  results[testrun][test]) {
        if (internalMembers.indexOf(oneTest) === -1) {
          var result = results[testrun][test][oneTest].status;
          var success = (typeof(result) === 'boolean')? result : false;
          
          xml.elem("testcase", {
            name: oneTest,
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

      if ((!results[testrun][test].status)                 && 
          results[testrun][test].hasOwnProperty('message')) 
      {

        xml.elem("testcase", {
          name: 'all tests in ' + test,
          time: results[testrun][test].duration
        }, false);
        xml.elem("failure");
        xml.text('<![CDATA[' + JSON.stringify(results[testrun][test].message) + ']]>\n');
        xml.elem("/failure");
        xml.elem("/testcase");
      }
      xml.elem("/testsuite");
      var fn = baseName + testrun.replace(/\//g, '_') + '_' + test.replace(/\//g, '_') + ".xml";
      fs.write(fn, xml.join(""));

    }
  }
}


function main (argv) {
  var test = argv[1];
  var options = {};
  var r;
  if (argv.length >= 3) {
    try {
      options = JSON.parse(argv[2]);
    }
    catch (x) {
      print("failed to parse the json options");
      print(x);
      return -1;
    }
  }
  options.jsonReply = true;
  var UnitTest = require("org/arangodb/testing");
  start_pretty_print();

  try {
    r = UnitTest.UnitTest(test,options); 
  }
  catch (x) {
    print("Caught exception during test execution!");
    print(x.message);
    print(JSON.stringify(r));
  }
  fs.write("UNITTEST_RESULT.json",JSON.stringify(r));
  fs.write("UNITTEST_RESULT_SUMMARY.txt",JSON.stringify(r.all_ok));
  try {
    resultsToXml(r, "UNITTEST_RESULT_");
  }
  catch (x) {
    print("Exception while serializing status xml!");
    print(x.message);
    print(JSON.stringify(r));
  }

  UnitTest.unitTestPrettyPrintResults(r);
}
