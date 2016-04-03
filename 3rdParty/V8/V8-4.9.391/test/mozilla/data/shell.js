/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rob Ginda rginda@netscape.com
 *   Bob Clary bob@bclary.com
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

// Spidermonkey shell now defaults to 1.8, so set the basic version to
// 1.5 for backwards compatibility.

if (typeof version != 'undefined')
{
  version(150);
}

var STATUS = "STATUS: ";
var VERBOSE = false;
var SECT_PREFIX = 'Section ';
var SECT_SUFFIX = ' of test - ';
var callStack = new Array();

var gTestfile;
var gTestPath;
var gTestsuite;
var gTestsubsuite;
var gDelayTestDriverEnd = false;

var gTestcases = new Array();
var gTc = gTestcases.length;
var BUGNUMBER = '';
var summary = '';
var description = '';
var expected = '';
var actual = '';
var msg = '';

var SECTION = "";
var VERSION = "";
var BUGNUMBER = "";

/*
 * constant strings
 */
var GLOBAL = this + '';
var PASSED = " PASSED! ";
var FAILED = " FAILED! ";

var DEBUG = false;

var DESCRIPTION;
var EXPECTED;

/*
 * wrapper for test case constructor that doesn't require the SECTION
 * argument.
 */

function AddTestCase( description, expect, actual ) {
  new TestCase( SECTION, description, expect, actual );
}

/*
 * Set up test environment.
 *
 */
function startTest() {
  // print out bugnumber

  if ( BUGNUMBER ) {
    print ("BUGNUMBER: " + BUGNUMBER );
  }
  if ( typeof version != 'function') {
    return;
  }

  // JavaScript 1.3 is supposed to be compliant ecma version 1.0
  if ( VERSION == "ECMA_1" ) {
    version ( "130" );
  }
  else if ( VERSION == "JS_1.8"  || gTestsuite == 'js1_8') {
    version ( "180" );
  }
  else if ( VERSION == "JS_1.7"  || gTestsuite == 'js1_7') {
    version ( "170" );
  }
  else if ( VERSION == "JS_1.6"  || gTestsuite == 'js1_6') {
    version ( "160" );
  }
  else if ( VERSION == "JS_1.5"  || gTestsuite == 'js1_5') {
    version ( "150" );
  }
  else if ( VERSION == "JS_1.4"  || gTestsuite == 'js1_4') {
    version ( "140" );
  }
  else if ( VERSION == "JS_1.3"  || gTestsuite == 'js1_3') {
    version ( "130" );
  }
  else if ( VERSION == "JS_1.2"  || gTestsuite == 'js1_2') {
    version ( "120" );
  }
  else if ( VERSION  == "JS_1.1" || gTestsuite == 'js1_1') {
    version ( "110" );
  }
}

function TestCase(n, d, e, a)
{
  this.path = (typeof gTestPath == 'undefined') ?
    (gTestsuite + '/' + gTestsubsuite + '/' + gTestfile) :
    gTestPath;
  this.file = gTestfile;
  this.name = n;
  this.description = d;
  this.expect = e;
  this.actual = a;
  this.passed = getTestCaseResult(e, a);
  this.reason = '';
  this.bugnumber = typeof(BUGNUMER) != 'undefined' ? BUGNUMBER : '';
  this.type = (typeof window == 'undefined' ? 'shell' : 'browser');
  gTestcases[gTc++] = this;
}

TestCase.prototype.dump = function () {
  dump('\njstest: '      + this.path + ' ' +
       'bug: '         + this.bugnumber + ' ' +
       'result: '      + (this.passed ? 'PASSED':'FAILED') + ' ' +
       'type: '        + this.type + ' ' +
       'description: ' + toPrinted(this.description) + ' ' +
//       'expected: '    + toPrinted(this.expect) + ' ' +
//       'actual: '      + toPrinted(this.actual) + ' ' +
       'reason: '      + toPrinted(this.reason) + '\n');
};

/*
 * The test driver searches for such a phrase in the test output.
 * If such phrase exists, it will set n as the expected exit code.
 */
function expectExitCode(n)
{
  print('--- NOTE: IN THIS TESTCASE, WE EXPECT EXIT CODE ' + n + ' ---');
}

/*
 * Statuses current section of a test
 */
function inSection(x)
{
  return SECT_PREFIX + x + SECT_SUFFIX;
}

/*
 * Report a failure in the 'accepted' manner
 */
function reportFailure (msg)
{
  var lines = msg.split ("\n");
  var l;
  var funcName = currentFunc();
  var prefix = (funcName) ? "[reported from " + funcName + "] ": "";
   
  for (var i=0; i<lines.length; i++)
    print (FAILED + prefix + lines[i]);
}

/*
 * Print a non-failure message.
 */
function printStatus (msg)
{
/* js1_6 had...
   msg = String(msg);
   msg = msg.toString();
*/
  msg = msg.toString();
  var lines = msg.split ("\n");
  var l;

  for (var i=0; i<lines.length; i++)
    print (STATUS + lines[i]);
}

/*
 * Print a bugnumber message.
 */
function printBugNumber (num)
{
  BUGNUMBER = num;
  print ('BUGNUMBER: ' + num);
}

function toPrinted(value)
{
  if (typeof value == "xml") 
  {
    value = value.toXMLString();
  } 
  else 
  {
    value = String(value);
  }
  value = value.replace(/\\n/g, 'NL')
               .replace(/\n/g, 'NL')
               .replace(/\\r/g, 'CR')
               .replace(/[^\x20-\x7E]+/g, escapeString);
  return value;
}

function escapeString (str)
{
  var a, b, c, d;
  var len = str.length;
  var result = "";
  var digits = ["0", "1", "2", "3", "4", "5", "6", "7",
                "8", "9", "A", "B", "C", "D", "E", "F"];

  for (var i=0; i<len; i++)
  {
    var ch = str.charCodeAt(i);

    a = digits[ch & 0xf];
    ch >>= 4;
    b = digits[ch & 0xf];
    ch >>= 4;

    if (ch)
    {
      c = digits[ch & 0xf];
      ch >>= 4;
      d = digits[ch & 0xf];

      result += "\\u" + d + c + b + a;
    }
    else
    {
      result += "\\x" + b + a;
    }
  }

  return result;
}

/*
 * assertEq(actual, expected)
 *           Throw if the two arguments are not ===
 * see https://bugzilla.mozilla.org/show_bug.cgi?id=480199
 */
if (typeof assertEq == 'undefined')
{
  var assertEq =
    function (actual, expected)
    {
      if (actual !== expected)
      {
        throw new TypeError('Assertion failed: got "' + actual + '", expected "' + expected);
      }
    };
}

/*
 * Compare expected result to actual result, if they differ (in value and/or
 * type) report a failure.  If description is provided, include it in the
 * failure report.
 */
function reportCompare (expected, actual, description) {
  var expected_t = typeof expected;
  var actual_t = typeof actual;
  var output = "";

  if (typeof description == "undefined")
  {
    description = '';
  }
  else if (VERBOSE)
  {
    printStatus ("Comparing '" + description + "'");
  }

  if (expected_t != actual_t)
  {
    output += "Type mismatch, expected type " + expected_t +
      ", actual type " + actual_t + " ";
  }
  else if (VERBOSE)
  {
    printStatus ("Expected type '" + expected_t + "' matched actual " +
                 "type '" + actual_t + "'");
  }

  if (expected != actual)
  {
    output += "Expected value '" + toPrinted(expected) +
      "', Actual value '" + toPrinted(actual) + "' ";
  }
  else if (VERBOSE)
  {
    printStatus ("Expected value '" + toPrinted(expected) +
                 "' matched actual value '" + toPrinted(actual) + "'");
  }

  var testcase = new TestCase(gTestfile, description, expected, actual);
  testcase.reason = output;

  if (testcase.passed)
  {
    print(PASSED + description);
  }
  else
  {
    reportFailure (description + " : " + output);
  }

  return testcase.passed;
}

/*
 * Attempt to match a regular expression describing the result to
 * the actual result, if they differ (in value and/or
 * type) report a failure.  If description is provided, include it in the
 * failure report.
 */
function reportMatch (expectedRegExp, actual, description) {
  var expected_t = "string";
  var actual_t = typeof actual;
  var output = "";

  if (typeof description == "undefined")
  {
    description = '';
  }
  else if (VERBOSE)
  {
    printStatus ("Comparing '" + description + "'");
  }

  if (expected_t != actual_t)
  {
    output += "Type mismatch, expected type " + expected_t +
      ", actual type " + actual_t + " ";
  }
  else if (VERBOSE)
  {
    printStatus ("Expected type '" + expected_t + "' matched actual " +
                 "type '" + actual_t + "'");
  }

  var matches = expectedRegExp.test(actual);
  if (!matches)
  {
    output += "Expected match to '" + toPrinted(expectedRegExp) +
      "', Actual value '" + toPrinted(actual) + "' ";
  }
  else if (VERBOSE)
  {
    printStatus ("Expected match to '" + toPrinted(expectedRegExp) +
                 "' matched actual value '" + toPrinted(actual) + "'");
  }

  var testcase = new TestCase(gTestfile, description, true, matches);
  testcase.reason = output;

  if (testcase.passed)
  {
    print(PASSED + description);
  }
  else
  {
    reportFailure (description + " : " + output);
  }

  return testcase.passed;
}

/*
 * Puts funcName at the top of the call stack.  This stack is used to show
 * a function-reported-from field when reporting failures.
 */
function enterFunc (funcName)
{
  if (!funcName.match(/\(\)$/))
    funcName += "()";

  callStack.push(funcName);
}

/*
 * Pops the top funcName off the call stack.  funcName is optional, and can be
 * used to check push-pop balance.
 */
function exitFunc (funcName)
{
  var lastFunc = callStack.pop();
   
  if (funcName)
  {
    if (!funcName.match(/\(\)$/))
      funcName += "()";

    if (lastFunc != funcName)
      reportCompare(funcName, lastFunc, "Test driver failure wrong exit function ");
  }
}

/*
 * Peeks at the top of the call stack.
 */
function currentFunc()
{
  return callStack[callStack.length - 1];
}

/*
  Calculate the "order" of a set of data points {X: [], Y: []}
  by computing successive "derivatives" of the data until
  the data is exhausted or the derivative is linear.
*/
function BigO(data)
{
  var order = 0;
  var origLength = data.X.length;

  while (data.X.length > 2)
  {
    var lr = new LinearRegression(data);
    if (lr.b > 1e-6)
    {
      // only increase the order if the slope
      // is "great" enough
      order++;
    }

    if (lr.r > 0.98 || lr.Syx < 1 || lr.b < 1e-6)
    {
      // terminate if close to a line lr.r
      // small error lr.Syx
      // small slope lr.b
      break;
    }
    data = dataDeriv(data);
  }

  if (2 == origLength - order)
  {
    order = Number.POSITIVE_INFINITY;
  }
  return order;

  function LinearRegression(data)
  {
    /*
      y = a + bx
      for data points (Xi, Yi); 0 <= i < n

      b = (n*SUM(XiYi) - SUM(Xi)*SUM(Yi))/(n*SUM(Xi*Xi) - SUM(Xi)*SUM(Xi))
      a = (SUM(Yi) - b*SUM(Xi))/n
    */
    var i;

    if (data.X.length != data.Y.length)
    {
      throw 'LinearRegression: data point length mismatch';
    }
    if (data.X.length < 3)
    {
      throw 'LinearRegression: data point length < 2';
    }
    var n = data.X.length;
    var X = data.X;
    var Y = data.Y;

    this.Xavg = 0;
    this.Yavg = 0;

    var SUM_X  = 0;
    var SUM_XY = 0;
    var SUM_XX = 0;
    var SUM_Y  = 0;
    var SUM_YY = 0;

    for (i = 0; i < n; i++)
    {
      SUM_X  += X[i];
      SUM_XY += X[i]*Y[i];
      SUM_XX += X[i]*X[i];
      SUM_Y  += Y[i];
      SUM_YY += Y[i]*Y[i];
    }

    this.b = (n * SUM_XY - SUM_X * SUM_Y)/(n * SUM_XX - SUM_X * SUM_X);
    this.a = (SUM_Y - this.b * SUM_X)/n;

    this.Xavg = SUM_X/n;
    this.Yavg = SUM_Y/n;

    var SUM_Ydiff2 = 0;
    var SUM_Xdiff2 = 0;
    var SUM_XdiffYdiff = 0;

    for (i = 0; i < n; i++)
    {
      var Ydiff = Y[i] - this.Yavg;
      var Xdiff = X[i] - this.Xavg;
       
      SUM_Ydiff2 += Ydiff * Ydiff;
      SUM_Xdiff2 += Xdiff * Xdiff;
      SUM_XdiffYdiff += Xdiff * Ydiff;
    }

    var Syx2 = (SUM_Ydiff2 - Math.pow(SUM_XdiffYdiff/SUM_Xdiff2, 2))/(n - 2);
    var r2   = Math.pow((n*SUM_XY - SUM_X * SUM_Y), 2) /
      ((n*SUM_XX - SUM_X*SUM_X)*(n*SUM_YY-SUM_Y*SUM_Y));

    this.Syx = Math.sqrt(Syx2);
    this.r = Math.sqrt(r2);

  }

  function dataDeriv(data)
  {
    if (data.X.length != data.Y.length)
    {
      throw 'length mismatch';
    }
    var length = data.X.length;

    if (length < 2)
    {
      throw 'length ' + length + ' must be >= 2';
    }
    var X = data.X;
    var Y = data.Y;

    var deriv = {X: [], Y: [] };

    for (var i = 0; i < length - 1; i++)
    {
      deriv.X[i] = (X[i] + X[i+1])/2;
      deriv.Y[i] = (Y[i+1] - Y[i])/(X[i+1] - X[i]);
    } 
    return deriv;
  }

  return 0;
}

function compareSource(expect, actual, summary)
{
  // compare source
  var expectP = expect.
    replace(/([(){},.:\[\]])/mg, ' $1 ').
    replace(/(\w+)/mg, ' $1 ').
    replace(/<(\/)? (\w+) (\/)?>/mg, '<$1$2$3>').
    replace(/\s+/mg, ' ').
    replace(/new (\w+)\s*\(\s*\)/mg, 'new $1');

  var actualP = actual.
    replace(/([(){},.:\[\]])/mg, ' $1 ').
    replace(/(\w+)/mg, ' $1 ').
    replace(/<(\/)? (\w+) (\/)?>/mg, '<$1$2$3>').
    replace(/\s+/mg, ' ').
    replace(/new (\w+)\s*\(\s*\)/mg, 'new $1');

  print('expect:\n' + expectP);
  print('actual:\n' + actualP);

  reportCompare(expectP, actualP, summary);

  // actual must be compilable if expect is?
  try
  {
    var expectCompile = 'No Error';
    var actualCompile;

    eval(expect);
    try
    {
      eval(actual);
      actualCompile = 'No Error';
    }
    catch(ex1)
    {
      actualCompile = ex1 + '';
    }
    reportCompare(expectCompile, actualCompile,
                  summary + ': compile actual');
  }
  catch(ex)
  {
  }
}

function optionsInit() {

  // record initial values to support resetting
  // options to their initial values
  options.initvalues  = {};

  // record values in a stack to support pushing
  // and popping options
  options.stackvalues = [];

  var optionNames = options().split(',');

  for (var i = 0; i < optionNames.length; i++)
  {
    var optionName = optionNames[i];
    if (optionName)
    {
      options.initvalues[optionName] = '';
    }
  }
}

function optionsClear() {
       
  // turn off current settings
  var optionNames = options().split(',');
  for (var i = 0; i < optionNames.length; i++)
  {
    var optionName = optionNames[i];
    if (optionName)
    {
      options(optionName);
    }
  }
}

function optionsPush()
{
  var optionsframe = {};

  options.stackvalues.push(optionsframe);

  var optionNames = options().split(',');

  for (var i = 0; i < optionNames.length; i++)
  {
    var optionName = optionNames[i];
    if (optionName)
    {
      optionsframe[optionName] = '';
    }
  }

  optionsClear();
}

function optionsPop()
{
  var optionsframe = options.stackvalues.pop();

  optionsClear();

  for (optionName in optionsframe)
  {
    options(optionName);
  }

}

function optionsReset() {

  try
  {
    optionsClear();

    // turn on initial settings
    for (optionName in options.initvalues)
    {
      options(optionName);
    }
  }
  catch(ex)
  {
    print('optionsReset: caught ' + ex);
  }

}

if (typeof options == 'function')
{
  optionsInit();
  optionsClear();
}

function getTestCaseResult(expected, actual)
{
  if (typeof expected != typeof actual)
    return false;
  if (typeof expected != 'number')
    // Note that many tests depend on the use of '==' here, not '==='.
    return actual == expected;

  // Distinguish NaN from other values.  Using x != x comparisons here
  // works even if tests redefine isNaN.
  if (actual != actual)
    return expected != expected;
  if (expected != expected)
    return false;

  // Tolerate a certain degree of error.
  if (actual != expected)
    return Math.abs(actual - expected) <= 1E-10;

  // Here would be a good place to distinguish 0 and -0, if we wanted
  // to.  However, doing so would introduce a number of failures in
  // areas where they don't seem important.  For example, the WeekDay
  // function in ECMA-262 returns -0 for Sundays before the epoch, but
  // the Date functions in SpiderMonkey specified in terms of WeekDay
  // often don't.  This seems unimportant.
  return true;
}

if (typeof dump == 'undefined')
{
  if (typeof window == 'undefined' &&
      typeof print == 'function')
  {
    dump = print;
  }
  else
  {
    dump = (function () {});
  }
}

function test() {
  for ( gTc=0; gTc < gTestcases.length; gTc++ ) {
    // temporary hack to work around some unknown issue in 1.7
    try
    {
      gTestcases[gTc].passed = writeTestCaseResult(
        gTestcases[gTc].expect,
        gTestcases[gTc].actual,
        gTestcases[gTc].description +" = "+ gTestcases[gTc].actual );
      gTestcases[gTc].reason += ( gTestcases[gTc].passed ) ? "" : "wrong value ";
    }
    catch(e)
    {
      print('test(): empty testcase for gTc = ' + gTc + ' ' + e);
    }
  }
  stopTest();
  return ( gTestcases );
}

/*
 * Begin printing functions.  These functions use the shell's
 * print function.  When running tests in the browser, these
 * functions, override these functions with functions that use
 * document.write.
 */

function writeTestCaseResult( expect, actual, string ) {
  var passed = getTestCaseResult( expect, actual );
  writeFormattedResult( expect, actual, string, passed );
  return passed;
}
function writeFormattedResult( expect, actual, string, passed ) {
  var s = ( passed ? PASSED : FAILED ) + string + ' expected: ' + expect;
  print(s);
  return passed;
}

function writeHeaderToLog( string ) {
  print( string );
}
/* end of print functions */


/*
 * When running in the shell, run the garbage collector after the
 * test has completed.
 */

function stopTest() {
  var gc;
  if ( gc != undefined ) {
    gc();
  }
}

/*
 * Convenience function for displaying failed test cases.  Useful
 * when running tests manually.
 *
 */
function getFailedCases() {
  for ( var i = 0; i < gTestcases.length; i++ ) {
    if ( ! gTestcases[i].passed ) {
      print( gTestcases[i].description + " = " +gTestcases[i].actual +
             " expected: " + gTestcases[i].expect );
    }
  }
}

function jsTestDriverEnd()
{
  // gDelayTestDriverEnd is used to
  // delay collection of the test result and
  // signal to Spider so that tests can continue
  // to run after page load has fired. They are
  // responsible for setting gDelayTestDriverEnd = true
  // then when completed, setting gDelayTestDriverEnd = false
  // then calling jsTestDriverEnd()

  if (gDelayTestDriverEnd)
  {
    return;
  }

  try
  {
    optionsReset();
  }
  catch(ex)
  {
    dump('jsTestDriverEnd ' + ex);
  }

  for (var i = 0; i < gTestcases.length; i++)
  {
    gTestcases[i].dump();
  }

}

function jit(on)
{
  if (on && !options().match(/jit/))
  {
    options('jit');
  }
  else if (!on && options().match(/jit/))
  {
    options('jit');
  }
}

/*
 * Some tests need to know if we are in Rhino as opposed to SpiderMonkey
 */
function inRhino()
{
  return (typeof defineClass == "function");
}

/* these functions are useful for running tests manually in Rhino */

function GetContext() {
  return Packages.com.netscape.javascript.Context.getCurrentContext();
}
function OptLevel( i ) {
  i = Number(i);
  var cx = GetContext();
  cx.setOptimizationLevel(i);
}
/* end of Rhino functions */


