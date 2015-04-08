var fs = require("fs");

var generatePerfReportXML = function (reportName, testdata) {
  'use strict';
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
  'use strict';
  var x = "Thread, Run, Test 1, Start time (ms since Epoch), Test time, Errors, TPS , HTTP response length, HTTP response errors, Time to resolve host, Time to establish connection, Time to first byte, New connections";


  for (var testname in testdata) {
    if (testdata.hasOwnProperty(testname)) {
      for (var testCalculation in testdata[testname]) {
        if (testdata[testname].hasOwnProperty(testCalculation) &&
            (testCalculation !== 'status'))
        {
          var s = testdata[testname][testCalculation].duration;
          if (!isNaN(s)) {
            x = x + '\n0, 0, ' + testname + '/' + testCalculation + ',' + Math.floor(require("internal").time()*1000) + ', ' + Math.floor(s*1000) + ', 0, 0, 0, 0, 0, 0, 0, 0';
          }
        }
      }
    }
  }
  fs.write("out_" + reportName + "_perftest.log", x);

};


var generatePerfReportJTL = function(reportName, testdata) {
  var testFileName = "out_" + reportName + "_perftest.jtl";
  var x = '<?xml version="1.0" encoding="UTF-8"?>\n<testResults version="1.2">\n';

  for (var testname in testdata) {
    if (testdata.hasOwnProperty(testname)) {
      for (var testCalculation in testdata[testname]) {
        if (testdata[testname].hasOwnProperty(testCalculation) &&
            (testCalculation !== 'status'))
        {
          var s = testdata[testname][testCalculation].duration;
          if (!isNaN(s)) {
            x = x + '<httpSample t="' + Math.floor(s*1000) + '" lt="0" ts="' + (require("internal").time() * 1000) + '" s="true" lb="' + testname + '/' + testCalculation + '" rc="200" rm="OK" tn="Thread Group 1-1" dt=""/>\n'
          }
        }
      }
    }
  }
  x = x + '</testResults>';
  fs.write(testFileName, x);
}

exports.reportGeneratorXML = generatePerfReportXML;
exports.generatePerfReportGrinderCSV = generatePerfReportGrinderCSV;
exports.generatePerfReportJTL = generatePerfReportJTL;