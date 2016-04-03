/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is JavaScript Engine testing utilities.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Mike Shaver
 *                 Brendan Eich
 *                 Andreas Gal
 *                 David Anderson
 *                 Boris Zbarsky
 *                 Brian Crowder
 *                 Blake Kaplan
 *                 Robert Sayre
 *                 Vladimir Vukicevic
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

var gTestfile = 'math-trace-tests.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 'none';
var summary = 'trace-capability math mini-testsuite';

printBugNumber(BUGNUMBER);
printStatus (summary);

jit(true);

/**
 * A number of the tests in this file depend on the setting of
 * HOTLOOP.  Define some constants up front, so they're easy to grep
 * for.
 */
// The HOTLOOP constant we depend on; only readable from our stats
// object in debug builds.
const haveTracemonkey = !!(this.tracemonkey)
  const HOTLOOP = haveTracemonkey ? tracemonkey.HOTLOOP : 2;
// The loop count at which we trace
const RECORDLOOP = HOTLOOP;
// The loop count at which we run the trace
const RUNLOOP = HOTLOOP + 1;

var testName = null;
if ("arguments" in this && arguments.length > 0)
  testName = arguments[0];
var fails = [], passes=[];

function jitstatHandler(f)
{
  if (!haveTracemonkey) 
    return;

  // XXXbz this is a nasty hack, but I can't figure out a way to
  // just use jitstats.tbl here
  f("recorderStarted");
  f("recorderAborted");
  f("traceCompleted");
  f("sideExitIntoInterpreter");
  f("typeMapMismatchAtEntry");
  f("returnToDifferentLoopHeader");
  f("traceTriggered");
  f("globalShapeMismatchAtEntry");
  f("treesTrashed");
  f("slotPromoted");
  f("unstableLoopVariable");
  f("noCompatInnerTrees");
  f("breakLoopExits");
  f("returnLoopExits");
}

function test(f)
{
  if (!testName || testName == f.name) {
    // Collect our jit stats
    var localJITstats = {};
    jitstatHandler(function(prop, local, global) {
        localJITstats[prop] = tracemonkey[prop];
      });
    check(f.name, f(), f.expected, localJITstats, f.jitstats);
  }
}

function map_test(t, cases)
{
  for (var i = 0; i < cases.length; i++) {
    function c() { return t(cases[i].input); }
    c.expected = cases[i].expected;
    c.name = t.name + "(" + uneval(cases[i].input) + ")";
    test(c);
  }
}

// Use this function to compare expected and actual test results.
// Types must match.
// For numbers, treat NaN as matching NaN, distinguish 0 and -0, and
// tolerate a certain degree of error for other values.
//
// These are the same criteria used by the tests in js/tests, except that
// we distinguish 0 and -0.
function close_enough(expected, actual)
{
  if (typeof expected != typeof actual)
    return false;
  if (typeof expected != 'number')
    return actual == expected;

  // Distinguish NaN from other values.  Using x != x comparisons here
  // works even if tests redefine isNaN.
  if (actual != actual)
    return expected != expected
      if (expected != expected)
        return false;

  // Tolerate a certain degree of error.
  if (actual != expected)
    return Math.abs(actual - expected) <= 1E-10;

  // Distinguish 0 and -0.
  if (actual == 0)
    return (1 / actual > 0) == (1 / expected > 0);

  return true;
}

function check(desc, actual, expected, oldJITstats, expectedJITstats)
{
  var pass = false;
  if (close_enough(expected, actual)) {
    pass = true;
    jitstatHandler(function(prop) {
        if (expectedJITstats && prop in expectedJITstats &&
            expectedJITstats[prop] !=
            tracemonkey[prop] - oldJITstats[prop]) {
          pass = false;
        }
      });
    if (pass) {
      reportCompare(expected, actual, desc);
      passes.push(desc);
      return print(desc, ": passed");
    }
  }

  if (expected instanceof RegExp) {
    pass = reportMatch(expected, actual + '', desc);
    if (pass) {
      jitstatHandler(function(prop) {
          if (expectedJITstats && prop in expectedJITstats &&
              expectedJITstats[prop] !=
              tracemonkey[prop] - oldJITstats[prop]) {
            pass = false;
          }
        });
    }
    if (pass) {
      passes.push(desc);
      return print(desc, ": passed");
    }
  }

  reportCompare(expected, actual, desc);

  fails.push(desc);
  var expectedStats = "";
  if (expectedJITstats) {
    jitstatHandler(function(prop) {
        if (prop in expectedJITstats) {
          if (expectedStats)
            expectedStats += " ";
          expectedStats +=
            prop + ": " + expectedJITstats[prop];
        }
      });
  }
  var actualStats = "";
  if (expectedJITstats) {
    jitstatHandler(function(prop) {
        if (prop in expectedJITstats) {
          if (actualStats)
            actualStats += " ";
          actualStats += prop + ": " + (tracemonkey[prop]-oldJITstats[prop]);
        }
      });
  }
  print(desc, ": FAILED: expected", typeof(expected), 
        "(", uneval(expected), ")",
        (expectedStats ? " [" + expectedStats + "] " : ""),
        "!= actual",
        typeof(actual), "(", uneval(actual), ")",
        (actualStats ? " [" + actualStats + "] " : ""));
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

// Apply FUNCNAME to ARGS, and check against EXPECTED.
// Expect a loop containing such a call to be traced.
// FUNCNAME and ARGS are both strings.
// ARGS has the form of an argument list: a comma-separated list of expressions.
// Certain Tracemonkey limitations require us to pass FUNCNAME as a string.
// Passing ARGS as a string allows us to assign better test names:
// expressions like Math.PI/4 haven't been evaluated to big hairy numbers.
function testmath(funcname, args, expected) {
    var i, j;

    var arg_value_list = eval("[" + args + "]");
    var arity = arg_value_list.length;

    // Build the string "a[i][0],...,a[i][ARITY-1]".
    var actuals = []
    for (i = 0; i < arity; i++)
        actuals.push("a[i][" + i + "]");
    actuals = actuals.join(",");

    // Create a function that maps FUNCNAME across an array of input values.
    // Unless we eval here, the call to funcname won't get traced.
    // FUNCNAME="Infinity/Math.abs" and cases like that happen to
    // parse, too, in a twisted way.
    var mapfunc = eval("(function(a) {\n"
                       + "   for (var i = 0; i < a.length; i++)\n"
                       + "       a[i] = " + funcname + "(" + actuals +");\n"
                       + " })\n");

    // To prevent the compiler from doing constant folding, produce an
    // array to pass to mapfunc that contains enough dummy
    // values at the front to get the loop body jitted, and then our
    // actual test value.
    var dummies_and_input = [];
    for (i = 0; i < RUNLOOP; i++) {
        var dummy_list = [];
        for (j = 0; j < arity; j++)
            dummy_list[j] = .0078125 * ((i + j) % 128);
        dummies_and_input[i] = dummy_list;
    }
    dummies_and_input[RUNLOOP] = arg_value_list;

    function testfunc() {
        // Map the function across the dummy values and the test input.
        mapfunc(dummies_and_input);
        return dummies_and_input[RUNLOOP];
    }
    testfunc.name = funcname + "(" + args + ")";
    testfunc.expected = expected;

    // Disable jitstats check. This never worked right. The actual part of the
    // loop we cared about was never traced. We traced the filler parts early
    // and then took a mismatch side exit on every subequent array read with
    // a different type (gal, discovered when fixing bug 479110).
    // testfunc.jitstats = {
    //   recorderStarted: 1,
    //   recorderAborted: 0,
    //   traceTriggered: 1
    // };

    test(testfunc);
}

testmath("Math.abs", "void 0", Number.NaN)
testmath("Math.abs", "null", 0)
testmath("Math.abs", "true", 1)
testmath("Math.abs", "false", 0)
testmath("Math.abs", "\"a string primitive\"", Number.NaN)
testmath("Math.abs", "new String( 'a String object' )", Number.NaN)
testmath("Math.abs", "Number.NaN", Number.NaN)
testmath("Math.abs", "0", 0)
testmath("Math.abs", "-0", 0)
testmath("Infinity/Math.abs", "-0", Infinity)
testmath("Math.abs", "Number.NEGATIVE_INFINITY", Number.POSITIVE_INFINITY)
testmath("Math.abs", "Number.POSITIVE_INFINITY", Number.POSITIVE_INFINITY)
testmath("Math.abs", "- Number.MAX_VALUE", Number.MAX_VALUE)
testmath("Math.abs", "-Number.MIN_VALUE", Number.MIN_VALUE)
testmath("Math.abs", "Number.MAX_VALUE", Number.MAX_VALUE)
testmath("Math.abs", "Number.MIN_VALUE", Number.MIN_VALUE)
testmath("Math.abs", "-1", 1)
testmath("Math.abs", "new Number(-1)", 1)
testmath("Math.abs", "1", 1)
testmath("Math.abs", "Math.PI", Math.PI)
testmath("Math.abs", "-Math.PI", Math.PI)
testmath("Math.abs", "-1/100000000", 1/100000000)
testmath("Math.abs", "-Math.pow(2,32)", Math.pow(2,32))
testmath("Math.abs", "Math.pow(2,32)", Math.pow(2,32))
testmath("Math.abs", "-0xfff", 4095)
testmath("Math.abs", "-0777", 511)
testmath("Math.abs", "'-1e-1'", 0.1)
testmath("Math.abs", "'0xff'", 255)
testmath("Math.abs", "'077'", 77)
testmath("Math.abs", "'Infinity'", Infinity)
testmath("Math.abs", "'-Infinity'", Infinity)

testmath("Math.acos", "void 0", Number.NaN)
testmath("Math.acos", "null", Math.PI/2)
testmath("Math.acos", "Number.NaN", Number.NaN)
testmath("Math.acos", "\"a string\"", Number.NaN)
testmath("Math.acos", "'0'", Math.PI/2)
testmath("Math.acos", "'1'", 0)
testmath("Math.acos", "'-1'", Math.PI)
testmath("Math.acos", "1.00000001", Number.NaN)
testmath("Math.acos", "-1.00000001", Number.NaN)
testmath("Math.acos", "1", 0)
testmath("Math.acos", "-1", Math.PI)
testmath("Math.acos", "0", Math.PI/2)
testmath("Math.acos", "-0", Math.PI/2)
testmath("Math.acos", "Math.SQRT1_2", Math.PI/4)
testmath("Math.acos", "-Math.SQRT1_2", Math.PI/4*3)
testmath("Math.acos", "0.9999619230642", Math.PI/360)
testmath("Math.acos", "-3.0", Number.NaN)

testmath("Math.asin", "void 0", Number.NaN)
testmath("Math.asin", "null", 0)
testmath("Math.asin", "Number.NaN", Number.NaN)
testmath("Math.asin", "\"string\"", Number.NaN)
testmath("Math.asin", "\"0\"", 0)
testmath("Math.asin", "\"1\"", Math.PI/2)
testmath("Math.asin", "\"-1\"", -Math.PI/2)
testmath("Math.asin", "Math.SQRT1_2+''", Math.PI/4)
testmath("Math.asin", "-Math.SQRT1_2+''", -Math.PI/4)
testmath("Math.asin", "1.000001", Number.NaN)
testmath("Math.asin", "-1.000001", Number.NaN)
testmath("Math.asin", "0", 0)
testmath("Math.asin", "-0", -0)
testmath("Infinity/Math.asin", "-0", -Infinity)
testmath("Math.asin", "1", Math.PI/2)
testmath("Math.asin", "-1", -Math.PI/2)
testmath("Math.asin", "Math.SQRT1_2", Math.PI/4)
testmath("Math.asin", "-Math.SQRT1_2", -Math.PI/4)

testmath("Math.atan", "void 0", Number.NaN)
testmath("Math.atan", "null", 0)
testmath("Math.atan", "Number.NaN", Number.NaN)
testmath("Math.atan", "\"a string\"", Number.NaN)
testmath("Math.atan", "'0'", 0)
testmath("Math.atan", "'1'", Math.PI/4)
testmath("Math.atan", "'-1'", -Math.PI/4)
testmath("Math.atan", "'Infinity'", Math.PI/2)
testmath("Math.atan", "'-Infinity'", -Math.PI/2)
testmath("Math.atan", "0", 0)
testmath("Math.atan", "-0", -0)
testmath("Infinity/Math.atan", "-0", -Infinity)
testmath("Math.atan", "Number.POSITIVE_INFINITY", Math.PI/2)
testmath("Math.atan", "Number.NEGATIVE_INFINITY", -Math.PI/2)
testmath("Math.atan", "1", Math.PI/4)
testmath("Math.atan", "-1", -Math.PI/4)

testmath("Math.atan2", "Number.NaN,0", Number.NaN)
testmath("Math.atan2", "null, null", 0)
testmath("Math.atan2", "void 0, void 0", Number.NaN)
testmath("Math.atan2", "0,Number.NaN", Number.NaN)
testmath("Math.atan2", "1,0", Math.PI/2)
testmath("Math.atan2", "1,-0", Math.PI/2)
testmath("Math.atan2", "0,0.001", 0)
testmath("Math.atan2", "0,0", 0)
testmath("Math.atan2", "0,-0", Math.PI)
testmath("Math.atan2", "0, -1", Math.PI)
testmath("Math.atan2", "-0, 1", -0)
testmath("Infinity/Math.atan2", "-0,1", -Infinity)
testmath("Math.atan2", "-0,0", -0)
testmath("Math.atan2", "-0, -0", -Math.PI)
testmath("Math.atan2", "-0, -1", -Math.PI)
testmath("Math.atan2", "-1, 0", -Math.PI/2)
testmath("Math.atan2", "-1, -0", -Math.PI/2)
testmath("Math.atan2", "1, Number.POSITIVE_INFINITY", 0)
testmath("Math.atan2", "1, Number.NEGATIVE_INFINITY", Math.PI)
testmath("Math.atan2", "-1,Number.POSITIVE_INFINITY", -0)
testmath("Infinity/Math.atan2", "-1,Infinity", -Infinity)
testmath("Math.atan2", "-1,Number.NEGATIVE_INFINITY", -Math.PI)
testmath("Math.atan2", "Number.POSITIVE_INFINITY, 0", Math.PI/2)
testmath("Math.atan2", "Number.POSITIVE_INFINITY, 1", Math.PI/2)
testmath("Math.atan2", "Number.POSITIVE_INFINITY,-1", Math.PI/2)
testmath("Math.atan2", "Number.POSITIVE_INFINITY,-0", Math.PI/2)
testmath("Math.atan2", "Number.NEGATIVE_INFINITY, 0", -Math.PI/2)
testmath("Math.atan2", "Number.NEGATIVE_INFINITY,-0", -Math.PI/2)
testmath("Math.atan2", "Number.NEGATIVE_INFINITY, 1", -Math.PI/2)
testmath("Math.atan2", "Number.NEGATIVE_INFINITY,-1", -Math.PI/2)
testmath("Math.atan2", "Number.POSITIVE_INFINITY, Number.POSITIVE_INFINITY", Math.PI/4)
testmath("Math.atan2", "Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY", 3*Math.PI/4)
testmath("Math.atan2", "Number.NEGATIVE_INFINITY, Number.POSITIVE_INFINITY", -Math.PI/4)
testmath("Math.atan2", "Number.NEGATIVE_INFINITY, Number.NEGATIVE_INFINITY", -3*Math.PI/4)
testmath("Math.atan2", "-1, 1", -Math.PI/4)

testmath("Math.ceil", "Number.NaN", Number.NaN)
testmath("Math.ceil", "null", 0)
testmath("Math.ceil", "void 0", Number.NaN)
testmath("Math.ceil", "'0'", 0)
testmath("Math.ceil", "'-0'", -0)
testmath("Infinity/Math.ceil", "'0'", Infinity)
testmath("Infinity/Math.ceil", "'-0'", -Infinity)
testmath("Math.ceil", "0", 0)
testmath("Math.ceil", "-0", -0)
testmath("Infinity/Math.ceil", "0", Infinity)
testmath("Infinity/Math.ceil", "-0", -Infinity)
testmath("Math.ceil", "Number.POSITIVE_INFINITY", Number.POSITIVE_INFINITY)
testmath("Math.ceil", "Number.NEGATIVE_INFINITY", Number.NEGATIVE_INFINITY)
testmath("Math.ceil", "-Number.MIN_VALUE", -0)
testmath("Infinity/Math.ceil", "-Number.MIN_VALUE", -Infinity)
testmath("Math.ceil", "1", 1)
testmath("Math.ceil", "-1", -1)
testmath("Math.ceil", "-0.9", -0)
testmath("Infinity/Math.ceil", "-0.9", -Infinity)
testmath("Math.ceil", "0.9", 1)
testmath("Math.ceil", "-1.1", -1)
testmath("Math.ceil", "1.1", 2)
testmath("Math.ceil", "Number.POSITIVE_INFINITY", -Math.floor(-Infinity))
testmath("Math.ceil", "Number.NEGATIVE_INFINITY", -Math.floor(Infinity))
testmath("Math.ceil", "-Number.MIN_VALUE", -Math.floor(Number.MIN_VALUE))
testmath("Math.ceil", "1", -Math.floor(-1))
testmath("Math.ceil", "-1", -Math.floor(1))
testmath("Math.ceil", "-0.9", -Math.floor(0.9))
testmath("Math.ceil", "0.9", -Math.floor(-0.9))
testmath("Math.ceil", "-1.1", -Math.floor(1.1))
testmath("Math.ceil", "1.1", -Math.floor(-1.1))

testmath("Math.cos", "void 0", Number.NaN)
testmath("Math.cos", "false", 1)
testmath("Math.cos", "null", 1)
testmath("Math.cos", "'0'", 1)
testmath("Math.cos", "\"Infinity\"", Number.NaN)
testmath("Math.cos", "'3.14159265359'", -1)
testmath("Math.cos", "Number.NaN", Number.NaN)
testmath("Math.cos", "0", 1)
testmath("Math.cos", "-0", 1)
testmath("Math.cos", "Number.POSITIVE_INFINITY", Number.NaN)
testmath("Math.cos", "Number.NEGATIVE_INFINITY", Number.NaN)
testmath("Math.cos", "0.7853981633974", 0.7071067811865)
testmath("Math.cos", "1.570796326795", 0)
testmath("Math.cos", "2.356194490192", -0.7071067811865)
testmath("Math.cos", "3.14159265359", -1)
testmath("Math.cos", "3.926990816987", -0.7071067811865)
testmath("Math.cos", "4.712388980385", 0)
testmath("Math.cos", "5.497787143782", 0.7071067811865)
testmath("Math.cos", "Math.PI*2", 1)
testmath("Math.cos", "Math.PI/4", Math.SQRT2/2)
testmath("Math.cos", "Math.PI/2", 0)
testmath("Math.cos", "3*Math.PI/4", -Math.SQRT2/2)
testmath("Math.cos", "Math.PI", -1)
testmath("Math.cos", "5*Math.PI/4", -Math.SQRT2/2)
testmath("Math.cos", "3*Math.PI/2", 0)
testmath("Math.cos", "7*Math.PI/4", Math.SQRT2/2)
testmath("Math.cos", "2*Math.PI", 1)
testmath("Math.cos", "-0.7853981633974", 0.7071067811865)
testmath("Math.cos", "-1.570796326795", 0)
testmath("Math.cos", "2.3561944901920", -.7071067811865)
testmath("Math.cos", "3.14159265359", -1)
testmath("Math.cos", "3.926990816987", -0.7071067811865)
testmath("Math.cos", "4.712388980385", 0)
testmath("Math.cos", "5.497787143782", 0.7071067811865)
testmath("Math.cos", "6.28318530718", 1)
testmath("Math.cos", "-Math.PI/4", Math.SQRT2/2)
testmath("Math.cos", "-Math.PI/2", 0)
testmath("Math.cos", "-3*Math.PI/4", -Math.SQRT2/2)
testmath("Math.cos", "-Math.PI", -1)
testmath("Math.cos", "-5*Math.PI/4", -Math.SQRT2/2)
testmath("Math.cos", "-3*Math.PI/2", 0)
testmath("Math.cos", "-7*Math.PI/4", Math.SQRT2/2)
testmath("Math.cos", "-Math.PI*2", 1)

testmath("Math.exp", "null", 1)
testmath("Math.exp", "void 0", Number.NaN)
testmath("Math.exp", "1", Math.E)
testmath("Math.exp", "true", Math.E)
testmath("Math.exp", "false", 1)
testmath("Math.exp", "'1'", Math.E)
testmath("Math.exp", "'0'", 1)
testmath("Math.exp", "Number.NaN", Number.NaN)
testmath("Math.exp", "0", 1)
testmath("Math.exp", "-0", 1)
testmath("Math.exp", "Number.POSITIVE_INFINITY", Number.POSITIVE_INFINITY)
testmath("Math.exp", "Number.NEGATIVE_INFINITY", 0)

testmath("Math.floor", "void 0", Number.NaN)
testmath("Math.floor", "null", 0)
testmath("Math.floor", "true", 1)
testmath("Math.floor", "false", 0)
testmath("Math.floor", "\"1.1\"", 1)
testmath("Math.floor", "\"-1.1\"", -2)
testmath("Math.floor", "\"0.1\"", 0)
testmath("Math.floor", "\"-0.1\"", -1)
testmath("Math.floor", "Number.NaN", Number.NaN)
testmath("Math.floor(Number.NaN) == -Math.ceil", "-Number.NaN", false)
testmath("Math.floor", "0", 0)
testmath("Math.floor(0) == -Math.ceil", "-0", true)
testmath("Math.floor", "-0", -0)
testmath("Infinity/Math.floor", "-0", -Infinity)
testmath("Math.floor(-0)== -Math.ceil", "0", true)
testmath("Math.floor", "Number.POSITIVE_INFINITY", Number.POSITIVE_INFINITY)
testmath("Math.floor(Number.POSITIVE_INFINITY) == -Math.ceil", "Number.NEGATIVE_INFINITY", true)
testmath("Math.floor", "Number.NEGATIVE_INFINITY", Number.NEGATIVE_INFINITY)
testmath("Math.floor(Number.NEGATIVE_INFINITY) == -Math.ceil", "Number.POSITIVE_INFINITY", true)
testmath("Math.floor", "0.0000001", 0)
testmath("Math.floor(0.0000001)==-Math.ceil", "-0.0000001", true)
testmath("Math.floor", "-0.0000001", -1)
testmath("Math.floor(-0.0000001)==-Math.ceil", "0.0000001", true)

testmath("Math.log", "void 0", Number.NaN)
testmath("Math.log", "null", Number.NEGATIVE_INFINITY)
testmath("Math.log", "true", 0)
testmath("Math.log", "false", -Infinity)
testmath("Math.log", "'0'", -Infinity)
testmath("Math.log", "'1'", 0)
testmath("Math.log", "\"Infinity\"", Infinity)
testmath("Math.log", "Number.NaN", Number.NaN)
testmath("Math.log", "-0.000001", Number.NaN)
testmath("Math.log", "-1", Number.NaN)
testmath("Math.log", "0", Number.NEGATIVE_INFINITY)
testmath("Math.log", "-0", Number.NEGATIVE_INFINITY)
testmath("Math.log", "1", 0)
testmath("Math.log", "Number.POSITIVE_INFINITY", Number.POSITIVE_INFINITY)
testmath("Math.log", "Number.NEGATIVE_INFINITY", Number.NaN)

testmath("Math.max", "void 0, 1", Number.NaN)
testmath("Math.max", "void 0, void 0", Number.NaN)
testmath("Math.max", "null, 1", 1)
testmath("Math.max", "-1, null", 0)
testmath("Math.max", "true,false", 1)
testmath("Math.max", "\"-99\",\"99\"", 99)
testmath("Math.max", "Number.NaN,Number.POSITIVE_INFINITY", Number.NaN)
testmath("Math.max", "Number.NaN, 0", Number.NaN)
testmath("Math.max", "\"a string\", 0", Number.NaN)
testmath("Math.max", "Number.NaN,1", Number.NaN)
testmath("Math.max", "\"a string\", Number.POSITIVE_INFINITY", Number.NaN)
testmath("Math.max", "Number.POSITIVE_INFINITY, Number.NaN", Number.NaN)
testmath("Math.max", "Number.NaN, Number.NaN", Number.NaN)
testmath("Math.max", "0,Number.NaN", Number.NaN)
testmath("Math.max", "1, Number.NaN", Number.NaN)
testmath("Math.max", "0,0", 0)
testmath("Math.max", "0,-0", 0)
testmath("Math.max", "-0,0", 0)
testmath("Math.max", "-0,-0", -0)
testmath("Infinity/Math.max", "-0,-0", -Infinity)
testmath("Math.max", "Number.POSITIVE_INFINITY, Number.MAX_VALUE", Number.POSITIVE_INFINITY)
testmath("Math.max", "Number.POSITIVE_INFINITY,Number.POSITIVE_INFINITY", Number.POSITIVE_INFINITY)
testmath("Math.max", "Number.NEGATIVE_INFINITY,Number.NEGATIVE_INFINITY", Number.NEGATIVE_INFINITY)
testmath("Math.max", "1,.99999999999999", 1)
testmath("Math.max", "-1,-.99999999999999", -.99999999999999)

testmath("Math.min", "void 0, 1", Number.NaN)
testmath("Math.min", "void 0, void 0", Number.NaN)
testmath("Math.min", "null, 1", 0)
testmath("Math.min", "-1, null", -1)
testmath("Math.min", "true,false", 0)
testmath("Math.min", "\"-99\",\"99\"", -99)
testmath("Math.min", "Number.NaN,0", Number.NaN)
testmath("Math.min", "Number.NaN,1", Number.NaN)
testmath("Math.min", "Number.NaN,-1", Number.NaN)
testmath("Math.min", "0,Number.NaN", Number.NaN)
testmath("Math.min", "1,Number.NaN", Number.NaN)
testmath("Math.min", "-1,Number.NaN", Number.NaN)
testmath("Math.min", "Number.NaN,Number.NaN", Number.NaN)
testmath("Math.min", "1,1.0000000001", 1)
testmath("Math.min", "1.0000000001,1", 1)
testmath("Math.min", "0,0", 0)
testmath("Math.min", "0,-0", -0)
testmath("Math.min", "-0,-0", -0)
testmath("Infinity/Math.min", "0,-0", -Infinity)
testmath("Infinity/Math.min", "-0,-0", -Infinity)

testmath("Math.pow", "null,null", 1)
testmath("Math.pow", "void 0, void 0", Number.NaN)
testmath("Math.pow", "true, false", 1)
testmath("Math.pow", "false,true", 0)
testmath("Math.pow", "'2','32'", 4294967296)
testmath("Math.pow", "1,Number.NaN", Number.NaN)
testmath("Math.pow", "0,Number.NaN", Number.NaN)
testmath("Math.pow", "Number.NaN,0", 1)
testmath("Math.pow", "Number.NaN,-0", 1)
testmath("Math.pow", "Number.NaN, 1", Number.NaN)
testmath("Math.pow", "Number.NaN, .5", Number.NaN)
testmath("Math.pow", "1.00000001, Number.POSITIVE_INFINITY", Number.POSITIVE_INFINITY)
testmath("Math.pow", "1.00000001, Number.NEGATIVE_INFINITY", 0)
testmath("Math.pow", "-1.00000001,Number.POSITIVE_INFINITY", Number.POSITIVE_INFINITY)
testmath("Math.pow", "-1.00000001,Number.NEGATIVE_INFINITY", 0)
testmath("Math.pow", "1, Number.POSITIVE_INFINITY", Number.NaN)
testmath("Math.pow", "1, Number.NEGATIVE_INFINITY", Number.NaN)
testmath("Math.pow", "-1, Number.POSITIVE_INFINITY", Number.NaN)
testmath("Math.pow", "-1, Number.NEGATIVE_INFINITY", Number.NaN)
testmath("Math.pow", ".0000000009, Number.POSITIVE_INFINITY", 0)
testmath("Math.pow", "-.0000000009, Number.POSITIVE_INFINITY", 0)
testmath("Math.pow", "-.0000000009, Number.NEGATIVE_INFINITY", Number.POSITIVE_INFINITY)
testmath("Math.pow", "Number.POSITIVE_INFINITY,.00000000001", Number.POSITIVE_INFINITY)
testmath("Math.pow", "Number.POSITIVE_INFINITY, 1", Number.POSITIVE_INFINITY)
testmath("Math.pow", "Number.POSITIVE_INFINITY, -.00000000001", 0)
testmath("Math.pow", "Number.POSITIVE_INFINITY, -1", 0)
testmath("Math.pow", "Number.NEGATIVE_INFINITY, 1", Number.NEGATIVE_INFINITY)
testmath("Math.pow", "Number.NEGATIVE_INFINITY, 333", Number.NEGATIVE_INFINITY)
testmath("Math.pow", "Number.POSITIVE_INFINITY, 2", Number.POSITIVE_INFINITY)
testmath("Math.pow", "Number.NEGATIVE_INFINITY, 666", Number.POSITIVE_INFINITY)
testmath("Math.pow", "Number.NEGATIVE_INFINITY, 0.5", Number.POSITIVE_INFINITY)
testmath("Math.pow", "Number.NEGATIVE_INFINITY, Number.POSITIVE_INFINITY", Number.POSITIVE_INFINITY)
testmath("Math.pow", "Number.NEGATIVE_INFINITY, -1", -0)
testmath("Infinity/Math.pow", "Number.NEGATIVE_INFINITY, -1", -Infinity)
testmath("Math.pow", "Number.NEGATIVE_INFINITY, -3", -0)
testmath("Math.pow", "Number.NEGATIVE_INFINITY, -2", 0)
testmath("Math.pow", "Number.NEGATIVE_INFINITY,-0.5", 0)
testmath("Math.pow", "Number.NEGATIVE_INFINITY, Number.NEGATIVE_INFINITY", 0)
testmath("Math.pow", "0,1", 0)
testmath("Math.pow", "0,0", 1)
testmath("Math.pow", "1,0", 1)
testmath("Math.pow", "-1,0", 1)
testmath("Math.pow", "0,0.5", 0)
testmath("Math.pow", "0,1000", 0)
testmath("Math.pow", "0, Number.POSITIVE_INFINITY", 0)
testmath("Math.pow", "0, -1", Number.POSITIVE_INFINITY)
testmath("Math.pow", "0, -0.5", Number.POSITIVE_INFINITY)
testmath("Math.pow", "0, -1000", Number.POSITIVE_INFINITY)
testmath("Math.pow", "0, Number.NEGATIVE_INFINITY", Number.POSITIVE_INFINITY)
testmath("Math.pow", "-0, 1", -0)
testmath("Math.pow", "-0,3", -0)
testmath("Infinity/Math.pow", "-0, 1", -Infinity)
testmath("Infinity/Math.pow", "-0,3", -Infinity)
testmath("Math.pow", "-0,2", 0)
testmath("Math.pow", "-0, Number.POSITIVE_INFINITY", 0)
testmath("Math.pow", "-0, -1", Number.NEGATIVE_INFINITY)
testmath("Math.pow", "-0, -10001", Number.NEGATIVE_INFINITY)
testmath("Math.pow", "-0, -2", Number.POSITIVE_INFINITY)
testmath("Math.pow", "-0, 0.5", 0)
testmath("Math.pow", "-0, Number.POSITIVE_INFINITY", 0)
testmath("Math.pow", "-1, 0.5", Number.NaN)
testmath("Math.pow", "-1, Number.NaN", Number.NaN)
testmath("Math.pow", "-1, -0.5", Number.NaN)

testmath("Math.round", "0", 0)
testmath("Math.round", "void 0", Number.NaN)
testmath("Math.round", "true", 1)
testmath("Math.round", "false", 0)
testmath("Math.round", "'.99999'", 1)
testmath("Math.round", "'12345e-2'", 123)
testmath("Math.round", "Number.NaN", Number.NaN)
testmath("Math.round", "0", 0)
testmath("Math.round", "-0", -0)
testmath("Infinity/Math.round", "-0", -Infinity)
testmath("Math.round", "Number.POSITIVE_INFINITY", Number.POSITIVE_INFINITY)
testmath("Math.round", "Number.NEGATIVE_INFINITY", Number.NEGATIVE_INFINITY)
testmath("Math.round", "0.49", 0)
testmath("Math.round", "0.5", 1)
testmath("Math.round", "0.51", 1)
testmath("Math.round", "-0.49", -0)
testmath("Math.round", "-0.5", -0)
testmath("Infinity/Math.round", "-0.49", -Infinity)
testmath("Infinity/Math.round", "-0.5", -Infinity)
testmath("Math.round", "-0.51", -1)
testmath("Math.round", "3.5", 4)
testmath("Math.round", "-3", -3)

testmath("Math.sin", "null", 0)
testmath("Math.sin", "void 0", Number.NaN)
testmath("Math.sin", "false", 0)
testmath("Math.sin", "'2.356194490192'", 0.7071067811865)
testmath("Math.sin", "Number.NaN", Number.NaN)
testmath("Math.sin", "0", 0)
testmath("Math.sin", "-0", -0)
testmath("Math.sin", "Number.POSITIVE_INFINITY", Number.NaN)
testmath("Math.sin", "Number.NEGATIVE_INFINITY", Number.NaN)
testmath("Math.sin", "0.7853981633974", 0.7071067811865)
testmath("Math.sin", "1.570796326795", 1)
testmath("Math.sin", "2.356194490192", 0.7071067811865)
testmath("Math.sin", "3.14159265359", 0)

testmath("Math.sqrt", "void 0", Number.NaN)
testmath("Math.sqrt", "null", 0)
testmath("Math.sqrt", "1", 1)
testmath("Math.sqrt", "false", 0)
testmath("Math.sqrt", "'225'", 15)
testmath("Math.sqrt", "Number.NaN", Number.NaN)
testmath("Math.sqrt", "Number.NEGATIVE_INFINITY", Number.NaN)
testmath("Math.sqrt", "-1", Number.NaN)
testmath("Math.sqrt", "-0.5", Number.NaN)
testmath("Math.sqrt", "0", 0)
testmath("Math.sqrt", "-0", -0)
testmath("Infinity/Math.sqrt", "-0", -Infinity)
testmath("Math.sqrt", "Number.POSITIVE_INFINITY", Number.POSITIVE_INFINITY)
testmath("Math.sqrt", "1", 1)
testmath("Math.sqrt", "2", Math.SQRT2)
testmath("Math.sqrt", "0.5", Math.SQRT1_2)
testmath("Math.sqrt", "4", 2)
testmath("Math.sqrt", "9", 3)
testmath("Math.sqrt", "16", 4)
testmath("Math.sqrt", "25", 5)
testmath("Math.sqrt", "36", 6)
testmath("Math.sqrt", "49", 7)
testmath("Math.sqrt", "64", 8)
testmath("Math.sqrt", "256", 16)
testmath("Math.sqrt", "10000", 100)
testmath("Math.sqrt", "65536", 256)
testmath("Math.sqrt", "0.09", 0.3)
testmath("Math.sqrt", "0.01", 0.1)
testmath("Math.sqrt", "0.00000001", 0.0001)

testmath("Math.tan", "void 0", Number.NaN)
testmath("Math.tan", "null", 0)
testmath("Math.tan", "false", 0)
testmath("Math.tan", "Number.NaN", Number.NaN)
testmath("Math.tan", "0", 0)
testmath("Math.tan", "-0", -0)
testmath("Math.tan", "Number.POSITIVE_INFINITY", Number.NaN)
testmath("Math.tan", "Number.NEGATIVE_INFINITY", Number.NaN)
testmath("Math.tan", "Math.PI/4", 1)
testmath("Math.tan", "3*Math.PI/4", -1)
testmath("Math.tan", "Math.PI", -0)
testmath("Math.tan", "5*Math.PI/4", 1)
testmath("Math.tan", "7*Math.PI/4", -1)
testmath("Infinity/Math.tan", "-0", -Infinity)

jit(false);

/* Keep these at the end so that we can see the summary after the trace-debug spew. */
if (0) {
  print("\npassed:", passes.length && passes.join(","));
  print("\nFAILED:", fails.length && fails.join(","));
}
