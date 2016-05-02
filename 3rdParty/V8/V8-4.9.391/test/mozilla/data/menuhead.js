/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bob Clary <http://bclary.com/>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var gTotalCount = 0;
var gTotalSelected  = 0;

function setCursor(aCursor)
{
  if (document.body && document.body.style)
  {
    document.body.style.cursor = aCursor;
  }
}

function selectAll(suiteName, testDirName)
{
  setCursor('wait');

  suiteName   = suiteName || '';
  testDirName = testDirName || '';

  setTimeout('_selectAll("' + suiteName + '", "' + testDirName + '")', 200);
}

function _selectAll(suiteName, testDirName)
{
  if (!suiteName)
  {
    for (suiteName in suites)
    {
      setAllDirs(suiteName, true);
    }
  }
  else if (!testDirName)
  {
    setAllDirs(suiteName, true);
  }
  else
  {
    setAllTests(suiteName, testDirName, true);
  }

  setCursor('auto');
}

function selectNone(suiteName, testDirName)
{
  setCursor('wait');

  suiteName   = suiteName || '';
  testDirName = testDirName || '';

  setTimeout('_selectNone("' + suiteName + '", "' + testDirName + '")', 200);
}

function _selectNone(suiteName, testDirName)
{
  if (!suiteName)
  {
    for (suiteName in suites)
    {
      setAllDirs(suiteName, false);
    }
  }
  else if (!testDirName)
  {
    setAllDirs(suiteName, false);
  }
  else
  {
    setAllTests(suiteName, testDirName, false);
  }

  setCursor('auto');
}

function updateCounts(suiteName)
{
  var suite = suites[suiteName];
  var pct = parseInt((suite.selected / suite.count) * 100);
  if (isNaN(pct))
    pct = 0;
       
  document.forms.testCases.elements['SUMMARY_' + suiteName].value =
    suite.selected + '/' + suite.count +
    ' (' + pct + '%) selected';

  pct = parseInt((gTotalSelected / gTotalCount) * 100);
  if (isNaN(pct))
    pct = 0;
       
  document.forms.testCases.elements['TOTAL'].value =
    gTotalSelected + '/' + gTotalCount + ' (' +
    pct + '%) selected';
}

function onRadioClick(radio)
{
  var info = radio.id.split(':');
  var suiteName = info[0];
  var suite = suites[suiteName];
  var incr  = (radio.checked ? 1 : -1);
  suite.selected += incr;
  gTotalSelected  += incr;

  updateCounts(suiteName);

  return true;
}
   
function updateTotals()
{
  setCursor('wait');

  setTimeout('_updateTotals()', 200);
}

function _updateTotals()
{
  gTotalCount = 0;
  gTotalSelected  = 0;

  for (var suiteName in suites)
  {
    gTotalCount += suites[suiteName].count;
  }
  for (suiteName in suites)
  {
    updateCounts(suiteName);
  }
  setCursor('auto');
}

function setAllDirs(suiteName, value)
{
  var testDirs = suites[suiteName].testDirs;

  for (var testDirName in testDirs)
  {
    setAllTests(suiteName, testDirName, value);
  }
}

function setAllTests(suiteName, testDirName, value)
{
  var suite = suites[suiteName];
  var tests = suite.testDirs[testDirName].tests;
  var elements = document.forms['testCases'].elements;
  var incr = 0;

  for (var testName in tests)
  {
    var radioName = tests[testName].id;
    var radio     = elements[radioName];
    if (radio.checked && !value)
    {
      incr--;
    }
    else if (!radio.checked && value)
    {
      incr++;
    }
    radio.checked = value;
  }

  suite.selected += incr;
  gTotalSelected += incr;

  updateCounts(suiteName);
}

function createList()
{
  var suiteName;
  var elements = document.forms['testCases'].elements;
  var win      = window.open('about:blank', 'other_window');
  var writer   = new CachedWriter(win.document);

  win.document.open();
  writer.writeln('<pre>');
   
  writer.writeln('# Created ' + new Date());

  for (suiteName in suites)
  {
    writer.writeln('# ' + suiteName + ': ' +
                   elements['SUMMARY_' + suiteName].value);
  }

  writer.writeln('# TOTAL: ' + elements['TOTAL'].value);

  for (suiteName in suites)
  {
    var testDirs = suites[suiteName].testDirs;
    for (var testDirName in testDirs)
    {
      var tests = testDirs[testDirName].tests;
      for (var testName in tests)
      { 
        var radioName = tests[testName].id;
        var radio     = elements[radioName];
        if (radio.checked)
          writer.writeln(suiteName + '/' + testDirName + '/' + radio.value);
      }
    }
  }
  writer.writeln('<\/pre>');
   
  writer.close();
}

var gTests;
var gTestNumber;
var gWindow;

function executeList()
{
  var elements = document.forms['testCases'].elements;

  gTests = [];
  gTestNumber = -1;

  for (var suiteName in suites)
  {
    var testDirs = suites[suiteName].testDirs;
    for (var testDirName in testDirs)
    {
      var tests = testDirs[testDirName].tests;
      for (var testName in tests)
      { 
        var test      = tests[testName];
        var radioName = test.id;
        var radio     = elements[radioName];
        delete test.testCases;
        if (radio.checked)
        {
          gTests[gTests.length] = test;
          test.path = suiteName + '/' + testDirName + '/' + radio.value;
        }
      }
    }
  }
  runNextTest();
}

function runNextTest()
{
  var iTestCase;

  if (gTestNumber != -1)
  {
    // tests have already run in gWindow, collect the results
    // for later reporting.

    var e;
    try
    {
      var test  = gTests[gTestNumber];
      test.testCases = [];
      //test.testCases = test.testCases.concat(gWindow.gTestcases);
      // note MSIE6 has a bug where instead of concating the new arrays
      // it concats them to the first element. work around...
      var origtestcases = gWindow.gTestcases;
      for (iTestCase = 0; iTestCase < origtestcases.length; iTestCase++)
      {
//        test.testCases[test.testCases.length] = origtestcases[iTestCase];
        var origtestcase = origtestcases[iTestCase];
        var testCase = test.testCases[test.testCases.length] = {};
        testCase.name = new String(origtestcase.name);
        testCase.description = new String(origtestcase.description);
        testCase.expect = new String(origtestcase.expect);
        testCase.actual = new String(origtestcase.actual);
        testCase.passed = origtestcase.passed ? true : false;
        testCase.reason = new String(origtestcase.reason);
        testCase.bugnumber = new String(origtestcase.bugnumber?origtestcase.bugnumber:'');
      }
      origtestcases = origtestcase = null;
    }
    catch(e)
    {
      ;
    }
  }

  ++gTestNumber;

  if (gTestNumber < gTests.length)
  {
    // run test
    test      = gTests[gTestNumber];

    gWindow = window.open('js-test-driver-' +
                          document.forms.testCases.doctype.value +
                          '.html?test=' +
                          test.path +
                          ';language=' +
                          document.forms.testCases.language.value,
                          'output');
    if (!gWindow)
    {
      alert('This test suite requires popup windows.\n' +
            'Please enable them for this site.');
    }
  }
  else if (document.forms.testCases.outputformat.value == 'html')
  {
    // all tests completed, display report
    reportHTML();
  }
  else if (document.forms.testCases.outputformat.value == 'javascript')
  {
    // all tests completed, display report
    reportJavaScript();
  }

}

function reportHTML()
{
  var errorsOnly = document.forms.testCases.failures.checked;
  var totalTestCases = 0;
  var totalTestCasesPassed = 0;
  var totalTestCasesFailed = 0;

  gWindow.document.close();

  var writer = new CachedWriter(gWindow.document);

  writer.writeln('<!DOCTYPE HTML PUBLIC ' +
                 '"-//W3C//DTD HTML 4.01 Transitional//EN" ' +
                 '"http://www.w3.org/TR/html4/loose.dtd">');

  writer.writeln('<html>');
  writer.writeln('<head>');
  writer.writeln('<title>JavaScript Tests Browser: ' +
                 navigator.userAgent + ' Language: ' +
                 document.forms.testCases.language.value +
                 '<\/title>');
  writer.writeln('<style type="text/css">');
  writer.writeln(' .passed { } .failed { background-color: red; }');
  writer.writeln('table {}');
  writer.writeln('th.description, th.actual, th.expect { width: 30em;}');
  writer.writeln('<\/style>');

  writer.writeln('<\/head>');
  writer.writeln('<body>');
  writer.writeln('<h2>JavaScript Tests Browser: ' +
                 navigator.userAgent + ' Language: ' +
                 document.forms.testCases.language.value +
                 '<\/h2>');

  writer.writeln('<table border="1">');
  writer.writeln('<thead>');
  writer.writeln('<tr>');
  writer.writeln('<th>Suite<\/th>');
  writer.writeln('<th>Directory<\/th>');
  writer.writeln('<th>Test<\/th>');
  writer.writeln('<th>Passed<\/th>');
  writer.writeln('<th class="description">Description<\/th>');
  writer.writeln('<th class="actual">Actual<\/th>');
  writer.writeln('<th class="expect">Expect<\/th>');
  writer.writeln('<th>BugNumber<\/th>');
  writer.writeln('<th>Reason<\/th>');
  writer.writeln('<\/tr>');
  writer.writeln('<\/thead>');
  writer.writeln('<tbody>');

  writer.writeln('<\/tr>');

  for (var suiteName in suites)
  {
    var suiteTotals = {passed: 0, failed: 0};
    var testDirs = suites[suiteName].testDirs;

    for (var testDirName in testDirs)
    {
      var dirTotals   = {passed: 0, failed: 0};
      var tests = testDirs[testDirName].tests;

      for (var testName in tests)
      {
        var nameTotals  = {passed: 0, failed: 0};
        var test = tests[testName];

        if (! ('testCases' in test) )
        {
          // test not run
          continue;
        }

        var testCases = test.testCases;

        for (var iTestCase = 0; iTestCase < testCases.length; iTestCase++)
        {
          var testCase = testCases[iTestCase];
          ++totalTestCases;
          if (testCase.passed)
          {
            ++totalTestCasesPassed;
            ++nameTotals.passed;
            ++dirTotals.passed;
            ++suiteTotals.passed;
          }
          else
          {
            ++totalTestCasesFailed;
            ++nameTotals.failed;
            ++dirTotals.failed;
            ++suiteTotals.failed;
          }

          if (errorsOnly && testCase.passed)
          {
            continue;
          }

          writer.writeln('<tr class="' +
                         (testCase.passed ? 'passed' : 'failed') + '">' +
                         '<td title="Suite">' + suiteName + '<\/td>' +
                         '<td title="Directory">' + testDirName   + '<\/td>' +
                         '<td title="Test">' + testName + '<\/td>' +
                         '<td title="Status">' +
                         (testCase.passed ? 'Test Passed':'Test Failed') +
                         '<\/td>' +
                         '<td title="Description">' +
                         splitString(testCase.description, '<br>', 30) +
                         '<\/td>' +
                         '<td title="Actual">' +
                         splitString(testCase.actual, '<br>', 30) +
                         '<\/td>' +
                         '<td title="Expected">' +
                         splitString(testCase.expect, '<br>', 30) +
                         '<\/td>' +
                         '<td title="Bug">' +
                         (testCase.bugnumber?testCase.bugnumber:'') +
                         '<\/td>' +
                         '<td title="Reason">' + testCase.reason + '<\/td>' +
                         '<\/tr>');
        } // for iTestCase

        if (!errorsOnly && nameTotals.passed + nameTotals.failed > 0)
        {
          writer.writeln('<tr>' +
                         '<td>' + suiteName + '<\/td>' +
                         '<td>' + testDirName   + '<\/td>' +
                         '<td colspan="7" style="font-weight:bold;">' +
                         testName + ' ' +
                         'Passed = ' + nameTotals.passed.toString() + ' ' +
                         'Failed = ' + nameTotals.failed.toString() +
                         '<\/td>' +
                         '<\/tr>');
        }

      } // for (var testName in tests)

      if (!errorsOnly && dirTotals.passed + dirTotals.failed > 0)
      {
        writer.writeln('<tr>' +
                       '<td>' + suiteName + '<\/td>' +
                       '<td colspan="8" style="font-weight:bold;">' +
                       testDirName  + ' ' +
                       'Passed = ' + dirTotals.passed.toString() + ' ' +
                       'Failed = ' + dirTotals.failed.toString() +
                       '<\/td>' +
                       '<\/tr>');
      }

    } // for (var testDirName in testDirs)

    if (!errorsOnly && suiteTotals.passed + suiteTotals.failed > 0)
    {
      writer.writeln('<tr>' +
                     '<td colspan="9" style="font-weight:bold;">' +
                     suiteName + ' ' +
                     'Passed = ' + suiteTotals.passed.toString() + ' ' +
                     'Failed = ' + suiteTotals.failed.toString() +
                     '<\/td>' +
                     '<\/tr>');
    }

  } // for (var suiteName in suites)

  writer.writeln('<\/tbody>');
  writer.writeln('<\/table>');
   
  writer.writeln('<p>Total TestCases = ' + totalTestCases +
                 ', Passed = ' + totalTestCasesPassed +
                 ', Failed = ' + totalTestCasesFailed +
                 '<\/p>');
  writer.writeln('<\/body>');
  writer.writeln('<\/html>');

  writer.close();
}

// return a string with delim inserted every pos characters
// if there are no spaces to break the line
function splitString(str, delim, pos)
{
  var newstr = '';
  var length = str.length;

  if (str.indexOf(' ') != -1)
  {
    return str;
  }
  for (var i = 0; i < length; i += pos)
  {
    newstr += str.substr(i, pos) + delim;
  }
  return newstr;
}
function reportJavaScript()
{
  gWindow.document.close();

  var errorsOnly = document.forms.testCases.failures.checked;
  var writer = new CachedWriter(gWindow.document);

  writer.writeln('<html>');
  writer.writeln('<head>');
  writer.writeln('<title>JavaScript Tests Browser: ' +
                 navigator.userAgent + ' Language: ' +
                 document.forms.testCases.language.value +
                 '<\/title>');
  writer.writeln('<script type="text/javascript">');
  writer.writeln('var global = window.parent;');
  writer.writeln('if (!global.gUAHash) {');
  writer.writeln('  global.gUAHash = {};');
  writer.writeln('  global.gUAList = {};');
  writer.writeln('  global.gUA_id = -1;');
  writer.writeln('  global.gTestResults = {};');
  writer.writeln('}');
  writer.writeln('var gUA = "' + navigator.userAgent + '";');
  writer.writeln('var gUA_id = global.gUAHash[gUA];');
  writer.writeln('if (typeof(gUA_id) != "number") {');
  writer.writeln('  gUA_id = global.gUAHash[gUA] = ++global.gUA_id;');
  writer.writeln('  global.gUAList[gUA_id] = gUA;');
  writer.writeln('}');
  writer.writeln('var gTestResults = global.gTestResults;');
  writer.writeln('var gLanguage = \'' + document.forms.testCases.language.value + '\';');
  writer.writeln('var gResult;');
  writer.writeln('var gResultId;');
  writer.writeln('var gUAResult;');
  writer.writeln('var gJSResult;');
  writer.writeln('var gSuiteId;');
  writer.writeln('var gSuite;');
  writer.writeln('var gDirId;');
  writer.writeln('var gDir;');
  writer.writeln('var gTestId;');
  writer.writeln('var gTest;');

  writer.writeln('gUAResult = gTestResults[gUA_id];');
  writer.writeln('if (!gUAResult) {');
  writer.writeln('  gUAResult = gTestResults[gUA_id] = {};');
  writer.writeln('}');
  writer.writeln('gJSResult = gUAResult[gLanguage];');
  writer.writeln('if (!gJSResult) {');
  writer.writeln('  gJSResult = gUAResult[gLanguage] = {};');
  writer.writeln('}');


  for (var suiteName in suites)
  {
    var suiteheader = '';

    suiteheader += 'gSuiteId = "' + suiteName + '";\n';
    suiteheader += 'gSuite = gJSResult[gSuiteId];\n';
    suiteheader += 'if (!gSuite) {\n';
    suiteheader += '  gSuite = gJSResult[gSuiteId] = {};\n';
    suiteheader += '}\n';

    var testDirs = suites[suiteName].testDirs;

    for (var testDirName in testDirs)
    {
      var dirheader = '';

      dirheader += 'gDirId = "' + testDirName + '";\n';
      dirheader += 'gDir = gSuite[gDirId];\n';
      dirheader += 'if (!gDir) {\n';
      dirheader += '  gDir = gSuite[gDirId] = {};\n';
      dirheader += '}\n';

      var tests = testDirs[testDirName].tests;

      for (var testName in tests)
      {
        var test = tests[testName];

        if (! ('testCases' in test) )
        {
          // test not run
          continue;
        }

        var testCases = test.testCases;
        var resultId = suiteName + '/' + testDirName + '/' + testName;

        var testheader = '';
        testheader += 'gTestId = "' + testName + '";\n';
        testheader += 'gTest  = gDir[gTestId];\n';
        testheader += 'if (!gTest) {\n';
        testheader += '  gTest = gDir[gTestId] = [];\n';
        testheader += '}\n';

        for (var iTestCase = 0; iTestCase < testCases.length; iTestCase++)
        {
          var testCase = testCases[iTestCase];
          if (errorsOnly && testCase.passed)
          {
            continue;
          }

          if (suiteheader)
          {
            writer.writeln(suiteheader);
            suiteheader = '';
          }
          if (dirheader)
          {
            writer.writeln(dirheader);
            dirheader = '';
          }
          if (testheader)
          {
            writer.writeln(testheader);
            testheader = '';
          }

          var name   = escape(testCase.name);
          var desc   = escape(testCase.description);
          var actual = escape(testCase.actual);
          var expect = escape(testCase.expect);
          var reason = escape(testCase.reason);
         
          var buffer = '';

          buffer += 'gTest[gTest.length] = \n';
          buffer += '{\n';
          buffer += 'name: "' + name + '",\n';
          buffer += 'description: "' + desc +  '",\n';
          buffer += 'expect: "' + expect + '",\n';
          buffer += 'actual: "' + actual + '",\n';
          buffer += 'passed: ' + testCase.passed + ',\n';
          buffer += 'reason: "' + reason + '",\n';
          buffer += 'bugnumber: "' + (testCase.bugnumber ?
                                      testCase.bugnumber : '') + '"\n';
          buffer += '};\n';

          writer.writeln(buffer);
          buffer = '';

        } // for iTestCase

      }  // for testName

    } // for testDirName

  } // for (var suiteName in suites)

  writer.writeln('<\/script>');
  writer.writeln('<\/head>');
  writer.writeln('<body>');
  writer.writeln('<p>JavaScript Tests Browser: ' +
                 navigator.userAgent + ' Language: ' +
                 document.forms.testCases.language.value +
                 '<\/p>');
  writer.writeln('<\/body>')
    writer.writeln('<\/html>');
  writer.close();
}
  
// improve IE's performance. Mozilla's is fine. ;-)
function CachedWriter(doc, limit)
{
  this.document     = doc;
  this.pendingCount = 0;
  this.pendingLimit = limit ? limit : 100;
  this.lines        = [];
}

CachedWriter.prototype.destroy = function _cachedWriterDestroy()
{
  this.document     = null;
  this.pendingCount = 0;
  this.pendingLimit = 0;
  this.lines        = null;
}

  CachedWriter.prototype.writeln = function _cachedWriterWriteLine(s)
{
  this.lines[this.lines.length] = s;
  if (++this.pendingCount >= this.pendingLimit)
  {
    this.flush();
  }
}

CachedWriter.prototype.flush = function _cachedWriterFlush()
{
  var s = this.lines.join('\n');
  this.pendingCount = 0;
  this.lines = [];
  if (!window.opera)
  {
    for (var ibadchar = 0; ibadchar < 32; ++ibadchar)
    {
      if (ibadchar != 10 && ibadchar != 13)
      {
        var badchar = String.fromCharCode(ibadchar);
        var badregx = new RegExp(badchar, 'mg');
        var escchar = '\\0x' + ibadchar.toString(16);
        s = s.replace(badregx, escchar);
      }
    }
  }
  this.document.writeln(s);
}

  CachedWriter.prototype.close = function _cachedWriterClose()
{
  this.flush();
  this.document.close();
  this.destroy();
}
