var internalMembers = ["code", "error", "status", "duration", "failed", "total"];


function resultsToXml(results) {
  function xmlEscape(s) {
    return s.replace(/[<>&"]/g, function (c) {
      return "&"
        + { "<": "lt", ">": "gt", "&": "amp", "\"": "quot" }[c]
        + ";";
    });
  }

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

  for (var i in  results.shell_server_aql) {
    xml.elem("testsuite", {
      errors: 0,
      failures: results.shell_server_aql[i].failed,
      name: i,
      tests: results.shell_server_aql[i].total,
      time: results.shell_server_aql[i].duration
    });

    for (var j in  results.shell_server_aql[i]) {
      if (internalMembers.indexOf(j) === -1) {
        var result = results.shell_server_aql[i][j].status;
        var success = (typeof(result) === 'boolean')? result : false;
        
        xml.elem("testcase", {
          name: j,
          time: 0.0
        }, success);
      
        if (!success) {
          xml.elem("failure", { message: results.shell_server_aql[i][j].message }, true)
            .elem("/testcase");
        }
      }
    }

    xml.elem("/testsuite");
  }

  return xml.join("");
}


function main (argv) {
  var fs = require("fs");
  var print = require("internal").print;
  if (argv.length < 2) {
    print("Usage: unittest TESTNAME [OPTIONS]");
    return;
  }
  var test = argv[1];
  var options = {};
  if (argv.length >= 3) {
    options = JSON.parse(argv[2]);
  }
  var UnitTest = require("org/arangodb/testing").UnitTest;
  start_pretty_print();
  var r = UnitTest(test,options); 
  fs.write("UNITTEST_RESULT.json",JSON.stringify(r));
  fs.write("UNITTEST_RESULT_SUMMARY.txt",JSON.stringify(r.all_ok));
  try {
    x = resultsToXml(r);
  }
  catch (x) {
    print(x.message);
  }
  fs.write("UNITTEST_RESULT.xml", x);
  
  print(r);
}
