var internalMembers = ["code", "error", "status", "duration", "failed", "total"];
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

        close && this.text("/");
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

      xml.elem("/testsuite");
      var fn = baseName + testrun.replace(/\//g, '_') + '_' + test.replace(/\//g, '_') + ".xml";
      //print('Writing: '+ fn);
      fs.write(fn, xml.join(""));

    }
  }
}


function main (argv) {
  if (argv.length < 2) {
    print("Usage: unittest TESTNAME [OPTIONS]");
    return;
  }
  var test = argv[1];
  var options = {};
  if (argv.length >= 3) {
    options = JSON.parse(argv[2]);
  }
  options.jsonReply = true;
  var UnitTest = require("org/arangodb/testing");
  start_pretty_print();
  var r = UnitTest.UnitTest(test,options); 
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
