/*global require, exports
 */
var fs = require("fs");

var generatePerfReportXML = function (reportName, testdata) {
  "use strict";
  var x = '<?xml version="1.0" encoding="UTF-8"?>\n' +
    '<report name="' + reportName + '" categ="' + reportName + '">\n' + 
    '<start> <date format="YYYYMMDD" val="20000101" /> <time format="HHMMSS" val="195043" /> </start>\n'; //// TODO: zeit einsetzen.


  for (var testname in testdata) {
    if (testdata.hasOwnProperty(testname)) {
      for (var testCalculation in testdata[testname]) {
        if (testdata[testname].hasOwnProperty(testCalculation) &&
            (testCalculation !== 'status'))
        {
          var s = testdata[testname][testCalculation].duration;
          x = x + '\t<test name="/' + testname + '/' + testCalculation + '" executed="yes">\n';
          x = x + '\t\t<description><![CDATA[This is the description of the test ' + testname + ' executed by the testrunner ' + testCalculation  + ']]></description>\n';
          x = x + '\t\t<targets>\n\t\t\t<target threaded="false">AQL</target>\n\t\t</targets>\n';
          
          x += '\t\t<commandline rank="0" time="20100128-195406.590832" duration="' + s + '">Sum time</commandline>\n';
          
          x = x + '\t\t<result>\n';
          x += '\t\t\t<success passed="yes" state="100" hasTimedOut="false" />\n';
          x += '\t\t\t<compiletime unit="s" mesure="' + s + '" isRelevant="true" />\n';
          x += '\t\t\t<performance unit="s" mesure="' + s + '" isRelevant="true" />\n';
          x += '\t\t\t<executiontime unit="s" mesure="'+ s + '" isRelevant="true" />\n';
          
          x += '\t\t</result>\n';
          x = x + '\t</test>\n';
        }
      }
    }
  }

  x = x + '\n</report>\n';
  fs.write(reportName + ".xml", x);

};

var generatePerfReportGrinderCSV = function (reportName, testdata) {
  "use strict";
  var x = "Thread, Run, Test, Start time (ms since Epoch), Test time, Errors, HTTP response code, HTTP response length, HTTP response errors, Time to resolve host, Time to establish connection, Time to first byte, New connections\n";


  for (var testname in testdata) {
    if (testdata.hasOwnProperty(testname)) {
      for (var testCalculation in testdata[testname]) {
        if (testdata[testname].hasOwnProperty(testCalculation) &&
            (testCalculation !== 'status'))
        {
          var s = testdata[testname][testCalculation].duration;
          if (!isNaN(s)) {
            x = x + '0, 0, ' + testname + '/' + testCalculation + ',' + require("internal").time() + ', ' + s + ', 0, 0, 0, 0, 0, 0, 0, 0\n';
          }
        }
      }
    }
  }
  fs.write("out_" + reportName + "_perftest.log", x);

};


exports.reportGeneratorXML = generatePerfReportXML;
exports.generatePerfReportGrinderCSV = generatePerfReportGrinderCSV;