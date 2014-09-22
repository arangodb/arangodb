/*global require, exports
*/
var fs = require("fs");
var measurements = ["time",
                    "ast optimization",
                    "plan instanciation",
                    "parsing",
                    "plan optimization",
                    "execution",
                    "initialization"];

var generatePerfReport = function (reportName, testdata, longdesc) {
  "use strict";
  var i;
  var x = '<?xml version="1.0" encoding="UTF-8"?>\n' +
    '<report name="' + reportName + '" categ="' + reportName + '">\n' + 
    '<start> <date format="YYYYMMDD" val="20000101" /> <time format="HHMMSS" val="195043" /> </start>\n'; //// TODO: zeit einsetzen.


  for (var testname in testdata) {
    if (testdata.hasOwnProperty(testname)) {
      for (var testrunner in testdata[testname]) {
        if (testdata[testname].hasOwnProperty(testrunner)) {
          x = x + '\t<test name="/' + testname + '/' + testrunner + '" executed="yes">\n';
          x = x + '\t\t<description><![CDATA[This is the description of the test ' + testname + ' executed by the testrunner ' + testrunner + ']]></description>\n';
          x = x + '\t\t<targets>\n\t\t\t<target threaded="false">AQL</target>\n\t\t</targets>\n';

          var oneResultSet = testdata[testname][testrunner][0].calced;
          var j = 0;
          for (i = 0; i < measurements.length; i++) {
            if (oneResultSet.hasOwnProperty([measurements[i]])) {
              var s = oneResultSet[measurements[0]].sum;
              var t = oneResultSet[measurements[0]].avg;
              x += '\t\t<commandline rank="' + j + '" time="20100128-195406.590832" duration="' + s + '">Sum ' + measurements[i] + '</commandline>\n';
              j += 1;

              x += '\t\t<commandline rank="' + j + '" time="20100128-195406.590832" duration="' + t + '">Average ' + measurements[i] + '</commandline>\n';
              j += 1;
            }
          }
          x = x + '\t\t<result>\n\t\t\t<success passed="yes" state="100" hasTimedOut="false" />\n\t\t</result>\n';
          x = x + '\t</test>\n';
        }
      }
    }
  }

  x = x + '\n</report>\n';
  fs.write(reportName + ".xml", x);

};

exports.reportGenerator = generatePerfReport;