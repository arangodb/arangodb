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

var gTestfile = 'trace-test.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 'none';
var summary = 'trace-capability mini-testsuite';

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

var gDoMandelbrotTest = true;
if ("gSkipSlowTests" in this && gSkipSlowTests) {
    print("** Skipping slow tests");
    gDoMandelbrotTest = false;
}

if (!('gSrcdir' in this))
    gSrcdir = '.';

if (!('gReportSummary' in this))
    gReportSummary = true;

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
  f("timeoutIntoInterpreter");
  f("typeMapMismatchAtEntry");
  f("returnToDifferentLoopHeader");
  f("traceTriggered");
  f("globalShapeMismatchAtEntry");
  f("treesTrashed");
  f("slotPromoted");
  f("unstableLoopVariable");
  f("breakLoopExits");
  f("returnLoopExits");
  f("mergedLoopExits");
  f("noCompatInnerTrees");
}

var jitProps = {};
jitstatHandler(function(prop) {
                 jitProps[prop] = true;
               });
var hadJITstats = false;
for (var p in jitProps)
  hadJITstats = true;

function test(f)
{
  // Clear out any accumulated confounding state in the oracle / JIT cache.
  gc();

  if (!testName || testName == f.name) {
    var expectedJITstats = f.jitstats;
    if (hadJITstats && expectedJITstats)
    {
      var expectedProps = {};
      jitstatHandler(function(prop) {
                       if (prop in expectedJITstats)
                         expectedProps[prop] = true;
                     });
      for (var p in expectedJITstats)
      {
        if (!(p in expectedProps))
          throw "Bad property in " + f.name + ".jitstats: " + p;
      }
    }

    // Collect our jit stats
    var localJITstats = {};
    jitstatHandler(function(prop) {
                     localJITstats[prop] = tracemonkey[prop];
                   });
    check(f.name, f(), f.expected, localJITstats, expectedJITstats);
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
      return print("TEST-PASS | trace-test.js |", desc);
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
  print("TEST-UNEXPECTED-FAIL | trace-test.js |", desc, ": expected", typeof(expected),
        "(", uneval(expected), ")",
        (expectedStats ? " [" + expectedStats + "] " : ""),
        "!= actual",
        typeof(actual), "(", uneval(actual), ")",
        (actualStats ? " [" + actualStats + "] " : ""));
}

function ifInsideLoop()
{
  var cond = true, intCond = 5, count = 0;
  for (var i = 0; i < 100; i++) {
    if (cond)
      count++;
    if (intCond)
      count++;
  }
  return count;
}
ifInsideLoop.expected = 200;
test(ifInsideLoop);

function bitwiseAnd_inner(bitwiseAndValue) {
  for (var i = 0; i < 60000; i++)
    bitwiseAndValue = bitwiseAndValue & i;
  return bitwiseAndValue;
}
function bitwiseAnd()
{
  return bitwiseAnd_inner(12341234);
}
bitwiseAnd.expected = 0;
test(bitwiseAnd);

if (!testName || testName == "bitwiseGlobal") {
  bitwiseAndValue = Math.pow(2,32);
  for (var i = 0; i < 60000; i++)
    bitwiseAndValue = bitwiseAndValue & i;
  check("bitwiseGlobal", bitwiseAndValue, 0);
}


function equalInt()
{
  var i1 = 55, one = 1, zero = 0, undef;
  var o1 = { }, o2 = { };
  var s = "5";
  var hits = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
  for (var i = 0; i < 5000; i++) {
    if (i1 == 55) hits[0]++;
    if (i1 != 56) hits[1]++;
    if (i1 < 56)  hits[2]++;
    if (i1 > 50)  hits[3]++;
    if (i1 <= 60) hits[4]++;
    if (i1 >= 30) hits[5]++;
    if (i1 == 7)  hits[6]++;
    if (i1 != 55) hits[7]++;
    if (i1 < 30)  hits[8]++;
    if (i1 > 90)  hits[9]++;
    if (i1 <= 40) hits[10]++;
    if (i1 >= 70) hits[11]++;
    if (o1 == o2) hits[12]++;
    if (o2 != null) hits[13]++;
    if (s < 10) hits[14]++;
    if (true < zero) hits[15]++;
    if (undef > one) hits[16]++;
    if (undef < zero) hits[17]++;
  }
  return hits.toString();
}
equalInt.expected = "5000,5000,5000,5000,5000,5000,0,0,0,0,0,0,0,5000,5000,0,0,0";
test(equalInt);

var a;
function setelem()
{
  a = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
  a = a.concat(a, a, a);
  var l = a.length;
  for (var i = 0; i < l; i++) {
    a[i] = i;
  }
  return a.toString();
}
setelem.expected = "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83";
test(setelem);

function getelem_inner(a)
{
  var accum = 0;
  var l = a.length;
  for (var i = 0; i < l; i++) {
    accum += a[i];
  }
  return accum;
}
function getelem()
{
  return getelem_inner(a);
}
getelem.expected = 3486;
test(getelem);

globalName = 907;
function name()
{
  var a = 0;
  for (var i = 0; i < 100; i++)
    a = globalName;
  return a;
}
name.expected = 907;
test(name);

var globalInt = 0;
if (!testName || testName == "globalGet") {
  for (var i = 0; i < 500; i++)
    globalInt = globalName + i;
  check("globalGet", globalInt, globalName + 499);
}

if (!testName || testName == "globalSet") {
  for (var i = 0; i < 500; i++)
    globalInt = i;
  check("globalSet", globalInt, 499);
}

function arith()
{
  var accum = 0;
  for (var i = 0; i < 100; i++) {
    accum += (i * 2) - 1;
  }
  return accum;
}
arith.expected = 9800;
test(arith);

function lsh_inner(n)
{
  var r;
  for (var i = 0; i < 35; i++)
    r = 0x1 << n;
  return r;
}

map_test (lsh_inner,
          [{input: 15, expected: 32768},
           {input: 55, expected: 8388608},
           {input: 1,  expected: 2},
           {input: 0,  expected: 1}]);

function rsh_inner(n)
{
  var r;
  for (var i = 0; i < 35; i++)
    r = 0x11010101 >> n;
  return r;
}
map_test (rsh_inner,
          [{input: 8,  expected: 1114369},
           {input: 5,  expected: 8914952},
           {input: 35, expected: 35659808},
           {input: -1, expected: 0}]);

function ursh_inner(n)
{
  var r;
  for (var i = 0; i < 35; i++)
    r = -55 >>> n;
  return r;
}
map_test (ursh_inner,
          [{input: 8,  expected: 16777215},
           {input: 33, expected: 2147483620},
           {input: 0,  expected: 4294967241},
           {input: 1,  expected: 2147483620}]);

function doMath_inner(cos)
{
  var s = 0;
  var sin = Math.sin;
  for (var i = 0; i < 200; i++)
    s = -Math.pow(sin(i) + cos(i * 0.75), 4);
  return s;
}
function doMath() {
  return doMath_inner(Math.cos);
}
doMath.expected = -0.5405549555611059;
test(doMath);

function fannkuch() {
  var count = Array(8);
  var r = 8;
  var done = 0;
  while (done < 40) {
    // write-out the first 30 permutations
    done += r;
    while (r != 1) { count[r - 1] = r; r--; }
    while (true) {
      count[r] = count[r] - 1;
      if (count[r] > 0) break;
      r++;
    }
  }
  return done;
}
fannkuch.expected = 41;
test(fannkuch);

function xprop()
{
  a = 0;
  for (var i = 0; i < 20; i++)
    a += 7;
  return a;
}
xprop.expected = 140;
test(xprop);

var a = 2;
function getprop_inner(o2)
{
  var o = {a:5};
  var t = this;
  var x = 0;
  for (var i = 0; i < 20; i++) {
    t = this;
    x += o.a + o2.a + this.a + t.a;
  }
  return x;
}
function getprop() {
  return getprop_inner({a:9});
}
getprop.expected = 360;
test(getprop);

function mod()
{
  var mods = [-1,-1,-1,-1];
  var a = 9.5, b = -5, c = 42, d = (1/0);
  for (var i = 0; i < 20; i++) {
    mods[0] = a % b;
    mods[1] = b % 1;
    mods[2] = c % d;
    mods[3] = c % a;
    mods[4] = b % 0;
  }
  return mods.toString();
}
mod.expected = "4.5,0,42,4,NaN";
test(mod);

function glob_f1() {
  return 1;
}
function glob_f2() {
  return glob_f1();
}
function call()
{
  var q1 = 0, q2 = 0, q3 = 0, q4 = 0, q5 = 0;
  var o = {};
  function f1() {
    return 1;
  }
  function f2(f) {
    return f();
  }
  o.f = f1;
  for (var i = 0; i < 100; ++i) {
    q1 += f1();
    q2 += f2(f1);
    q3 += glob_f1();
    q4 += o.f();
    q5 += glob_f2();
  }
  var ret = String([q1, q2, q3, q4, q5]);
  return ret;
}
call.expected =  "100,100,100,100,100";
test(call);

function setprop()
{
  var obj = { a:-1 };
  var obj2 = { b:-1, a:-1 };
  for (var i = 0; i < 20; i++) {
    obj2.b = obj.a = i;
  }
  return [obj.a, obj2.a, obj2.b].toString();
}
setprop.expected =  "19,-1,19";
test(setprop);

function testif() {
	var q = 0;
	for (var i = 0; i < 100; i++) {
		if ((i & 1) == 0)
			q++;
		else
			q--;
	}
  return q;
}
testif.expected = 0;
test(testif);

var globalinc = 0;
function testincops(n) {
  var i = 0, o = {p:0}, a = [0];
  n = 100;

  for (i = 0; i < n; i++);
  while (i-- > 0);
  for (i = 0; i < n; ++i);
  while (--i >= 0);

  for (o.p = 0; o.p < n; o.p++) globalinc++;
  while (o.p-- > 0) --globalinc;
  for (o.p = 0; o.p < n; ++o.p) ++globalinc;
  while (--o.p >= 0) globalinc--;

  ++i; // set to 0
  for (a[i] = 0; a[i] < n; a[i]++);
  while (a[i]-- > 0);
  for (a[i] = 0; a[i] < n; ++a[i]);
  while (--a[i] >= 0);

  return [++o.p, ++a[i], globalinc].toString();
}
testincops.expected = "0,0,0";
test(testincops);

function trees() {
  var i = 0, o = [0,0,0];
  for (i = 0; i < 100; ++i) {
    if ((i & 1) == 0) o[0]++;
    else if ((i & 2) == 0) o[1]++;
    else o[2]++;
  }
  return String(o);
}
trees.expected = "50,25,25";
test(trees);

function unboxint() {
  var q = 0;
  var o = [4];
  for (var i = 0; i < 100; ++i)
    q = o[0] << 1;
  return q;
}
unboxint.expected = 8;
test(unboxint);

function strings()
{
  var a = [], b = -1;
  var s = "abcdefghij", s2 = "a";
  var f = "f";
  var c = 0, d = 0, e = 0, g = 0;
  for (var i = 0; i < 10; i++) {
    a[i] = (s.substring(i, i+1) + s[i] + String.fromCharCode(s2.charCodeAt(0) + i)).concat(i) + i;
    if (s[i] == f)
      c++;
    if (s[i] != 'b')
      d++;
    if ("B" > s2)
      g++; // f already used
    if (s2 < "b")
      e++;
    b = s.length;
  }
  return a.toString() + b + c + d + e + g;
}
strings.expected = "aaa00,bbb11,ccc22,ddd33,eee44,fff55,ggg66,hhh77,iii88,jjj991019100";
test(strings);

function dependentStrings()
{
  var a = [];
  var t = "abcdefghijklmnopqrst";
  for (var i = 0; i < 10; i++) {
    var s = t.substring(2*i, 2*i + 2);
    a[i] = s + s.length;
  }
  return a.join("");
}
dependentStrings.expected = "ab2cd2ef2gh2ij2kl2mn2op2qr2st2";
test(dependentStrings);

function stringConvert()
{
  var a = [];
  var s1 = "F", s2 = "1.3", s3 = "5";
  for (var i = 0; i < 10; i++) {
    a[0] = 1 >> s1;
    a[1] = 10 - s2;
    a[2] = 15 * s3;
    a[3] = s3 | 32;
    a[4] = s2 + 60;
    // a[5] = 9 + s3;
    // a[6] = -s3;
    a[7] = s3 & "7";
    // a[8] = ~s3;
  }
  return a.toString();
}
stringConvert.expected = "1,8.7,75,37,1.360,,,5";
test(stringConvert);

function orTestHelper(a, b, n)
{
  var k = 0;
  for (var i = 0; i < n; i++) {
    if (a || b)
      k += i;
  }
  return k;
}

var orNaNTest1, orNaNTest2;

orNaNTest1 = new Function("return orTestHelper(NaN, NaN, 10);");
orNaNTest1.name = 'orNaNTest1';
orNaNTest1.expected = 0;
orNaNTest2 = new Function("return orTestHelper(NaN, 1, 10);");
orNaNTest2.name = 'orNaNTest2';
orNaNTest2.expected = 45;
test(orNaNTest1);
test(orNaNTest2);

function andTestHelper(a, b, n)
{
  var k = 0;
  for (var i = 0; i < n; i++) {
    if (a && b)
      k += i;
  }
  return k;
}

if (!testName || testName == "truthies") {
  (function () {
    var opsies   = ["||", "&&"];
    var falsies  = [null, undefined, false, NaN, 0, ""];
    var truthies = [{}, true, 1, 42, 1/0, -1/0, "blah"];
    var boolies  = [falsies, truthies];

    // The for each here should abort tracing, so that this test framework
    // relies only on the interpreter while the orTestHelper and andTestHelper
    //  functions get trace-JITed.
    for each (var op in opsies) {
        for (var i in boolies) {
          for (var j in boolies[i]) {
            var x = uneval(boolies[i][j]);
            for (var k in boolies) {
              for (var l in boolies[k]) {
                var y = uneval(boolies[k][l]);
                var prefix = (op == "||") ? "or" : "and";
                var f = new Function("return " + prefix + "TestHelper(" + x + "," + y + ",10)");
                f.name = prefix + "Test(" + x + "," + y + ")";
                f.expected = eval(x + op + y) ? 45 : 0;
                test(f);
              }
            }
          }
        }
      }
  })();
}

function nonEmptyStack1Helper(o, farble) {
  var a = [];
  var j = 0;
  for (var i in o)
    a[j++] = i;
  return a.join("");
}

function nonEmptyStack1() {
  return nonEmptyStack1Helper({a:1,b:2,c:3,d:4,e:5,f:6,g:7,h:8}, "hi");
}

nonEmptyStack1.expected = "abcdefgh";
test(nonEmptyStack1);

function nonEmptyStack2()
{
  var a = 0;
  for (var c in {a:1, b:2, c:3}) {
    for (var i = 0; i < 10; i++)
      a += i;
  }
  return String(a);
}
nonEmptyStack2.expected = "135";
test(nonEmptyStack2);

function arityMismatchMissingArg(arg)
{
  for (var a = 0, i = 1; i < 10000; i *= 2) {
    a += i;
  }
  return a;
}
arityMismatchMissingArg.expected = 16383;
test(arityMismatchMissingArg);

function arityMismatchExtraArg()
{
  return arityMismatchMissingArg(1, 2);
}
arityMismatchExtraArg.expected = 16383;
test(arityMismatchExtraArg);

function MyConstructor(i)
{
  this.i = i;
}
MyConstructor.prototype.toString = function() {return this.i + ""};

function newTest()
{
  var a = [];
  for (var i = 0; i < 10; i++)
    a[i] = new MyConstructor(i);
  return a.join("");
}
newTest.expected = "0123456789";
test(newTest);

// The following functions use a delay line of length 2 to change the value
// of the callee without exiting the traced loop. This is obviously tuned to
// match the current HOTLOOP setting of 2.
function shapelessArgCalleeLoop(f, g, h, a)
{
  for (var i = 0; i < 10; i++) {
    f(i, a);
    f = g;
    g = h;
  }
}

function shapelessVarCalleeLoop(f0, g, h, a)
{
  var f = f0;
  for (var i = 0; i < 10; i++) {
    f(i, a);
    f = g;
    g = h;
  }
}

function shapelessLetCalleeLoop(f0, g, h, a)
{
  for (var i = 0; i < 10; i++) {
    let f = f0;
    f(i, a);
    f = g;
    g = h;
  }
}

function shapelessUnknownCalleeLoop(n, f, g, h, a)
{
  for (var i = 0; i < 10; i++) {
    (n || f)(i, a);
    f = g;
    g = h;
  }
}

function shapelessCalleeTest()
{
  var a = [];

  var helper = function (i, a) a[i] = i;
  shapelessArgCalleeLoop(helper, helper, function (i, a) a[i] = -i, a);

  helper = function (i, a) a[10 + i] = i;
  shapelessVarCalleeLoop(helper, helper, function (i, a) a[10 + i] = -i, a);

  helper = function (i, a) a[20 + i] = i;
  shapelessLetCalleeLoop(helper, helper, function (i, a) a[20 + i] = -i, a);

  helper = function (i, a) a[30 + i] = i;
  shapelessUnknownCalleeLoop(null, helper, helper, function (i, a) a[30 + i] = -i, a);

  try {
    helper = {hack: 42};
    shapelessUnknownCalleeLoop(null, helper, helper, helper, a);
  } catch (e) {
    if (e + "" != "TypeError: f is not a function")
      print("shapelessUnknownCalleeLoop: unexpected exception " + e);
  }
  return a.join("");
}
shapelessCalleeTest.expected = "01-2-3-4-5-6-7-8-901-2-3-4-5-6-7-8-9012345678901-2-3-4-5-6-7-8-9";
test(shapelessCalleeTest);

function typeofTest()
{
  var values = ["hi", "hi", "hi", null, 5, 5.1, true, undefined, /foo/, typeofTest, [], {}], types = [];
  for (var i = 0; i < values.length; i++)
    types[i] = typeof values[i];
  return types.toString();
}
typeofTest.expected = "string,string,string,object,number,number,boolean,undefined,object,function,object,object";
test(typeofTest);

function joinTest()
{
  var s = "";
  var a = [];
  for (var i = 0; i < 8; i++)
    a[i] = [String.fromCharCode(97 + i)];
  for (i = 0; i < 8; i++) {
    for (var j = 0; j < 8; j++)
      a[i][1 + j] = j;
  }
  for (i = 0; i < 8; i++)
    s += a[i].join(",");
  return s;
}
joinTest.expected = "a,0,1,2,3,4,5,6,7b,0,1,2,3,4,5,6,7c,0,1,2,3,4,5,6,7d,0,1,2,3,4,5,6,7e,0,1,2,3,4,5,6,7f,0,1,2,3,4,5,6,7g,0,1,2,3,4,5,6,7h,0,1,2,3,4,5,6,7";
test(joinTest);

function arity1(x)
{
  return (x == undefined) ? 1 : 0;
}
function missingArgTest() {
  var q;
  for (var i = 0; i < 10; i++) {
    q = arity1();
  }
  return q;
}
missingArgTest.expected = 1;
test(missingArgTest);

JSON = function () {
  return {
  stringify: function stringify(value, whitelist) {
      switch (typeof(value)) {
      case "object":
      return value.constructor.name;
      }
    }
  };
}();

function missingArgTest2() {
  var testPairs = [
    ["{}", {}],
    ["[]", []],
    ['{"foo":"bar"}', {"foo":"bar"}],
    ]

    var a = [];
  for (var i=0; i < testPairs.length; i++) {
    var s = JSON.stringify(testPairs[i][1])
      a[i] = s;
  }
  return a.join(",");
}
missingArgTest2.expected = /(Object,Array,Object|{},\[\],{"foo":"bar"})/;
test(missingArgTest2);

function deepForInLoop() {
  // NB: the number of props set in C is arefully tuned to match HOTLOOP = 2.
  function C(){this.p = 1, this.q = 2}
  C.prototype = {p:1, q:2, r:3, s:4, t:5};
  var o = new C;
  var j = 0;
  var a = [];
  for (var i in o)
    a[j++] = i;
  return a.join("");
}
deepForInLoop.expected = "pqrst";
test(deepForInLoop);

function nestedExit(x) {
  var q = 0;
  for (var i = 0; i < 10; ++i)
  {
    if (x)
	    ++q;
  }
}
function nestedExitLoop() {
  for (var j = 0; j < 10; ++j)
    nestedExit(j < 7);
  return "ok";
}
nestedExitLoop.expected = "ok";
test(nestedExitLoop);

function bitsinbyte(b) {
  var m = 1, c = 0;
  while(m<0x100) {
    if(b & m) c++;
    m <<= 1;
  }
  return 1;
}
function TimeFunc(func) {
  var x,y;
  for(var y=0; y<256; y++) func(y);
}
function nestedExit2() {
  TimeFunc(bitsinbyte);
  return "ok";
}
nestedExit2.expected = "ok";
test(nestedExit2);

function parsingNumbers() {
  var s1 = "123";
  var s1z = "123zzz";
  var s2 = "123.456";
  var s2z = "123.456zzz";

  var e1 = 123;
  var e2 = 123.456;

  var r1, r1z, r2, r2z;

  for (var i = 0; i < 10; i++) {
    r1 = parseInt(s1);
    r1z = parseInt(s1z);
    r2 = parseFloat(s2);
    r2z = parseFloat(s2z);
  }

  if (r1 == e1 && r1z == e1 && r2 == e2 && r2z == e2)
    return "ok";
  return "fail";
}
parsingNumbers.expected = "ok";
test(parsingNumbers);

function matchInLoop() {
  var k = "hi";
  for (var i = 0; i < 10; i++) {
    var result = k.match(/hi/) != null;
  }
  return result;
}
matchInLoop.expected = true;
test(matchInLoop);

function testMatchAsCondition() {
    var a = ['0', '0', '0', '0'];
    var r = /0/;
    "x".q;
    for (var z = 0; z < 4; z++)
        a[z].match(r) ? 1 : 2;
}
test(testMatchAsCondition);

function deep1(x) {
  if (x > 90)
    return 1;
  return 2;
}
function deep2() {
  for (var i = 0; i < 100; ++i)
    deep1(i);
  return "ok";
}
deep2.expected = "ok";
test(deep2);

function heavyFn1(i) { 
    if (i == 3) {
	var x = 3;
        return [0, i].map(function (i) i + x);
    }
    return [];
}
function testHeavy() {
    for (var i = 0; i <= 3; i++)
        heavyFn1(i);
}
test(testHeavy);

function heavyFn2(i) {
    if (i < 1000)
        return heavyFn1(i);
    return function () i;
}
function testHeavy2() {
    for (var i = 0; i <= 3; i++)
        heavyFn2(i);
}
test(testHeavy2);

var merge_type_maps_x = 0, merge_type_maps_y = 0;
function merge_type_maps() {
  for (merge_type_maps_x = 0; merge_type_maps_x < 50; ++merge_type_maps_x)
    if ((merge_type_maps_x & 1) == 1)
	    ++merge_type_maps_y;
  return [merge_type_maps_x,merge_type_maps_y].join(",");
}
merge_type_maps.expected = "50,25";
test(merge_type_maps)

function inner_double_outer_int() {
  function f(i) {
    for (var m = 0; m < 20; ++m)
	    for (var n = 0; n < 100; n += i)
        ;
    return n;
  }
  return f(.5);
}
inner_double_outer_int.expected = 100;
test(inner_double_outer_int);

function newArrayTest()
{
  var a = [];
  for (var i = 0; i < 10; i++)
    a[i] = new Array();
  return a.map(function(x) x.length).toString();
}
newArrayTest.expected="0,0,0,0,0,0,0,0,0,0";
test(newArrayTest);

function stringSplitTest()
{
  var s = "a,b"
    var a = null;
  for (var i = 0; i < 10; ++i)
    a = s.split(",");
  return a.join();
}
stringSplitTest.expected="a,b";
test(stringSplitTest);

function stringSplitIntoArrayTest()
{
  var s = "a,b"
    var a = [];
  for (var i = 0; i < 10; ++i)
    a[i] = s.split(",");
  return a.join();
}
stringSplitIntoArrayTest.expected="a,b,a,b,a,b,a,b,a,b,a,b,a,b,a,b,a,b,a,b";
test(stringSplitIntoArrayTest);

function forVarInWith() {
  function foo() ({notk:42});
  function bar() ({p:1, q:2, r:3, s:4, t:5});
  var o = foo();
  var a = [];
  with (o) {
    for (var k in bar())
      a[a.length] = k;
  }
  return a.join("");
}
forVarInWith.expected = "pqrst";
test(forVarInWith);

function inObjectTest() {
  var o = {p: 1, q: 2, r: 3, s: 4, t: 5};
  var r = 0;
  for (var i in o) {
    if (!(i in o))
      break;
    if ((i + i) in o)
      break;
    ++r;
  }
  return r;
}
inObjectTest.expected = 5;
test(inObjectTest);

function inArrayTest() {
  var a = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
  for (var i = 0; i < a.length; i++) {
    if (!(i in a))
      break;
  }
  return i;
}
inArrayTest.expected = 10;
test(inArrayTest);

function innerLoopIntOuterDouble() {
  var n = 10000, i=0, j=0, count=0, limit=0;
  for (i = 1; i <= n; ++i) {
    limit = i * 1;
    for (j = 0; j < limit; ++j) {
	    ++count;
    }
  }
  return "" + count;
}
innerLoopIntOuterDouble.expected="50005000";
test(innerLoopIntOuterDouble);

function outerline(){
  var i=0;
  var j=0;

  for (i = 3; i<= 100000; i+=2)
  {
    for (j = 3; j < 1000; j+=2)
    {
	    if ((i & 1) == 1)
        break;
    }
  }
  return "ok";
}
outerline.expected="ok";
test(outerline);

function addAccumulations(f) {
  var a = f();
  var b = f();
  return a() + b();
}

function loopingAccumulator() {
  var x = 0;
  return function () {
    for (var i = 0; i < 10; ++i) {
      ++x;
    }
    return x;
  }
}

function testLoopingAccumulator() {
	var x = addAccumulations(loopingAccumulator);
	return x;
}
testLoopingAccumulator.expected = 20;
test(testLoopingAccumulator);

function testBranchingLoop() {
  var x = 0;
  for (var i=0; i < 100; ++i) {
    if (i == 51) {
      x += 10;
    }
    x++;
  }
  return x;
}
testBranchingLoop.expected = 110;
test(testBranchingLoop);

function testBranchingUnstableLoop() {
  var x = 0;
  for (var i=0; i < 100; ++i) {
    if (i == 51) {
      x += 10.1;
    }
    x++;
  }
  return x;
}
testBranchingUnstableLoop.expected = 110.1;
test(testBranchingUnstableLoop);

function testBranchingUnstableLoopCounter() {
  var x = 0;
  for (var i=0; i < 100; ++i) {
    if (i == 51) {
      i += 1.1;
    }
    x++;
  }
  return x;
}
testBranchingUnstableLoopCounter.expected = 99;
test(testBranchingUnstableLoopCounter);


function testBranchingUnstableObject() {
  var x = {s: "a"};
  var t = "";
  for (var i=0; i < 100; ++i) {
    if (i == 51)
    {
      x.s = 5;
    }
    t += x.s;
  }
  return t.length;
}
testBranchingUnstableObject.expected = 100;
test(testBranchingUnstableObject);

function testArrayDensityChange() {
  var x = [];
  var count = 0;
  for (var i=0; i < 100; ++i) {
    x[i] = "asdf";
  }
  for (var i=0; i < x.length; ++i) {
    if (i == 51)
    {
      x[199] = "asdf";
    }
    if (x[i])
      count += x[i].length;
  }
  return count;
}
testArrayDensityChange.expected = 404;
test(testArrayDensityChange);

function testDoubleToStr() {
  var x = 0.0;
  var y = 5.5;
  for (var i = 0; i < 200; i++) {
    x += parseFloat(y.toString());
  }
  return x;
}
testDoubleToStr.expected = 5.5*200;
test(testDoubleToStr);

function testNumberToString() {
  var x = new Number(0);
  for (var i = 0; i < 4; i++)
    x.toString();
}
test(testNumberToString);

function testDecayingInnerLoop() {
  var i, j, k = 10;
  for (i = 0; i < 5000; ++i) {
    for (j = 0; j < k; ++j);
    --k;
  }
  return i;
}
testDecayingInnerLoop.expected = 5000;
test(testDecayingInnerLoop);

function testContinue() {
  var i;
  var total = 0;
  for (i = 0; i < 20; ++i) {
    if (i == 11)
	    continue;
    total++;
  }
  return total;
}
testContinue.expected = 19;
test(testContinue);

function testContinueWithLabel() {
  var i = 0;
  var j = 20;
checkiandj :
  while (i<10) {
    i+=1;
	checkj :
    while (j>10) {
	    j-=1;
	    if ((j%2)==0)
        continue checkj;
    }
  }
  return i + j;
}
testContinueWithLabel.expected = 20;
test(testContinueWithLabel);

function testDivision() {
  var a = 32768;
  var b;
  while (b !== 1) {
    b = a / 2;
    a = b;
  }
  return a;
}
testDivision.expected = 1;
test(testDivision);

function testDivisionFloat() {
  var a = 32768.0;
  var b;
  while (b !== 1) {
    b = a / 2.0;
    a = b;
  }
  return a === 1.0;
}
testDivisionFloat.expected = true;
test(testDivisionFloat);

function testToUpperToLower() {
  var s = "Hello", s1, s2;
  for (i = 0; i < 100; ++i) {
    s1 = s.toLowerCase();
    s2 = s.toUpperCase();
  }
  return s1 + s2;
}
testToUpperToLower.expected = "helloHELLO";
test(testToUpperToLower);

function testReplace2() {
  var s = "H e l l o", s1;
  for (i = 0; i < 100; ++i)
    s1 = s.replace(" ", "");
  return s1;
}
testReplace2.expected = "He l l o";
test(testReplace2);

function testBitwise() {
  var x = 10000;
  var y = 123456;
  var z = 987234;
  for (var i = 0; i < 50; i++) {
    x = x ^ y;
    y = y | z;
    z = ~x;
  }
  return x + y + z;
}
testBitwise.expected = -1298;
test(testBitwise);

function testSwitch() {
  var x = 0;
  var ret = 0;
  for (var i = 0; i < 100; ++i) {
    switch (x) {
    case 0:
      ret += 1;
      break;
    case 1:
      ret += 2;
      break;
    case 2:
      ret += 3;
      break;
    case 3:
      ret += 4;
      break;
    default:
      x = 0;
    }
    x++;
  }
  return ret;
}
testSwitch.expected = 226;
test(testSwitch);

function testSwitchString() {
  var x = "asdf";
  var ret = 0;
  for (var i = 0; i < 100; ++i) {
    switch (x) {
    case "asdf":
      x = "asd";
      ret += 1;
      break;
    case "asd":
      x = "as";
      ret += 2;
      break;
    case "as":
      x = "a";
      ret += 3;
      break;
    case "a":
      x = "foo";
      ret += 4;
      break;
    default:
      x = "asdf";
    }
  }
  return ret;
}
testSwitchString.expected = 200;
test(testSwitchString);

function testNegZero1Helper(z) {
  for (let j = 0; j < 5; ++j) { z = -z; }
  return Math.atan2(0, -0) == Math.atan2(0, z);
}

var testNegZero1 = function() { return testNegZero1Helper(0); }
  testNegZero1.expected = true;
testNegZero1.name = 'testNegZero1';
testNegZero1Helper(1);
test(testNegZero1);

// No test case, just make sure this doesn't assert.
function testNegZero2() {
  var z = 0;
  for (let j = 0; j < 5; ++j) { ({p: (-z)}); }
}
testNegZero2();

function testConstSwitch() {
  var x;
  for (var j=0;j<5;++j) { switch(1.1) { case NaN: case 2: } x = 2; }
  return x;
}
testConstSwitch.expected = 2;
test(testConstSwitch);

function testConstSwitch2() {
  var x;
  for (var j = 0; j < 4; ++j) { switch(0/0) { } }
  return "ok";
}
testConstSwitch2.expected = "ok";
test(testConstSwitch2);

function testConstIf() {
  var x;
  for (var j=0;j<5;++j) { if (1.1 || 5) { } x = 2;}
  return x;
}
testConstIf.expected = 2;
test(testConstIf);

function testTypeofHole() {
  var a = new Array(6);
  a[5] = 3;
  for (var i = 0; i < 6; ++i)
    a[i] = typeof a[i];
  return a.join(",");
}
testTypeofHole.expected = "undefined,undefined,undefined,undefined,undefined,number"
  test(testTypeofHole);

function testNativeLog() {
  var a = new Array(5);
  for (var i = 0; i < 5; i++) {
    a[i] = Math.log(Math.pow(Math.E, 10));
  }
  return a.join(",");
}
testNativeLog.expected = "10,10,10,10,10";
test(testNativeLog);

function test_JSOP_ARGSUB() {
  function f0() { return arguments[0]; }
  function f1() { return arguments[1]; }
  function f2() { return arguments[2]; }
  function f3() { return arguments[3]; }
  function f4() { return arguments[4]; }
  function f5() { return arguments[5]; }
  function f6() { return arguments[6]; }
  function f7() { return arguments[7]; }
  function f8() { return arguments[8]; }
  function f9() { return arguments[9]; }
  var a = [];
  for (var i = 0; i < 10; i++) {
    a[0] = f0('a');
    a[1] = f1('a','b');
    a[2] = f2('a','b','c');
    a[3] = f3('a','b','c','d');
    a[4] = f4('a','b','c','d','e');
    a[5] = f5('a','b','c','d','e','f');
    a[6] = f6('a','b','c','d','e','f','g');
    a[7] = f7('a','b','c','d','e','f','g','h');
    a[8] = f8('a','b','c','d','e','f','g','h','i');
    a[9] = f9('a','b','c','d','e','f','g','h','i','j');
  }
  return a.join("");
}
test_JSOP_ARGSUB.expected = "abcdefghij";
test(test_JSOP_ARGSUB);

function test_JSOP_ARGCNT() {
  function f0() { return arguments.length; }
  function f1() { return arguments.length; }
  function f2() { return arguments.length; }
  function f3() { return arguments.length; }
  function f4() { return arguments.length; }
  function f5() { return arguments.length; }
  function f6() { return arguments.length; }
  function f7() { return arguments.length; }
  function f8() { return arguments.length; }
  function f9() { return arguments.length; }
  var a = [];
  for (var i = 0; i < 10; i++) {
    a[0] = f0('a');
    a[1] = f1('a','b');
    a[2] = f2('a','b','c');
    a[3] = f3('a','b','c','d');
    a[4] = f4('a','b','c','d','e');
    a[5] = f5('a','b','c','d','e','f');
    a[6] = f6('a','b','c','d','e','f','g');
    a[7] = f7('a','b','c','d','e','f','g','h');
    a[8] = f8('a','b','c','d','e','f','g','h','i');
    a[9] = f9('a','b','c','d','e','f','g','h','i','j');
  }
  return a.join(",");
}
test_JSOP_ARGCNT.expected = "1,2,3,4,5,6,7,8,9,10";
test(test_JSOP_ARGCNT);

function testNativeMax() {
  var out = [], k;
  for (var i = 0; i < 5; ++i) {
    k = Math.max(k, i);
  }
  out.push(k);

  k = 0;
  for (var i = 0; i < 5; ++i) {
    k = Math.max(k, i);
  }
  out.push(k);

  for (var i = 0; i < 5; ++i) {
    k = Math.max(0, -0);
  }
  out.push((1 / k) < 0);
  return out.join(",");
}
testNativeMax.expected = "NaN,4,false";
test(testNativeMax);

function testFloatArrayIndex() {
  var a = [];
  for (var i = 0; i < 10; ++i) {
    a[3] = 5;
    a[3.5] = 7;
  }
  return a[3] + "," + a[3.5];
}
testFloatArrayIndex.expected = "5,7";
test(testFloatArrayIndex);

function testStrict() {
  var n = 10, a = [];
  for (var i = 0; i < 10; ++i) {
    a[0] = (n === 10);
    a[1] = (n !== 10);
    a[2] = (n === null);
    a[3] = (n == null);
  }
  return a.join(",");
}
testStrict.expected = "true,false,false,false";
test(testStrict);

function testSetPropNeitherMissNorHit() {
  for (var j = 0; j < 5; ++j) { if (({}).__proto__ = 1) { } }
  return "ok";
}
testSetPropNeitherMissNorHit.expected = "ok";
test(testSetPropNeitherMissNorHit);

function testPrimitiveConstructorPrototype() {
  var f = function(){};
  f.prototype = false;
  for (let j=0;j<5;++j) { new f; }
  return "ok";
}
testPrimitiveConstructorPrototype.expected = "ok";
test(testPrimitiveConstructorPrototype);

function testSideExitInConstructor() {
  var FCKConfig = {};
  FCKConfig.CoreStyles =
    {
	    'Bold': { },
	    'Italic': { },
	    'FontFace': { },
	    'Size' :
	    {
      Overrides: [ ]
	    },

	    'Color' :
	    {
      Element: '',
      Styles: {  },
      Overrides: [  ]
	    },
	    'BackColor': {
      Element : '',
      Styles : { 'background-color' : '' }
	    },

    };
  var FCKStyle = function(A) {
    A.Element;
  };

  var pass = true;
  for (var s in FCKConfig.CoreStyles) {
    var x = new FCKStyle(FCKConfig.CoreStyles[s]);
    if (!x)
      pass = false;
  }
  return pass;
}
testSideExitInConstructor.expected = true;
test(testSideExitInConstructor);

function testNot() {
  var a = new Object(), b = null, c = "foo", d = "", e = 5, f = 0, g = 5.5, h = -0, i = true, j = false, k = undefined;
  var r;
  for (var i = 0; i < 10; ++i)
    r = [!a, !b, !c, !d, !e, !f, !g, !h, !i, !j, !k];
  return r.join(",");
}
testNot.expected = "false,true,false,true,false,true,false,true,false,true,true";
test(testNot);

function doTestDifferingArgc(a, b)
{
  var k = 0;
  for (var i = 0; i < 10; i++)
  {
    k += i;
  }
  return k;
}
function testDifferingArgc()
{
  var x = 0;
  x += doTestDifferingArgc(1, 2);
  x += doTestDifferingArgc(1);
  x += doTestDifferingArgc(1, 2, 3);
  return x;
}
testDifferingArgc.expected = 45*3;
test(testDifferingArgc);

function doTestMoreArgcThanNargs()
{
  var x = 0;
  for (var i = 0; i < 10; i++)
  {
    x = x + arguments[3];
  }
  return x;
}
function testMoreArgcThanNargs()
{
  return doTestMoreArgcThanNargs(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
}
testMoreArgcThanNargs.expected = 4*10;
test(testMoreArgcThanNargs);

// Test stack reconstruction after a nested exit
function testNestedExitStackInner(j, counter) {
  ++counter;
  var b = 0;
  for (var i = 1; i <= RUNLOOP; i++) {
    ++b;
    var a;
    // Make sure that once everything has been traced we suddenly switch to
    // a different control flow the first time we run the outermost tree,
    // triggering a side exit.
    if (j < RUNLOOP)
      a = 1;
    else
      a = 0;
    ++b;
    b += a;
  }
  return counter + b;
}
function testNestedExitStackOuter() {
  var counter = 0;
  for (var j = 1; j <= RUNLOOP; ++j) {
    for (var k = 1; k <= RUNLOOP; ++k) {
      counter = testNestedExitStackInner(j, counter);
    }
  }
  return counter;
}
testNestedExitStackOuter.expected = 81;
test(testNestedExitStackOuter);

function testHOTLOOPSize() {
  return HOTLOOP > 1;
}
testHOTLOOPSize.expected = true;
test(testHOTLOOPSize);

function testMatchStringObject() {
  var a = new String("foo");
  var b;
  for (i = 0; i < 300; i++)
    b = a.match(/bar/);
  return b;
}
testMatchStringObject.expected = null;
test(testMatchStringObject);

function innerSwitch(k)
{
  var m = 0;

  switch (k)
  {
  case 0:
    m = 1;
    break;
  }

  return m;
}
function testInnerSwitchBreak()
{
  var r = new Array(5);
  for (var i = 0; i < 5; i++)
  {
    r[i] = innerSwitch(0);
  }

  return r.join(",");
}
testInnerSwitchBreak.expected = "1,1,1,1,1";
test(testInnerSwitchBreak);

function testArrayNaNIndex()
{
  for (var j = 0; j < 4; ++j) { [this[NaN]]; }
  for (var j = 0; j < 5; ++j) { if([1][-0]) { } }
  return "ok";
}
testArrayNaNIndex.expected = "ok";
test(testArrayNaNIndex);

function innerTestInnerMissingArgs(a,b,c,d)
{
  if (a) {
  } else {
  }
}
function doTestInnerMissingArgs(k)
{
  for (i = 0; i < 10; i++) {
    innerTestInnerMissingArgs(k);
  }
}
function testInnerMissingArgs()
{
  doTestInnerMissingArgs(1);
  doTestInnerMissingArgs(0);
  return 1;
}
testInnerMissingArgs.expected = 1;  //Expected: that we don't crash.
test(testInnerMissingArgs);

function regexpLastIndex()
{
  var n = 0;
  var re = /hi/g;
  var ss = " hi hi hi hi hi hi hi hi hi hi";
  for (var i = 0; i < 10; i++) {
    // re.exec(ss);
    n += (re.lastIndex > 0) ? 3 : 0;
    re.lastIndex = 0;
  }
  return n;
}
regexpLastIndex.expected = 0; // 30;
test(regexpLastIndex);

function testHOTLOOPCorrectness() {
  var b = 0;
  for (var i = 0; i < HOTLOOP; ++i)
    ++b;
  return b;
}
testHOTLOOPCorrectness.expected = HOTLOOP;
testHOTLOOPCorrectness.jitstats = {
recorderStarted: 1,
recorderAborted: 0,
traceTriggered: 0
};
// Change the global shape right before doing the test
this.testHOTLOOPCorrectnessVar = 1;
test(testHOTLOOPCorrectness);

function testRUNLOOPCorrectness() {
  var b = 0;
  for (var i = 0; i < RUNLOOP; ++i) {
    ++b;
  }
  return b;
}
testRUNLOOPCorrectness.expected = RUNLOOP;
testRUNLOOPCorrectness.jitstats = {
recorderStarted: 1,
recorderAborted: 0,
traceTriggered: 1
};
// Change the global shape right before doing the test
this.testRUNLOOPCorrectnessVar = 1;
test(testRUNLOOPCorrectness);

function testDateNow() {
  // Accessing global.Date for the first time will change the global shape,
  // so do it before the loop starts; otherwise we have to loop an extra time
  // to pick things up.
  var time = Date.now();
  for (var j = 0; j < RUNLOOP; ++j)
    time = Date.now();
  return "ok";
}
testDateNow.expected = "ok";
testDateNow.jitstats = {
recorderStarted: 1,
recorderAborted: 0,
traceTriggered: 1
};
test(testDateNow);

function testINITELEM()
{
  var x;
  for (var i = 0; i < 10; ++i)
    x = { 0: 5, 1: 5 };
  return x[0] + x[1];
}
testINITELEM.expected = 10;
test(testINITELEM);

function testUndefinedBooleanCmp()
{
  var t = true, f = false, x = [];
  for (var i = 0; i < 10; ++i) {
    x[0] = t == undefined;
    x[1] = t != undefined;
    x[2] = t === undefined;
    x[3] = t !== undefined;
    x[4] = t < undefined;
    x[5] = t > undefined;
    x[6] = t <= undefined;
    x[7] = t >= undefined;
    x[8] = f == undefined;
    x[9] = f != undefined;
    x[10] = f === undefined;
    x[11] = f !== undefined;
    x[12] = f < undefined;
    x[13] = f > undefined;
    x[14] = f <= undefined;
    x[15] = f >= undefined;
  }
  return x.join(",");
}
testUndefinedBooleanCmp.expected = "false,true,false,true,false,false,false,false,false,true,false,true,false,false,false,false";
test(testUndefinedBooleanCmp);

function testConstantBooleanExpr()
{
  for (var j = 0; j < 3; ++j) { if(true <= true) { } }
  return "ok";
}
testConstantBooleanExpr.expected = "ok";
test(testConstantBooleanExpr);

function testNegativeGETELEMIndex()
{
  for (let i=0;i<3;++i) /x/[-4];
  return "ok";
}
testNegativeGETELEMIndex.expected = "ok";
test(testNegativeGETELEMIndex);

function doTestInvalidCharCodeAt(input)
{
  var q = "";
  for (var i = 0; i < 10; i++)
    q += input.charCodeAt(i);
  return q;
}
function testInvalidCharCodeAt()
{
  return doTestInvalidCharCodeAt("");
}
testInvalidCharCodeAt.expected = "NaNNaNNaNNaNNaNNaNNaNNaNNaNNaN";
test(testInvalidCharCodeAt);

function FPQuadCmp()
{
  for (let j = 0; j < 3; ++j) { true == 0; }
  return "ok";
}
FPQuadCmp.expected = "ok";
test(FPQuadCmp);

function testDestructuring() {
  var t = 0;
  for (var i = 0; i < HOTLOOP + 1; ++i) {
    var [r, g, b] = [1, 1, 1];
    t += r + g + b;
  }
  return t
    }
testDestructuring.expected = (HOTLOOP + 1) * 3;
test(testDestructuring);

function loopWithUndefined1(t, val) {
  var a = new Array(6);
  for (var i = 0; i < 6; i++)
    a[i] = (t > val);
  return a;
}
loopWithUndefined1(5.0, 2);     //compile version with val=int

function testLoopWithUndefined1() {
  return loopWithUndefined1(5.0).join(",");  //val=undefined
};
testLoopWithUndefined1.expected = "false,false,false,false,false,false";
test(testLoopWithUndefined1);

function loopWithUndefined2(t, dostuff, val) {
  var a = new Array(6);
  for (var i = 0; i < 6; i++) {
    if (dostuff) {
      val = 1; 
      a[i] = (t > val);
    } else {
      a[i] = (val == undefined);
    }
  }
  return a;
}
function testLoopWithUndefined2() {
  var a = loopWithUndefined2(5.0, true, 2);
  var b = loopWithUndefined2(5.0, true);
  var c = loopWithUndefined2(5.0, false, 8);
  var d = loopWithUndefined2(5.0, false);
  return [a[0], b[0], c[0], d[0]].join(",");
}
testLoopWithUndefined2.expected = "true,true,false,true";
test(testLoopWithUndefined2);

//test no multitrees assert
function testBug462388() {
  var c = 0, v; for each (let x in ["",v,v,v]) { for (c=0;c<4;++c) { } }
  return true;
}
testBug462388.expected = true;
test(testBug462388);

//test no multitrees assert
function testBug462407() {
  for each (let i in [0, {}, 0, 1.5, {}, 0, 1.5, 0, 0]) { }
  return true;
}
testBug462407.expected = true;
test(testBug462407);

//test no multitrees assert
function testBug463490() {
  function f(a, b, d) {
    for (var i = 0; i < 10; i++) {
      if (d)
        b /= 2;
    }
    return a + b;
  }
  //integer stable loop
  f(2, 2, false);
  //double stable loop
  f(3, 4.5, false);
  //integer unstable branch
  f(2, 2, true);
  return true;
};
testBug463490.expected = true;
test(testBug463490);

// Test no assert (bug 464089)
function shortRecursiveLoop(b, c) {
  for (var i = 0; i < c; i++) {
    if (b)
      shortRecursiveLoop(c - 1);
  }
}
function testClosingRecursion() {
  shortRecursiveLoop(false, 1);
  shortRecursiveLoop(true, 3);
  return true;
}
testClosingRecursion.expected = true;
test(testClosingRecursion);

// Test no assert or crash from outer recorders (bug 465145)
function testBug465145() {
	this.__defineSetter__("x", function(){});
	this.watch("x", function(){});
	y = this;
	for (var z = 0; z < 2; ++z) { x = y };
	this.__defineSetter__("x", function(){});
	for (var z = 0; z < 2; ++z) { x = y };
}

function testTrueShiftTrue() {
  var a = new Array(5);
  for (var i=0;i<5;++i) a[i] = "" + (true << true);
  return a.join(",");
}
testTrueShiftTrue.expected = "2,2,2,2,2";
test(testTrueShiftTrue);

// Test no assert or crash
function testBug465261() {
  for (let z = 0; z < 2; ++z) { for each (let x in [0, true, (void 0), 0, (void
                                                                           0)]) { if(x){} } };
  return true;
}
testBug465261.expected = true;
test(testBug465261);

function testBug465272() {
  var a = new Array(5);
  for (j=0;j<5;++j) a[j] = "" + ((5) - 2);
  return a.join(",");
}
testBug465272.expected = "3,3,3,3,3"
  test(testBug465272);

function testBug465483() {
	var a = new Array(4);
	var c = 0;
	for each (i in [4, 'a', 'b', (void 0)]) a[c++] = '' + (i + i);
	return a.join(',');
}
testBug465483.expected = '8,aa,bb,NaN';
test(testBug465483);

function testNullCallee() {
  try {
    function f() {
      var x = new Array(5);
      for (var i = 0; i < 5; i++)
        x[i] = a[i].toString();
      return x.join(',');
    }
    f([[1],[2],[3],[4],[5]]);
    f([null, null, null, null, null]);
  } catch (e) {
    return true;
  }
  return false;
}
testNullCallee.expected = true;
test(testNullCallee);

//test no multitrees assert
function testBug466128() {
    for (let a = 0; a < 3; ++a) {
      for each (let b in [1, 2, "three", 4, 5, 6, 7, 8]) {
      }
    }
    return true;
}
testBug466128.expected = true;
test(testBug466128);

//test no assert
function testBug465688() {
    for each (let d in [-0x80000000, -0x80000000]) - -d;
    return true;
}
testBug465688.expected = true;
test(testBug465688);

//test no assert
function testBug466262() {
    var e = 1;
    for (var d = 0; d < 3; ++d) {
      if (d == 2) {
        e = "";
      }
    }
    return true;
}
testBug466262.expected = true;
test(testBug466262);

function testNewDate()
{
  // Accessing global.Date for the first time will change the global shape,
  // so do it before the loop starts; otherwise we have to loop an extra time
  // to pick things up.
  var start = new Date();
  var time = new Date();
  for (var j = 0; j < RUNLOOP; ++j)
    time = new Date();
  return time > 0 && time >= start;
}
testNewDate.expected = true;
testNewDate.jitstats = {
recorderStarted: 1,
recorderAborted: 0,
traceTriggered: 1
};
test(testNewDate);

function testArrayPushPop() {
  var a = [], sum1 = 0, sum2 = 0;
  for (var i = 0; i < 10; ++i)
    sum1 += a.push(i);
  for (var i = 0; i < 10; ++i)
    sum2 += a.pop();
  a.push(sum1);
  a.push(sum2);
  return a.join(",");
}
testArrayPushPop.expected = "55,45";
test(testArrayPushPop);

function testSlowArrayPop() {
    var a = [];
    for (var i = 0; i < RUNLOOP; i++)
        a[i] = [0];
    a[RUNLOOP-1].__defineGetter__("0", function () { return 'xyzzy'; });

    var last;
    for (var i = 0; i < RUNLOOP; i++)
        last = a[i].pop();  // reenters interpreter in getter
    return last;
}
testSlowArrayPop.expected = 'xyzzy';
test(testSlowArrayPop);

// Same thing but it needs to reconstruct multiple stack frames (so,
// multiple functions called inside the loop)
function testSlowArrayPopMultiFrame() {    
    var a = [];
    for (var i = 0; i < RUNLOOP; i++)
        a[i] = [0];
    a[RUNLOOP-1].__defineGetter__("0", function () { return 23; });

    function child(a, i) {
        return a[i].pop();  // reenters interpreter in getter
    }
    function parent(a, i) {
        return child(a, i);
    }
    function gramps(a, i) { 
        return parent(a, i);
    }

    var last;
    for (var i = 0; i < RUNLOOP; i++)
        last = gramps(a, i);
    return last;
}
testSlowArrayPopMultiFrame.expected = 23;
test(testSlowArrayPopMultiFrame);

// Same thing but nested trees, each reconstructing one or more stack frames 
// (so, several functions with loops, such that the loops end up being
// nested though they are not lexically nested)

function testSlowArrayPopNestedTrees() {    
    var a = [];
    for (var i = 0; i < RUNLOOP; i++)
        a[i] = [0];
    a[RUNLOOP-1].__defineGetter__("0", function () { return 3.14159 });

    function child(a, i, j, k) {
        var last = 2.71828;
        for (var l = 0; l < RUNLOOP; l++)
            if (i == RUNLOOP-1 && j == RUNLOOP-1 && k == RUNLOOP-1)
                last = a[l].pop();  // reenters interpreter in getter
        return last;
    }
    function parent(a, i, j) {
        var last;
        for (var k = 0; k < RUNLOOP; k++)
            last = child(a, i, j, k);
        return last;
    }
    function gramps(a, i) { 
        var last;
        for (var j = 0; j < RUNLOOP; j++)
            last = parent(a, i, j);
        return last;
    }

    var last;
    for (var i = 0; i < RUNLOOP; i++)
        last = gramps(a, i);
    return last;
}
testSlowArrayPopNestedTrees.expected = 3.14159;
test(testSlowArrayPopNestedTrees);

function testResumeOp() {
  var a = [1,"2",3,"4",5,"6",7,"8",9,"10",11,"12",13,"14",15,"16"];
  var x = "";
  while (a.length > 0)
    x += a.pop();
  return x;
}
testResumeOp.expected = "16151413121110987654321";
test(testResumeOp);

function testUndefinedCmp() {
  var a = false;
  for (var j = 0; j < 4; ++j) { if (undefined < false) { a = true; } }
  return a;
}
testUndefinedCmp.expected = false;
test(testUndefinedCmp);

function reallyDeepNestedExit(schedule)
{
  var c = 0, j = 0;
  for (var i = 0; i < 5; i++) {
    for (j = 0; j < 4; j++) {
      c += (schedule[i*4 + j] == 1) ? 1 : 2;
    }
  }
  return c;
}
function testReallyDeepNestedExit()
{
  var c = 0;
  var schedule1 = new Array(5*4);
  var schedule2 = new Array(5*4);
  for (var i = 0; i < 5*4; i++) {
    schedule1[i] = 0;
    schedule2[i] = 0;
  }
  /**
   * First innermost compile: true branch runs through.
   * Second '': false branch compiles new loop edge.
   * First outer compile: expect true branch.
   * Second '': hit false branch.
   */
  schedule1[0*4 + 3] = 1;
  var schedules = [schedule1,
                   schedule2,
                   schedule1,
                   schedule2,
                   schedule2];

  for (var i = 0; i < 5; i++) {
    c += reallyDeepNestedExit(schedules[i]);
  }
  return c;
}
testReallyDeepNestedExit.expected = 198;
test(testReallyDeepNestedExit);

function testRegExpTest() {
  var r = /abc/;
  var flag = false;
  for (var i = 0; i < 10; ++i)
    flag = r.test("abc");
  return flag;
}
testRegExpTest.expected = true;
test(testRegExpTest);

function testNumToString() {
  var r = [];
  var d = 123456789;
  for (var i = 0; i < 10; ++i) {
    r = [
      d.toString(),
      (-d).toString(),
      d.toString(10),
      (-d).toString(10),
      d.toString(16),
      (-d).toString(16),
      d.toString(36),
      (-d).toString(36)
      ];
  }
  return r.join(",");
}
testNumToString.expected = "123456789,-123456789,123456789,-123456789,75bcd15,-75bcd15,21i3v9,-21i3v9";
test(testNumToString);

function testLongNumToString() {
    var s;
    for (var i = 0; i < 5; i++)
        s = (0x08000000).toString(2);
    return s;
}
testLongNumToString.expected = '1000000000000000000000000000';
test(testLongNumToString);

function testSubstring() {
  for (var i = 0; i < 5; ++i) {
    actual = "".substring(5);
  }
  return actual;
}
testSubstring.expected = "";
test(testSubstring);

function testForInLoopChangeIteratorType() {
  for(y in [0,1,2]) y = NaN;
  (function(){
    [].__proto__.u = void 0;
    for (let y in [5,6,7,8])
      y = NaN;
    delete [].__proto__.u;
  })()
    return "ok";
}
testForInLoopChangeIteratorType.expected = "ok";
test(testForInLoopChangeIteratorType);

function testGrowDenseArray() {
  var a = new Array();
  for (var i = 0; i < 10; ++i)
    a[i] |= 5;
  return a.join(",");
}
testGrowDenseArray.expected = "5,5,5,5,5,5,5,5,5,5";
test(testGrowDenseArray);

function testCallProtoMethod() {
  function X() { this.x = 1; }
  X.prototype.getName = function () { return "X"; }

  function Y() { this.x = 2; }
  Y.prototype.getName = function() "Y";

  var a = [new X, new X, new X, new X, new Y];
  var s = '';
  for (var i = 0; i < a.length; i++)
    s += a[i].getName();
  return s;
}
testCallProtoMethod.expected = 'XXXXY';
test(testCallProtoMethod);

function testTypeUnstableForIn() {
  var a = [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16];
  var x = 0;
  for (var i in a) {
    i = parseInt(i);
    x++;
  }
  return x;
}
testTypeUnstableForIn.expected = 16;
test(testTypeUnstableForIn);

function testAddUndefined() {
  for (var j = 0; j < 3; ++j)
    (0 + void 0) && 0;
}
test(testAddUndefined);

function testStringify() {
  var t = true, f = false, u = undefined, n = 5, d = 5.5, s = "x";
  var a = [];
  for (var i = 0; i < 10; ++i) {
    a[0] = "" + t;
    a[1] = t + "";
    a[2] = "" + f;
    a[3] = f + "";
    a[4] = "" + u;
    a[5] = u + "";
    a[6] = "" + n;
    a[7] = n + "";
    a[8] = "" + d;
    a[9] = d + "";
    a[10] = "" + s;
    a[11] = s + "";
  }
  return a.join(",");
}
testStringify.expected = "true,true,false,false,undefined,undefined,5,5,5.5,5.5,x,x";
test(testStringify);

function testObjectToString() {
  var o = {toString: function()"foo"};
  var s = "";
  for (var i = 0; i < 10; i++)
    s += o;
  return s;
}
testObjectToString.expected = "foofoofoofoofoofoofoofoofoofoo";
test(testObjectToString);

function testObjectToNumber() {
  var o = {valueOf: function()-3};
  var x = 0;
  for (var i = 0; i < 10; i++)
    x -= o;
  return x;
}
testObjectToNumber.expected = 30;
test(testObjectToNumber);

function my_iterator_next() {
  if (this.i == 10) {
    this.i = 0;
    throw this.StopIteration;
  }
  return this.i++;
}
function testCustomIterator() {
  var o = {
  __iterator__: function () {
      return {
      i: 0,
      next: my_iterator_next,
      StopIteration: StopIteration
      };
    }
  };
  var a=[];
  for (var k = 0; k < 100; k += 10) {
    for(var j in o) {
      a[k + (j >> 0)] = j*k;
    }
  }
  return a.join();
}
testCustomIterator.expected = "0,0,0,0,0,0,0,0,0,0,0,10,20,30,40,50,60,70,80,90,0,20,40,60,80,100,120,140,160,180,0,30,60,90,120,150,180,210,240,270,0,40,80,120,160,200,240,280,320,360,0,50,100,150,200,250,300,350,400,450,0,60,120,180,240,300,360,420,480,540,0,70,140,210,280,350,420,490,560,630,0,80,160,240,320,400,480,560,640,720,0,90,180,270,360,450,540,630,720,810";
test(testCustomIterator);

function bug464403() {
  print(8);
  var u = [print, print, function(){}]
    for each (x in u) for (u.e in [1,1,1,1]);
  return "ok";
}
bug464403.expected = "ok";
test(bug464403);

function testBoxDoubleWithDoubleSizedInt()
{
  var i = 0;
  var a = new Array(3);

  while (i < a.length)
    a[i++] = 0x5a827999;
  return a.join(",");
}
testBoxDoubleWithDoubleSizedInt.expected = "1518500249,1518500249,1518500249";
test(testBoxDoubleWithDoubleSizedInt);

function testObjectOrderedCmp()
{
  var a = new Array(5);
  for(var i=0;i<5;++i) a[i] = ({} < {});
  return a.join(",");
}
testObjectOrderedCmp.expected = "false,false,false,false,false";
test(testObjectOrderedCmp);

function testObjectOrderedCmp2()
{
  var a = new Array(5);
  for(var i=0;i<5;++i) a[i] = ("" <= null);
  return a.join(",");
}
testObjectOrderedCmp2.expected = "true,true,true,true,true";
test(testObjectOrderedCmp2);

function testLogicalNotNaN() {
  var i = 0;
  var a = new Array(5);
  while (i < a.length)
    a[i++] = !NaN;
  return a.join();
}
testLogicalNotNaN.expected = "true,true,true,true,true";
test(testLogicalNotNaN);

function testStringToInt32() {
  var s = "";
  for (let j = 0; j < 5; ++j) s += ("1e+81" ^  3);
  return s;
}
testStringToInt32.expected = "33333";
test(testStringToInt32);

function testIn() {
  var array = [3];
  var obj = { "-1": 5, "1.7": 3, "foo": 5, "1": 7 };
  var a = [];
  for (let j = 0; j < 5; ++j) {
    a.push("0" in array);
    a.push(-1 in obj);
    a.push(1.7 in obj);
    a.push("foo" in obj);
    a.push(1 in obj);
    a.push("1" in array);  
    a.push(-2 in obj);
    a.push(2.7 in obj);
    a.push("bar" in obj);
    a.push(2 in obj);    
  }
  return a.join(",");
}
testIn.expected = "true,true,true,true,true,false,false,false,false,false,true,true,true,true,true,false,false,false,false,false,true,true,true,true,true,false,false,false,false,false,true,true,true,true,true,false,false,false,false,false,true,true,true,true,true,false,false,false,false,false";
test(testIn);

function testBranchCse() {
  empty = [];
  out = [];
  for (var j=0;j<10;++j) { empty[42]; out.push((1 * (1)) | ""); }
  return out.join(",");
}
testBranchCse.expected = "1,1,1,1,1,1,1,1,1,1";
test(testBranchCse);

function testMulOverflow() {
  var a = [];
  for (let j=0;j<5;++j) a.push(0 | ((0x60000009) * 0x60000009));
  return a.join(",");
}
testMulOverflow.expected = "-1073741824,-1073741824,-1073741824,-1073741824,-1073741824";
test(testMulOverflow);

function testThinLoopDemote() {
  function f()
  {
    var k = 1;
    for (var n = 0; n < 4; n++) {
      k = (k * 10);
    }
    return k;
  }
  f();
  return f();
}
testThinLoopDemote.expected = 10000;
testThinLoopDemote.jitstats = {
recorderStarted: 2,
recorderAborted: 0,
traceCompleted: 2,
traceTriggered: 2,
unstableLoopVariable: 1
};
test(testThinLoopDemote);

var global = this;
function testWeirdDateParseOuter()
{
  var vDateParts = ["11", "17", "2008"];
  var out = [];
  for (var vI = 0; vI < vDateParts.length; vI++)
    out.push(testWeirdDateParseInner(vDateParts[vI]));
  /* Mutate the global shape so we fall off trace; this causes
   * additional oddity */
  global.x = Math.random();
  return out;
} 
function testWeirdDateParseInner(pVal)
{
  var vR = 0;
  for (var vI = 0; vI < pVal.length; vI++) {
    var vC = pVal.charAt(vI);
    if ((vC >= '0') && (vC <= '9'))
	    vR = (vR * 10) + parseInt(vC);
  }
  return vR;
}
function testWeirdDateParse() {
  var result = [];
  result.push(testWeirdDateParseInner("11"));
  result.push(testWeirdDateParseInner("17"));
  result.push(testWeirdDateParseInner("2008"));
  result.push(testWeirdDateParseInner("11"));
  result.push(testWeirdDateParseInner("17"));
  result.push(testWeirdDateParseInner("2008"));
  result = result.concat(testWeirdDateParseOuter());
  result = result.concat(testWeirdDateParseOuter());
  result.push(testWeirdDateParseInner("11"));
  result.push(testWeirdDateParseInner("17"));
  result.push(testWeirdDateParseInner("2008"));
  return result.join(",");
}
testWeirdDateParse.expected = "11,17,2008,11,17,2008,11,17,2008,11,17,2008,11,17,2008";
testWeirdDateParse.jitstats = {
recorderStarted: 8,
recorderAborted: 1,
traceCompleted: 7,
traceTriggered: 14,
unstableLoopVariable: 3,
noCompatInnerTrees: 1
};
test(testWeirdDateParse);

function testUndemotableBinaryOp() {
  var out = [];
  for (let j = 0; j < 5; ++j) { out.push(6 - ((void 0) ^ 0x80000005)); }
  return out.join(",");
}
testUndemotableBinaryOp.expected = "2147483649,2147483649,2147483649,2147483649,2147483649";
test(testUndemotableBinaryOp);

function testNullRelCmp() {
  var out = [];
  for(j=0;j<3;++j) { out.push(3 > null); out.push(3 < null); out.push(0 == null); out.push(3 == null); }
  return out.join(",");
}
testNullRelCmp.expected = "true,false,false,false,true,false,false,false,true,false,false,false";
test(testNullRelCmp);

function testEqFalseEmptyString() {
  var x = [];
  for (var i=0;i<5;++i) x.push(false == "");
  return x.join(",");
}
testEqFalseEmptyString.expected = "true,true,true,true,true";
test(testEqFalseEmptyString);

function testIncDec2(ii) {
  var x = [];
  for (let j=0;j<5;++j) { 
    ii=j;
    jj=j; 
    var kk=j; 
    x.push(ii--);
    x.push(jj--); 
    x.push(kk--); 
    x.push(++ii);
    x.push(++jj); 
    x.push(++kk); 
  }
  return x.join(",");
}
function testIncDec() {
  return testIncDec2(0);
}
testIncDec.expected = "0,0,0,0,0,0,1,1,1,1,1,1,2,2,2,2,2,2,3,3,3,3,3,3,4,4,4,4,4,4";
test(testIncDec);

function testApply() {
    var q = [];
    for (var i = 0; i < 10; ++i)
        Array.prototype.push.apply(q, [5]);
    return q.join(",");
}
testApply.expected = "5,5,5,5,5,5,5,5,5,5";
test(testApply);

function testNestedForIn() {
    var a = {x: 1, y: 2, z: 3};
    var s = '';
    for (var p1 in a)
        for (var p2 in a)
            s += p1 + p2 + ' ';
    return s;
}
testNestedForIn.expected = 'xx xy xz yx yy yz zx zy zz ';
test(testNestedForIn);

function testForEach() {
    var r;
    var a = ["zero", "one", "two", "three"];
    for (var i = 0; i < RUNLOOP; i++) {
        r = "";
        for each (var s in a)
            r += s + " ";
    }
    return r;
}
testForEach.expected = "zero one two three ";
test(testForEach);

function testThinForEach() {
    var a = ["red"];
    var n = 0;
    for (var i = 0; i < 10; i++)
        for each (var v in a)
            if (v)
                n++;
    return n;
}
testThinForEach.expected = 10;
test(testThinForEach);

function testComparisons()
{
  // All the special values from each of the types in
  // ECMA-262, 3rd ed. section 8
  var undefinedType, nullType, booleanType, stringType, numberType, objectType;

  var types = [];
  types[undefinedType = 0] = "Undefined";
  types[nullType = 1] = "Null";
  types[booleanType = 2] = "Boolean";
  types[stringType = 3] = "String";
  types[numberType = 4] = "Number";
  types[objectType = 5] = "Object";

  var JSVAL_INT_MIN = -Math.pow(2, 30);
  var JSVAL_INT_MAX = Math.pow(2, 30) - 1;

  // Values from every ES3 type, hitting all the edge-case and special values
  // that can be dreamed up
  var values =
    {
     "undefined":
       {
         value: function() { return undefined; },
         type: undefinedType
       },
     "null":
       {
         value: function() { return null; },
         type: nullType
       },
     "true":
       {
         value: function() { return true; },
         type: booleanType
       },
     "false":
       {
         value: function() { return false; },
         type: booleanType
       },
     '""':
       {
         value: function() { return ""; },
         type: stringType
       },
     '"a"':
       {
         // a > [, for string-object comparisons
         value: function() { return "a"; },
         type: stringType
       },
     '"Z"':
       {
         // Z < [, for string-object comparisons
         value: function() { return "Z"; },
         type: stringType
       },
     "0":
       {
         value: function() { return 0; },
         type: numberType
       },
     "-0":
       {
         value: function() { return -0; },
         type: numberType
       },
     "1":
       {
         value: function() { return 1; },
         type: numberType
       },
     "Math.E":
       {
         value: function() { return Math.E; },
         type: numberType
       },
     "JSVAL_INT_MIN - 1":
       {
         value: function() { return JSVAL_INT_MIN - 1; },
         type: numberType
       },
     "JSVAL_INT_MIN":
       {
         value: function() { return JSVAL_INT_MIN; },
         type: numberType
       },
     "JSVAL_INT_MIN + 1":
       {
         value: function() { return JSVAL_INT_MIN + 1; },
         type: numberType
       },
     "JSVAL_INT_MAX - 1":
       {
         value: function() { return JSVAL_INT_MAX - 1; },
         type: numberType
       },
     "JSVAL_INT_MAX":
       {
         value: function() { return JSVAL_INT_MAX; },
         type: numberType
       },
     "JSVAL_INT_MAX + 1":
       {
         value: function() { return JSVAL_INT_MAX + 1; },
         type: numberType
       },
     "Infinity":
       {
         value: function() { return Infinity; },
         type: numberType
       },
     "-Infinity":
       {
         value: function() { return -Infinity; },
         type: numberType
       },
     "NaN":
       {
         value: function() { return NaN; },
         type: numberType
       },
     "{}":
       {
         value: function() { return {}; },
         type: objectType
       },
     "{ valueOf: undefined }":
       {
         value: function() { return { valueOf: undefined }; },
         type: objectType
       },
     "[]":
       {
         value: function() { return []; },
         type: objectType
       },
     '[""]':
       {
         value: function() { return [""]; },
         type: objectType
       },
     '["a"]':
       {
         value: function() { return ["a"]; },
         type: objectType
       },
     "[0]":
       {
         value: function() { return [0]; },
         type: objectType
       }
    };

  var orderOps =
    {
     "<": function(a, b) { return a < b; },
     ">": function(a, b) { return a > b; },
     "<=": function(a, b) { return a <= b; },
     ">=": function(a, b) { return a >= b; }
    };
  var eqOps =
    {
     "==": function(a, b) { return a == b; },
     "!=": function(a, b) { return a != b; },
     "===": function(a, b) { return a === b; },
     "!==": function(a, b) { return a !== b; }
    };


  var notEqualIncomparable =
    {
      eq: { "==": false, "!=": true, "===": false, "!==": true },
      order: { "<": false, ">": false, "<=": false, ">=": false }
    };
  var notEqualLessThan =
    {
      eq: { "==": false, "!=": true, "===": false, "!==": true },
      order: { "<": true, ">": false, "<=": true, ">=": false }
    };
  var notEqualGreaterThan =
    {
      eq: { "==": false, "!=": true, "===": false, "!==": true },
      order: { "<": false, ">": true, "<=": false, ">=": true }
    };
  var notEqualNorDifferent =
    {
      eq: { "==": false, "!=": true, "===": false, "!==": true },
      order: { "<": false, ">": false, "<=": true, ">=": true }
    };
  var strictlyEqual =
    {
      eq: { "==": true, "!=": false, "===": true, "!==": false },
      order: { "<": false, ">": false, "<=": true, ">=": true }
    };
  var looselyEqual =
    {
      eq: { "==": true, "!=": false, "===": false, "!==": true },
      order: { "<": false, ">": false, "<=": true, ">=": true }
    };
  var looselyEqualNotDifferent =
    {
      eq: { "==": true, "!=": false, "===": false, "!==": true },
      order: { "<": false, ">": false, "<=": true, ">=": true }
    };
  var looselyEqualIncomparable =
    {
      eq: { "==": true, "!=": false, "===": false, "!==": true },
      order: { "<": false, ">": false, "<=": false, ">=": false }
    };
  var strictlyEqualNotDifferent =
    {
      eq: { "==": true, "!=": false, "===": true, "!==": false },
      order: { "<": false, ">": false, "<=": true, ">=": true }
    };
  var strictlyEqualIncomparable =
    {
      eq: { "==": true, "!=": false, "===": true, "!==": false },
      order: { "<": false, ">": false, "<=": false, ">=": false }
    };

  var comparingZeroToSomething =
    {
      "undefined": notEqualIncomparable,
      "null": notEqualNorDifferent,
      "true": notEqualLessThan,
      "false": looselyEqual,
      '""': looselyEqualNotDifferent,
      '"a"': notEqualIncomparable,
      '"Z"': notEqualIncomparable,
      "0": strictlyEqual,
      "-0": strictlyEqual,
      "1": notEqualLessThan,
      "Math.E": notEqualLessThan,
      "JSVAL_INT_MIN - 1": notEqualGreaterThan,
      "JSVAL_INT_MIN": notEqualGreaterThan,
      "JSVAL_INT_MIN + 1": notEqualGreaterThan,
      "JSVAL_INT_MAX - 1": notEqualLessThan,
      "JSVAL_INT_MAX": notEqualLessThan,
      "JSVAL_INT_MAX + 1": notEqualLessThan,
      "Infinity": notEqualLessThan,
      "-Infinity": notEqualGreaterThan,
      "NaN": notEqualIncomparable,
      "{}": notEqualIncomparable,
      "{ valueOf: undefined }": notEqualIncomparable,
      "[]": looselyEqual,
      '[""]': looselyEqual,
      '["a"]': notEqualIncomparable,
      "[0]": looselyEqual
    };

  var comparingObjectOrObjectWithValueUndefined =
    {
      "undefined": notEqualIncomparable,
      "null": notEqualIncomparable,
      "true": notEqualIncomparable,
      "false": notEqualIncomparable,
      '""': notEqualGreaterThan,
      '"a"': notEqualLessThan,
      '"Z"': notEqualGreaterThan,
      "0": notEqualIncomparable,
      "-0": notEqualIncomparable,
      "1": notEqualIncomparable,
      "Math.E": notEqualIncomparable,
      "JSVAL_INT_MIN - 1": notEqualIncomparable,
      "JSVAL_INT_MIN": notEqualIncomparable,
      "JSVAL_INT_MIN + 1": notEqualIncomparable,
      "JSVAL_INT_MAX - 1": notEqualIncomparable,
      "JSVAL_INT_MAX": notEqualIncomparable,
      "JSVAL_INT_MAX + 1": notEqualIncomparable,
      "Infinity": notEqualIncomparable,
      "-Infinity": notEqualIncomparable,
      "NaN": notEqualIncomparable,
      "{}": notEqualNorDifferent,
      "{ valueOf: undefined }": notEqualNorDifferent,
      "[]": notEqualGreaterThan,
      '[""]': notEqualGreaterThan,
      '["a"]': notEqualLessThan,
      "[0]": notEqualGreaterThan
    };

  // Constructed expected-value matrix
  var expected =
    {
     "undefined":
       {
         "undefined": strictlyEqualIncomparable,
         "null": looselyEqualIncomparable,
         "true": notEqualIncomparable,
         "false": notEqualIncomparable,
         '""': notEqualIncomparable,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualIncomparable,
         "-0": notEqualIncomparable,
         "1": notEqualIncomparable,
         "Math.E": notEqualIncomparable,
         "JSVAL_INT_MIN - 1": notEqualIncomparable,
         "JSVAL_INT_MIN": notEqualIncomparable,
         "JSVAL_INT_MIN + 1": notEqualIncomparable,
         "JSVAL_INT_MAX - 1": notEqualIncomparable,
         "JSVAL_INT_MAX": notEqualIncomparable,
         "JSVAL_INT_MAX + 1": notEqualIncomparable,
         "Infinity": notEqualIncomparable,
         "-Infinity": notEqualIncomparable,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualIncomparable,
         '[""]': notEqualIncomparable,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualIncomparable
       },
     "null":
       {
         "undefined": looselyEqualIncomparable,
         "null": strictlyEqualNotDifferent,
         "true": notEqualLessThan,
         "false": notEqualNorDifferent,
         '""': notEqualNorDifferent,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualNorDifferent,
         "-0": notEqualNorDifferent,
         "1": notEqualLessThan,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualNorDifferent,
         '[""]': notEqualNorDifferent,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualNorDifferent
       },
     "true":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualGreaterThan,
         "true": strictlyEqual,
         "false": notEqualGreaterThan,
         '""': notEqualGreaterThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualGreaterThan,
         "-0": notEqualGreaterThan,
         "1": looselyEqual,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualGreaterThan
       },
     "false":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualNorDifferent,
         "true": notEqualLessThan,
         "false": strictlyEqual,
         '""': looselyEqualNotDifferent,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": looselyEqual,
         "-0": looselyEqual,
         "1": notEqualLessThan,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": looselyEqual,
         '[""]': looselyEqual,
         '["a"]': notEqualIncomparable,
         "[0]": looselyEqual
       },
     '""':
       {
         "undefined": notEqualIncomparable,
         "null": notEqualNorDifferent,
         "true": notEqualLessThan,
         "false": looselyEqual,
         '""': strictlyEqual,
         '"a"': notEqualLessThan,
         '"Z"': notEqualLessThan,
         "0": looselyEqual,
         "-0": looselyEqual,
         "1": notEqualLessThan,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualLessThan,
         "{ valueOf: undefined }": notEqualLessThan,
         "[]": looselyEqual,
         '[""]': looselyEqual,
         '["a"]': notEqualLessThan,
         "[0]": notEqualLessThan
       },
     '"a"':
       {
         "undefined": notEqualIncomparable,
         "null": notEqualIncomparable,
         "true": notEqualIncomparable,
         "false": notEqualIncomparable,
         '""': notEqualGreaterThan,
         '"a"': strictlyEqual,
         '"Z"': notEqualGreaterThan,
         "0": notEqualIncomparable,
         "-0": notEqualIncomparable,
         "1": notEqualIncomparable,
         "Math.E": notEqualIncomparable,
         "JSVAL_INT_MIN - 1": notEqualIncomparable,
         "JSVAL_INT_MIN": notEqualIncomparable,
         "JSVAL_INT_MIN + 1": notEqualIncomparable,
         "JSVAL_INT_MAX - 1": notEqualIncomparable,
         "JSVAL_INT_MAX": notEqualIncomparable,
         "JSVAL_INT_MAX + 1": notEqualIncomparable,
         "Infinity": notEqualIncomparable,
         "-Infinity": notEqualIncomparable,
         "NaN": notEqualIncomparable,
         "{}": notEqualGreaterThan,
         "{ valueOf: undefined }": notEqualGreaterThan,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': looselyEqualNotDifferent,
         "[0]": notEqualGreaterThan
       },
     '"Z"':
       {
         "undefined": notEqualIncomparable,
         "null": notEqualIncomparable,
         "true": notEqualIncomparable,
         "false": notEqualIncomparable,
         '""': notEqualGreaterThan,
         '"a"': notEqualLessThan,
         '"Z"': strictlyEqual,
         "0": notEqualIncomparable,
         "-0": notEqualIncomparable,
         "1": notEqualIncomparable,
         "Math.E": notEqualIncomparable,
         "JSVAL_INT_MIN - 1": notEqualIncomparable,
         "JSVAL_INT_MIN": notEqualIncomparable,
         "JSVAL_INT_MIN + 1": notEqualIncomparable,
         "JSVAL_INT_MAX - 1": notEqualIncomparable,
         "JSVAL_INT_MAX": notEqualIncomparable,
         "JSVAL_INT_MAX + 1": notEqualIncomparable,
         "Infinity": notEqualIncomparable,
         "-Infinity": notEqualIncomparable,
         "NaN": notEqualIncomparable,
         "{}": notEqualLessThan,
         "{ valueOf: undefined }": notEqualLessThan,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': notEqualLessThan,
         "[0]": notEqualGreaterThan
       },
     "0": comparingZeroToSomething,
     "-0": comparingZeroToSomething,
     "1":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualGreaterThan,
         "true": looselyEqual,
         "false": notEqualGreaterThan,
         '""': notEqualGreaterThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualGreaterThan,
         "-0": notEqualGreaterThan,
         "1": strictlyEqual,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualGreaterThan
       },
     "Math.E":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualGreaterThan,
         "true": notEqualGreaterThan,
         "false": notEqualGreaterThan,
         '""': notEqualGreaterThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualGreaterThan,
         "-0": notEqualGreaterThan,
         "1": notEqualGreaterThan,
         "Math.E": strictlyEqual,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualGreaterThan
       },
     "JSVAL_INT_MIN - 1":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualLessThan,
         "true": notEqualLessThan,
         "false": notEqualLessThan,
         '""': notEqualLessThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualLessThan,
         "-0": notEqualLessThan,
         "1": notEqualLessThan,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": strictlyEqual,
         "JSVAL_INT_MIN": notEqualLessThan,
         "JSVAL_INT_MIN + 1": notEqualLessThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualLessThan,
         '[""]': notEqualLessThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualLessThan
       },
     "JSVAL_INT_MIN":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualLessThan,
         "true": notEqualLessThan,
         "false": notEqualLessThan,
         '""': notEqualLessThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualLessThan,
         "-0": notEqualLessThan,
         "1": notEqualLessThan,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": strictlyEqual,
         "JSVAL_INT_MIN + 1": notEqualLessThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualLessThan,
         '[""]': notEqualLessThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualLessThan
       },
     "JSVAL_INT_MIN + 1":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualLessThan,
         "true": notEqualLessThan,
         "false": notEqualLessThan,
         '""': notEqualLessThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualLessThan,
         "-0": notEqualLessThan,
         "1": notEqualLessThan,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": strictlyEqual,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualLessThan,
         '[""]': notEqualLessThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualLessThan
       },
     "JSVAL_INT_MAX - 1":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualGreaterThan,
         "true": notEqualGreaterThan,
         "false": notEqualGreaterThan,
         '""': notEqualGreaterThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualGreaterThan,
         "-0": notEqualGreaterThan,
         "1": notEqualGreaterThan,
         "Math.E": notEqualGreaterThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": strictlyEqual,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualGreaterThan
       },
     "JSVAL_INT_MAX":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualGreaterThan,
         "true": notEqualGreaterThan,
         "false": notEqualGreaterThan,
         '""': notEqualGreaterThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualGreaterThan,
         "-0": notEqualGreaterThan,
         "1": notEqualGreaterThan,
         "Math.E": notEqualGreaterThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualGreaterThan,
         "JSVAL_INT_MAX": strictlyEqual,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualGreaterThan
       },
     "JSVAL_INT_MAX + 1":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualGreaterThan,
         "true": notEqualGreaterThan,
         "false": notEqualGreaterThan,
         '""': notEqualGreaterThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualGreaterThan,
         "-0": notEqualGreaterThan,
         "1": notEqualGreaterThan,
         "Math.E": notEqualGreaterThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualGreaterThan,
         "JSVAL_INT_MAX": notEqualGreaterThan,
         "JSVAL_INT_MAX + 1": strictlyEqual,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualGreaterThan
       },
     "Infinity":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualGreaterThan,
         "true": notEqualGreaterThan,
         "false": notEqualGreaterThan,
         '""': notEqualGreaterThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualGreaterThan,
         "-0": notEqualGreaterThan,
         "1": notEqualGreaterThan,
         "Math.E": notEqualGreaterThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualGreaterThan,
         "JSVAL_INT_MAX": notEqualGreaterThan,
         "JSVAL_INT_MAX + 1": notEqualGreaterThan,
         "Infinity": strictlyEqual,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualGreaterThan
       },
     "-Infinity":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualLessThan,
         "true": notEqualLessThan,
         "false": notEqualLessThan,
         '""': notEqualLessThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualLessThan,
         "-0": notEqualLessThan,
         "1": notEqualLessThan,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualLessThan,
         "JSVAL_INT_MIN": notEqualLessThan,
         "JSVAL_INT_MIN + 1": notEqualLessThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": strictlyEqual,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualLessThan,
         '[""]': notEqualLessThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualLessThan
       },
     "NaN":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualIncomparable,
         "true": notEqualIncomparable,
         "false": notEqualIncomparable,
         '""': notEqualIncomparable,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualIncomparable,
         "-0": notEqualIncomparable,
         "1": notEqualIncomparable,
         "Math.E": notEqualIncomparable,
         "JSVAL_INT_MIN - 1": notEqualIncomparable,
         "JSVAL_INT_MIN": notEqualIncomparable,
         "JSVAL_INT_MIN + 1": notEqualIncomparable,
         "JSVAL_INT_MAX - 1": notEqualIncomparable,
         "JSVAL_INT_MAX": notEqualIncomparable,
         "JSVAL_INT_MAX + 1": notEqualIncomparable,
         "Infinity": notEqualIncomparable,
         "-Infinity": notEqualIncomparable,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualIncomparable,
         '[""]': notEqualIncomparable,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualIncomparable
       },
     "{}": comparingObjectOrObjectWithValueUndefined,
     "{ valueOf: undefined }": comparingObjectOrObjectWithValueUndefined,
     "[]":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualNorDifferent,
         "true": notEqualLessThan,
         "false": looselyEqual,
         '""': looselyEqual,
         '"a"': notEqualLessThan,
         '"Z"': notEqualLessThan,
         "0": looselyEqual,
         "-0": looselyEqual,
         "1": notEqualLessThan,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualLessThan,
         "{ valueOf: undefined }": notEqualLessThan,
         "[]": notEqualNorDifferent,
         '[""]': notEqualNorDifferent,
         '["a"]': notEqualLessThan,
         "[0]": notEqualLessThan
       },
     '[""]':
       {
         "undefined": notEqualIncomparable,
         "null": notEqualNorDifferent,
         "true": notEqualLessThan,
         "false": looselyEqual,
         '""': looselyEqual,
         '"a"': notEqualLessThan,
         '"Z"': notEqualLessThan,
         "0": looselyEqual,
         "-0": looselyEqual,
         "1": notEqualLessThan,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualLessThan,
         "{ valueOf: undefined }": notEqualLessThan,
         "[]": notEqualNorDifferent,
         '[""]': notEqualNorDifferent,
         '["a"]': notEqualLessThan,
         "[0]": notEqualLessThan
       },
     '["a"]':
       {
         "undefined": notEqualIncomparable,
         "null": notEqualIncomparable,
         "true": notEqualIncomparable,
         "false": notEqualIncomparable,
         '""': notEqualGreaterThan,
         '"a"': looselyEqual,
         '"Z"': notEqualGreaterThan,
         "0": notEqualIncomparable,
         "-0": notEqualIncomparable,
         "1": notEqualIncomparable,
         "Math.E": notEqualIncomparable,
         "JSVAL_INT_MIN - 1": notEqualIncomparable,
         "JSVAL_INT_MIN": notEqualIncomparable,
         "JSVAL_INT_MIN + 1": notEqualIncomparable,
         "JSVAL_INT_MAX - 1": notEqualIncomparable,
         "JSVAL_INT_MAX": notEqualIncomparable,
         "JSVAL_INT_MAX + 1": notEqualIncomparable,
         "Infinity": notEqualIncomparable,
         "-Infinity": notEqualIncomparable,
         "NaN": notEqualIncomparable,
         "{}": notEqualGreaterThan,
         "{ valueOf: undefined }": notEqualGreaterThan,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': notEqualNorDifferent,
         "[0]": notEqualGreaterThan
       },
     "[0]":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualNorDifferent,
         "true": notEqualLessThan,
         "false": looselyEqual,
         '""': notEqualGreaterThan,
         '"a"': notEqualLessThan,
         '"Z"': notEqualLessThan,
         "0": looselyEqual,
         "-0": looselyEqual,
         "1": notEqualLessThan,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualLessThan,
         "{ valueOf: undefined }": notEqualLessThan,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': notEqualLessThan,
         "[0]": notEqualNorDifferent
       }
    };



  var failures = [];
  function fail(a, ta, b, tb, ex, ac, op)
  {
    failures.push("(" + a + " " + op + " " + b + ") wrong: " +
                  "expected " + ex + ", got " + ac +
                  " (types " + types[ta] + ", " + types[tb] + ")");
  }

  var result = false;
  for (var i in values)
  {
    for (var j in values)
    {
      // Constants, so hoist to help JIT know that
      var vala = values[i], valb = values[j];
      var a = vala.value(), b = valb.value();

      for (var opname in orderOps)
      {
        var op = orderOps[opname];
        var expect = expected[i][j].order[opname];
        var failed = false;

        for (var iter = 0; iter < 5; iter++)
        {
          result = op(a, b);
          failed = failed || result !== expect;
        }

        if (failed)
          fail(i, vala.type, j, valb.type, expect, result, opname);
      }

      for (var opname in eqOps)
      {
        var op = eqOps[opname];
        var expect = expected[i][j].eq[opname];
        var failed = false;

        for (var iter = 0; iter < 5; iter++)
        {
          result = op(a, b);
          failed = failed || result !== expect;
        }

        if (failed)
          fail(i, vala.type, j, valb.type, expect, result, opname);
      }
    }
  }

  if (failures.length == 0)
    return "no failures reported!";

  return "\n" + failures.join(",\n");
}
testComparisons.expected = "no failures reported!";
test(testComparisons);

function testCaseAbort()
{
  var four = "4";
  var r = 0;
  for (var i = 0; i < 5; i++)
  {
    switch (i)
    {
      case four: r += 1; break;
      default: r += 2; break;
    }
  }

  return "" + r;
}
testCaseAbort.expected = "10";
testCaseAbort.jitstats = {
  recorderAborted: 0
};
test(testCaseAbort);

function testApplyCallHelper(f) {
    var r = [];
    for (var i = 0; i < 10; ++i) f.call();
    r.push(x);
    for (var i = 0; i < 10; ++i) f.call(this);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.apply(this);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.call(this,0);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.apply(this,[0]);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.call(this,0,1);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.apply(this,[0,1]);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.call(this,0,1,2);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.apply(this,[0,1,2]);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.call(this,0,1,2,3);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.apply(this,[0,1,2,3]);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.call(this,0,1,2,3,4);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.apply(this,[0,1,2,3,4]);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.call(this,0,1,2,3,4,5);
    r.push(x);
    for (var i = 0; i < 10; ++i) f.apply(this,[0,1,2,3,4,5])
    r.push(x);
    return(r.join(","));
}
function testApplyCall() {
    var r = testApplyCallHelper(function (a0,a1,a2,a3,a4,a5,a6,a7) { x = [a0,a1,a2,a3,a4,a5,a6,a7]; });
    r += testApplyCallHelper(function (a0,a1,a2,a3,a4,a5,a6,a7) { x = [a0,a1,a2,a3,a4,a5,a6,a7]; });
    return r;
}
testApplyCall.expected =
",,,,,,,,,,,,,,,,,,,,,,,,0,,,,,,,,0,,,,,,,,0,1,,,,,,,0,1,,,,,,,0,1,2,,,,,,0,1,2,,,,,,0,1,2,3,,,,,0,1,2,3,,,,,0,1,2,3,4,,,,0,1,2,3,4,,,,0,1,2,3,4,5,,,0,1,2,3,4,5,," +
",,,,,,,,,,,,,,,,,,,,,,,,0,,,,,,,,0,,,,,,,,0,1,,,,,,,0,1,,,,,,,0,1,2,,,,,,0,1,2,,,,,,0,1,2,3,,,,,0,1,2,3,,,,,0,1,2,3,4,,,,0,1,2,3,4,,,,0,1,2,3,4,5,,,0,1,2,3,4,5,,";
test(testApplyCall);

function testApplyUnboxHelper(f,a) {
    var q;
    for (var i = 0; i < 10; ++i)
        q = f.apply(f,a);
    return q;
}
function testApplyUnbox() {
    var f = function(x) { return x; }
    return [testApplyUnboxHelper(f,[1]), testApplyUnboxHelper(f,[true])].join(",");
}
testApplyUnbox.expected = "1,true";
test(testApplyUnbox);

function testCallPick() {
    function g(x,a) {
        x.f();
    }

    var x = [];
    x.f = function() { }

    var y = [];
    y.f = function() { }

    var z = [x,x,x,x,x,y,y,y,y,y];

    for (var i = 0; i < 10; ++i)
        g.call(this, z[i], "");
    return true;
}
testCallPick.expected = true;
test(testCallPick);

function testInvertNullAfterNegateNull()
{
  for (var i = 0; i < 5; i++) !null;
  for (var i = 0; i < 5; i++) -null;
  return "no assertion";
}
testInvertNullAfterNegateNull.expected = "no assertion";
test(testInvertNullAfterNegateNull);

function testUnaryImacros()
{
  function checkArg(x)
  {
    return 1;
  }

  var o = { valueOf: checkArg, toString: null };
  var count = 0;
  var v = 0;
  for (var i = 0; i < 5; i++)
    v += +o + -(-o);

  var results = [v === 10 ? "valueOf passed" : "valueOf failed"];

  o.valueOf = null;
  o.toString = checkArg;

  for (var i = 0; i < 5; i++)
    v += +o + -(-o);

  results.push(v === 20 ? "toString passed" : "toString failed");

  return results.join(", ");
}
testUnaryImacros.expected = "valueOf passed, toString passed";
test(testUnaryImacros);

function testAddAnyInconvertibleObject()
{
  var count = 0;
  function toString() { ++count; if (count == 95) return {}; return "" + count; }

  var threw = false;
  try
  {
    for (var i = 0; i < 100; i++)
    {
        var o = {valueOf: undefined, toString: toString};
        var q = 5 + o;
    }
  }
  catch (e)
  {
    threw = true;
    if (i !== 94)
      return "expected i === 94, got " + i;
    if (q !== "594")
      return "expected q === '594', got " + q + " (type " + typeof q + ")";
    if (count !== 95)
      return "expected count === 95, got " + count;
  }
  if (!threw)
    return "expected throw with 5 + o"; // hey, a rhyme!

  return "pass";
}
testAddAnyInconvertibleObject.expected = "pass";
testAddAnyInconvertibleObject.jitstats = {
  recorderStarted: 1,
  recorderAborted: 0,
  sideExitIntoInterpreter: 93
};
test(testAddAnyInconvertibleObject);

function testAddInconvertibleObjectAny()
{
  var count = 0;
  function toString() 
  { 
      ++count; 
      if (count == 95) 
          return {}; 
      return "" + count;
  }

  var threw = false;
  try
  {
    for (var i = 0; i < 100; i++)
    {
        var o = {valueOf: undefined, toString: toString};
        var q = o + 5;
    }
  }
  catch (e)
  {
    threw = true;
    if (i !== 94)
      return "expected i === 94, got " + i;
    if (q !== "945")
      return "expected q === '945', got " + q + " (type " + typeof q + ")";
    if (count !== 95)
      return "expected count === 95, got " + count;
  }
  if (!threw)
    return "expected throw with o + 5";

  return "pass";
}
testAddInconvertibleObjectAny.expected = "pass";
testAddInconvertibleObjectAny.jitstats = {
  recorderStarted: 1,
  recorderAborted: 0,
  sideExitIntoInterpreter: 93
};
test(testAddInconvertibleObjectAny);

function testAddInconvertibleObjectInconvertibleObject()
{
  var count1 = 0;
  function toString1() { ++count1; if (count1 == 95) return {}; return "" + count1; }
  var count2 = 0;
  function toString2() { ++count2; if (count2 == 95) return {}; return "" + count2; }

  var threw = false;
  try
  {
    for (var i = 0; i < 100; i++)
    {
        var o1 = {valueOf: undefined, toString: toString1};
        var o2 = {valueOf: undefined, toString: toString2};
        var q = o1 + o2;
    }
  }
  catch (e)
  {
    threw = true;
    if (i !== 94)
      return "expected i === 94, got " + i;
    if (q !== "9494")
      return "expected q === '9494', got " + q + " (type " + typeof q + ")";
    if (count1 !== 95)
      return "expected count1 === 95, got " + count1;
    if (count2 !== 94)
      return "expected count2 === 94, got " + count2;
  }
  if (!threw)
    return "expected throw with o1 + o2";

  return "pass";
}
testAddInconvertibleObjectInconvertibleObject.expected = "pass";
testAddInconvertibleObjectInconvertibleObject.jitstats = {
  recorderStarted: 1,
  recorderAborted: 0,
  sideExitIntoInterpreter: 93
};
test(testAddInconvertibleObjectInconvertibleObject);

function testBitOrAnyInconvertibleObject()
{
  var count = 0;
  function toString() { ++count; if (count == 95) return {}; return count; }

  var threw = false;
  try
  {
    for (var i = 0; i < 100; i++)
    {
        var o = {valueOf: undefined, toString: toString};
        var q = 1 | o;
    }
  }
  catch (e)
  {
    threw = true;
    if (i !== 94)
      return "expected i === 94, got " + i;
    if (q !== 95)
      return "expected q === 95, got " + q;
    if (count !== 95)
      return "expected count === 95, got " + count;
  }
  if (!threw)
    return "expected throw with 2 | o"; // hey, a rhyme!

  return "pass";
}
testBitOrAnyInconvertibleObject.expected = "pass";
testBitOrAnyInconvertibleObject.jitstats = {
  recorderStarted: 1,
  recorderAborted: 0,
  sideExitIntoInterpreter: 93
};
test(testBitOrAnyInconvertibleObject);

function testBitOrInconvertibleObjectAny()
{
  var count = 0;
  function toString() { ++count; if (count == 95) return {}; return count; }

  var threw = false;
  try
  {
    for (var i = 0; i < 100; i++)
    {
        var o = {valueOf: undefined, toString: toString};
        var q = o | 1;
    }
  }
  catch (e)
  {
    threw = true;
    if (i !== 94)
      return "expected i === 94, got " + i;
    if (q !== 95)
      return "expected q === 95, got " + q;
    if (count !== 95)
      return "expected count === 95, got " + count;
  }
  if (!threw)
    return "expected throw with o | 2";

  return "pass";
}
testBitOrInconvertibleObjectAny.expected = "pass";
testBitOrInconvertibleObjectAny.jitstats = {
  recorderStarted: 1,
  recorderAborted: 0,
  sideExitIntoInterpreter: 93
};
test(testBitOrInconvertibleObjectAny);

function testBitOrInconvertibleObjectInconvertibleObject()
{
  var count1 = 0;
  function toString1() { ++count1; if (count1 == 95) return {}; return count1; }
  var count2 = 0;
  function toString2() { ++count2; if (count2 == 95) return {}; return count2; }

  var threw = false;
  try
  {
    for (var i = 0; i < 100; i++)
    {
        var o1 = {valueOf: undefined, toString: toString1};
        var o2 = {valueOf: undefined, toString: toString2};
        var q = o1 | o2;
    }
  }
  catch (e)
  {
    threw = true;
    if (i !== 94)
      return "expected i === 94, got " + i;
    if (q !== 94)
      return "expected q === 94, got " + q;
    if (count1 !== 95)
      return "expected count1 === 95, got " + count1;
    if (count2 !== 94)
      return "expected count2 === 94, got " + count2;
  }
  if (!threw)
    return "expected throw with o1 | o2";

  return "pass";
}
testBitOrInconvertibleObjectInconvertibleObject.expected = "pass";
testBitOrInconvertibleObjectInconvertibleObject.jitstats = {
  recorderStarted: 1,
  recorderAborted: 0,
  sideExitIntoInterpreter: 93
};
test(testBitOrInconvertibleObjectInconvertibleObject);

function testCaseTypeMismatchBadness()
{
  for (var z = 0; z < 3; ++z)
  {
    switch ("")
    {
      default:
      case 9:
        break;

      case "":
      case <x/>:
        break;
    }
  }

  return "no crash";
}
testCaseTypeMismatchBadness.expected = "no crash";
testCaseTypeMismatchBadness.jitstats = {
    recorderAborted: 0
};
test(testCaseTypeMismatchBadness);

function testDoubleComparison()
{
  for (var i = 0; i < 500000; ++i)
  {
    switch (1 / 0)
    {
      case Infinity:
    }
  }

  return "finished";
}
testDoubleComparison.expected = "finished";
testDoubleComparison.jitstats = {
  sideExitIntoInterpreter: 1
};
test(testDoubleComparison);

function testLirBufOOM()
{
    var a = [
             "12345678901234",
             "123456789012",
             "1234567890123456789012345678",
             "12345678901234567890123456789012345678901234567890123456",
             "f",
             "$",
             "",
             "f()",
             "(\\*)",
             "b()",
             "()",
             "(#)",
             "ABCDEFGHIJK",
             "ABCDEFGHIJKLM",
             "ABCDEFGHIJKLMNOPQ",
             "ABCDEFGH",
             "(.)",
             "(|)",
             "()$",
             "/()",
             "(.)$"
             ];
    
    for (var j = 0; j < 200; ++j) {
        var js = "" + j;
        for (var i = 0; i < a.length; i++)
            "".match(a[i] + js)
    }
    return "ok";
}
testLirBufOOM.expected = "ok";
test(testLirBufOOM);

function testStringResolve() {
    var x = 0;
    for each (let d in [new String('q'), new String('q'), new String('q')]) {
        if (("" + (0 in d)) === "true")
            x++;
    }
    return x;
}
testStringResolve.expected = 3;
test(testStringResolve);

//test no multitrees assert
function testGlobalMultitrees1() {
    (function() { 
      for (var j = 0; j < 4; ++j) {
        for each (e in ['A', 1, 'A']) {
        }
      }
    })();
    return true;
}
testGlobalMultitrees1.expected = true;
test(testGlobalMultitrees1);

var q = [];
for each (b in [0x3FFFFFFF, 0x3FFFFFFF, 0x3FFFFFFF]) {
  for each (let e in [{}, {}, {}, "", {}]) { 
    b = (b | 0x40000000) + 1;
    q.push(b);
  }
}
function testLetWithUnstableGlobal() {
    return q.join(",");
}
testLetWithUnstableGlobal.expected = "2147483648,-1073741823,-1073741822,-1073741821,-1073741820,2147483648,-1073741823,-1073741822,-1073741821,-1073741820,2147483648,-1073741823,-1073741822,-1073741821,-1073741820";
test(testLetWithUnstableGlobal);
delete b;
delete q;

for each (testBug474769_b in [1, 1, 1, 1.5, 1, 1]) {
    (function() { for each (let testBug474769_h in [0, 0, 1.4, ""]) {} })()
}
function testBug474769() {
    return testBug474769_b;
}
testBug474769.expected = 1;
test(testBug474769);

function testReverseArgTypes() {
    for (var j = 0; j < 4; ++j) ''.replace('', /x/);
    return 1;
}
testReverseArgTypes.expected = 1;
test(testReverseArgTypes);

function testBug458838() {
    var a = 1;
    function g() {
        var b = 0
            for (var i = 0; i < 10; ++i) {
                b += a;
            }
        return b;
    }

    return g();
}
testBug458838.expected = 10;
testBug458838.jitstats = {
  recorderStarted: 1,
  recorderAborted: 0,
  traceCompleted: 1
};
test(testBug458838);

function testInterpreterReentry() {
    this.__defineSetter__('x', function(){})
    for (var j = 0; j < 5; ++j) { x = 3; }
    return 1;
}
testInterpreterReentry.expected = 1;
test(testInterpreterReentry);

function testInterpreterReentry2() {
    var a = false;
    var b = {};
    var c = false;
    var d = {};
    this.__defineGetter__('e', function(){});
    for (let f in this) print(f);
    [1 for each (g in this) for each (h in [])]
    return 1;
}
testInterpreterReentry2.expected = 1;
test(testInterpreterReentry2);

function testInterpreterReentry3() {
    for (let i=0;i<5;++i) this["y" + i] = function(){};
    this.__defineGetter__('e', function (x2) { yield; });
    [1 for each (a in this) for (b in {})];
    return 1;
}
testInterpreterReentry3.expected = 1;
test(testInterpreterReentry3);

function testInterpreterReentry4() {
    var obj = {a:1, b:1, c:1, d:1, get e() 1000 };
    for (var p in obj)
        obj[p];
}
test(testInterpreterReentry4);

function testInterpreterReentry5() {
    var arr = [0, 1, 2, 3, 4];
    arr.__defineGetter__("4", function() 1000);
    for (var i = 0; i < 5; i++)
        arr[i];
    for (var p in arr)
        arr[p];
}
test(testInterpreterReentry5);

function testInterpreterReentry6() {
    var obj = {a:1, b:1, c:1, d:1, set e(x) { this._e = x; }};
    for (var p in obj)
        obj[p] = "grue";
    return obj._e;
}
testInterpreterReentry6.expected = "grue";
test(testInterpreterReentry6);

function testInterpreterReentry7() {
    var arr = [0, 1, 2, 3, 4];
    arr.__defineSetter__("4", function(x) { this._4 = x; });
    for (var i = 0; i < 5; i++)
        arr[i] = "grue";
    var tmp = arr._4;
    for (var p in arr)
        arr[p] = "bleen";
    return tmp + " " + arr._4;
}
testInterpreterReentry7.expected = "grue bleen";
test(testInterpreterReentry7);

// Bug 462027 comment 54.
function testInterpreterReentery8() {
    var e = <x><y/></x>;
    for (var j = 0; j < 4; ++j) { +[e]; }
}
test(testInterpreterReentery8);

function testHolePushing() {
    var a = ["foobar", "baz"];
    for (var i = 0; i < 5; i++)
        a = [, "overwritten", "new"];
    var s = "[";
    for (i = 0; i < a.length; i++) {
        s += (i in a) ? a[i] : "<hole>";
        if (i != a.length - 1)
            s += ",";
    }
    return s + "], " + (0 in a);
}
testHolePushing.expected = "[<hole>,overwritten,new], false";
test(testHolePushing);

function testDeepBail1() {
    var y = <z/>;
    for (var i = 0; i < RUNLOOP; i++)
        "" in y;
}
test(testDeepBail1);

/* Array comprehension tests */

function Range(start, stop) {
    this.i = start;
    this.stop = stop;
}
Range.prototype = {
    __iterator__: function() this,
    next: function() {
        if (this.i >= this.stop)
            throw StopIteration;
        return this.i++;
    }
};

function range(start, stop) {
    return new Range(start, stop);
}

function testArrayComp1() {
    return [a for (a in range(0, 10))].join('');
}
testArrayComp1.expected='0123456789';
test(testArrayComp1);

function testArrayComp2() {
    return [a + b for (a in range(0, 5)) for (b in range(0, 5))].join('');
}
testArrayComp2.expected='0123412345234563456745678';
test(testArrayComp2);

function testSwitchUndefined()
{
  var x = undefined;
  var y = 0;
  for (var i = 0; i < 5; i++)
  {
    switch (x)
    {
      default:
        y++;
    }
  }
  return y;
}
testSwitchUndefined.expected = 5;
test(testSwitchUndefined);

function testGeneratorDeepBail() {
    function g() { yield 2; }
    var iterables = [[1], [], [], [], g()];

    var total = 0;
    for (let i = 0; i < iterables.length; i++)
        for each (let j in iterables[i])
                     total += j;
    return total;
}
testGeneratorDeepBail.expected = 3;
test(testGeneratorDeepBail);

function testRegexpGet() {
    var re = /hi/;
    var a = [];
    for (let i = 0; i < 5; ++i)
        a.push(re.source);
    return a.toString();
}
testRegexpGet.expected = "hi,hi,hi,hi,hi";
test(testRegexpGet);

function testThrowingObjectEqUndefined()
{
  try
  {
    var obj = { toString: function() { throw 0; } };
    for (var i = 0; i < 5; i++)
      "" + (obj == undefined);
    return i === 5;
  }
  catch (e)
  {
    return "" + e;
  }
}
testThrowingObjectEqUndefined.expected = true;
testThrowingObjectEqUndefined.jitstats = {
  sideExitIntoInterpreter: 1
};
test(testThrowingObjectEqUndefined);

function x4(v) { return "" + v + v + v + v; }
function testConvertibleObjectEqUndefined()
{
  var compares =
    [
     false, false, false, false,
     undefined, undefined, undefined, undefined,
     false, false, false, false,
     undefined, undefined, undefined, undefined,
     false, false, false, false,
     undefined, undefined, undefined, undefined,
     false, false, false, false,
     undefined, undefined, undefined, undefined,
     false, false, false, false,
     undefined, undefined, undefined, undefined,
    ];
  var count = 0;
  var obj = { valueOf: function() { count++; return 1; } };
  var results = compares.map(function(v) { return "unwritten"; });

  for (var i = 0, sz = compares.length; i < sz; i++)
    results[i] = compares[i] == obj;

  return results.join("") + count;
}
testConvertibleObjectEqUndefined.expected =
  x4(false) + x4(false) + x4(false) + x4(false) + x4(false) + x4(false) +
  x4(false) + x4(false) + x4(false) + x4(false) + "20";
testConvertibleObjectEqUndefined.jitstats = {
  sideExitIntoInterpreter: 3
};
test(testConvertibleObjectEqUndefined);

function testUndefinedPropertyAccess() {
    var x = [1,2,3];
    var y = {};
    var a = { foo: 1 };
    y.__proto__ = x;
    var z = [x, x, x, y, y, y, y, a, a, a];
    var s = "";
    for (var i = 0; i < z.length; ++i)
        s += z[i].foo;
    return s;
}
testUndefinedPropertyAccess.expected = "undefinedundefinedundefinedundefinedundefinedundefinedundefined111";
testUndefinedPropertyAccess.jitstats = {
    traceCompleted: 3
};
test(testUndefinedPropertyAccess);

q = "";
function g() { q += "g"; }
function h() { q += "h"; }
a = [g, g, g, g, h];
for (i=0; i<5; i++) { f = a[i];  f(); }

function testRebranding() {
    return q;
}
testRebranding.expected = "ggggh";
test(testRebranding);
delete q;
delete g;
delete h;
delete a;
delete f;

function testLambdaCtor() {
    var a = [];
    for (var x = 0; x < RUNLOOP; ++x) {
        var f = function(){};
        a[a.length] = new f;
    }

    // This prints false until the upvar2 bug is fixed:
    // print(a[HOTLOOP].__proto__ !== a[HOTLOOP-1].__proto__);

    // Assert that the last f was properly constructed.
    return a[RUNLOOP-1].__proto__ === f.prototype;
}

testLambdaCtor.expected = true;
test(testLambdaCtor);

function testNonStubGetter() {
    let ([] = false) { (this.watch("x", /a/g)); };
    (function () { (eval("(function(){for each (x in [1, 2, 2]);});"))(); })();
    this.unwatch("x");
    return "ok";
}
testNonStubGetter.expected = "ok";
test(testNonStubGetter);

function testString() {
    var q;
    for (var i = 0; i <= RUNLOOP; ++i) {
        q = [];
        q.push(String(void 0));
        q.push(String(true));
        q.push(String(5));
        q.push(String(5.5));
        q.push(String("5"));
        q.push(String([5]));
    }
    return q.join(",");
}
testString.expected = "undefined,true,5,5.5,5,5";
testString.jitstats = {
    recorderStarted: 1,
    sideExitIntoInterpreter: 1
};
test(testString);

function testToStringBeforeValueOf()
{
  var o = {toString: function() { return "s"; }, valueOf: function() { return "v"; } };
  var a = [];
  for (var i = 0; i < 10; i++)
    a.push(String(o));
  return a.join(",");
}
testToStringBeforeValueOf.expected = "s,s,s,s,s,s,s,s,s,s";
testToStringBeforeValueOf.jitstats = {
  recorderStarted: 1,
  sideExitIntoInterpreter: 1
};
test(testToStringBeforeValueOf);

function testNullToString()
{
  var a = [];
  for (var i = 0; i < 10; i++)
    a.push(String(null));
  for (i = 0; i < 10; i++) {
    var t = typeof a[i];
    if (t != "string")
      a.push(t);
  }
  return a.join(",");
}
testNullToString.expected = "null,null,null,null,null,null,null,null,null,null";
testNullToString.jitstats = {
  recorderStarted: 2,
  sideExitIntoInterpreter: 2,
  recorderAborted: 0
};
test(testNullToString);

function testAddNull()
{
  var rv;
  for (var x = 0; x < HOTLOOP + 1; ++x)
    rv = null + [,,];
  return rv;
}
testAddNull.expected = "null,";
testAddNull.jitstats = {
  recorderStarted: 1,
  sideExitIntoInterpreter: 1,
  recorderAborted: 0
};
test(testAddNull);

function testClosures()
{
    function MyObject(id) {
        var thisObject = this;
        this.id = id;
        this.toString = str;

        function str() {
            return "" + this.id + thisObject.id;
        }
    }

    var a = [];
    for (var i = 0; i < 5; i++)
        a.push(new MyObject(i));
    return a.toString();
}
testClosures.expected = "00,11,22,33,44";
test(testClosures);

function testMoreClosures() {
    var f = {}, max = 3;

    var hello = function(n) {
        function howdy() { return n * n }
        f.test = howdy;
    };

    for (var i = 0; i <= max; i++)
        hello(i);

    return f.test();
}
testMoreClosures.expected = 9;
test(testMoreClosures);

function testLambdaInitedVar() {
    var jQuery = function (a, b) {
        return jQuery && jQuery.length;
    }
    return jQuery();
}

testLambdaInitedVar.expected = 2;
test(testLambdaInitedVar);

function testNestedEscapingLambdas()
{
    try {
        return (function() {
            var a = [], r = [];
            function setTimeout(f, t) {
                a.push(f);
            }

            function runTimeouts() {
                for (var i = 0; i < a.length; i++)
                    a[i]();
            }

            var $foo = "#nothiddendiv";
            setTimeout(function(){
                r.push($foo);
                setTimeout(function(){
                    r.push($foo);
                }, 100);
            }, 100);

            runTimeouts();

            return r.join("");
        })();
    } catch (e) {
        return e;
    }
}
testNestedEscapingLambdas.expected = "#nothiddendiv#nothiddendiv";
test(testNestedEscapingLambdas);

function testPropagatedFunArgs()
{
  var win = this;
  var res = [], q = [];
  function addEventListener(name, func, flag) {
    q.push(func);
  }

  var pageInfo, obs;
  addEventListener("load", handleLoad, true);

  var observer = {
    observe: function(win, topic, data) {
      // obs.removeObserver(observer, "page-info-dialog-loaded");
      handlePageInfo();
    }
  };

  function handleLoad() {
    pageInfo = { toString: function() { return "pageInfo"; } };
    obs = { addObserver: function (obs, topic, data) { obs.observe(win, topic, data); } };
    obs.addObserver(observer, "page-info-dialog-loaded", false);
  }

  function handlePageInfo() {
    res.push(pageInfo);
    function $(aId) { res.push(pageInfo); };
    var feedTab = $("feedTab");
  }

  q[0]();
  return res.join(',');
}
testPropagatedFunArgs.expected = "pageInfo,pageInfo";
test(testPropagatedFunArgs);

// Second testPropagatedFunArgs test -- this is a crash-test.
(function () {
  var escapee;

  function testPropagatedFunArgs()
  {
    const magic = 42;

    var win = this;
    var res = [], q = [];
    function addEventListener(name, func, flag) {
      q.push(func);
    }

    var pageInfo = "pageInfo", obs;
    addEventListener("load", handleLoad, true);

    var observer = {
      observe: function(win, topic, data) {
        // obs.removeObserver(observer, "page-info-dialog-loaded");
        handlePageInfo();
      }
    };

    function handleLoad() {
      //pageInfo = { toString: function() { return "pageInfo"; } };
      obs = { addObserver: function (obs, topic, data) { obs.observe(win, topic, data); } };
      obs.addObserver(observer, "page-info-dialog-loaded", false);
    }

    function handlePageInfo() {
      res.push(pageInfo);
      function $(aId) {
        function notSafe() {
          return magic;
        }
        notSafe();
        res.push(pageInfo);
      };
      var feedTab = $("feedTab");
    }

    escapee = q[0];
    return res.join(',');
  }

  testPropagatedFunArgs();

  escapee();
})();

function testStringLengthNoTinyId()
{
  var x = "unset";
  var t = new String("");
  for (var i = 0; i < 5; i++)
    x = t["-1"];

  var r = "t['-1'] is " + x;
  t["-1"] = "foo";
  r += " when unset, '" + t["-1"] + "' when set";
  return r;
}
testStringLengthNoTinyId.expected = "t['-1'] is undefined when unset, 'foo' when set";
test(testStringLengthNoTinyId);

function testLengthInString()
{
  var s = new String();
  var res = "length" in s;
  for (var i = 0; i < 5; i++)
    res = res && ("length" in s);
  res = res && s.hasOwnProperty("length");
  for (var i = 0; i < 5; i++)
    res = res && s.hasOwnProperty("length");
  return res;
}
testLengthInString.expected = true;
test(testLengthInString);

function testSlowArrayLength()
{
  var counter = 0;
  var a = [];
  a[10000000 - 1] = 0;
  for (var i = 0; i < a.length; i++)
    counter++;
  return counter;
}
testSlowArrayLength.expected = 10000000;
testSlowArrayLength.jitstats = {
  recorderStarted: 1,
  recorderAborted: 0,
  sideExitIntoInterpreter: 1
};
test(testSlowArrayLength);

function testObjectLength()
{
  var counter = 0;
  var a = {};
  a.length = 10000000;
  for (var i = 0; i < a.length; i++)
    counter++;
  return counter;
}
testObjectLength.expected = 10000000;
testObjectLength.jitstats = {
  recorderStarted: 1,
  recorderAborted: 0,
  sideExitIntoInterpreter: 1
};
test(testObjectLength);

function testChangingObjectWithLength()
{
  var obj = { length: 10 };
  var dense = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
  var slow = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]; slow.slow = 5;

  /*
   * The elements of objs constitute a De Bruijn sequence repeated 4x to trace
   * and run native code for every object and transition.
   */
  var objs = [obj, obj, obj, obj,
              obj, obj, obj, obj,
              dense, dense, dense, dense,
              obj, obj, obj, obj,
              slow, slow, slow, slow,
              dense, dense, dense, dense,
              dense, dense, dense, dense,
              slow, slow, slow, slow,
              slow, slow, slow, slow,
              obj, obj, obj, obj];

  var counter = 0;

  for (var i = 0, sz = objs.length; i < sz; i++)
  {
    var o = objs[i];
    for (var j = 0; j < o.length; j++)
      counter++;
  }

  return counter;
}
testChangingObjectWithLength.expected = 400;
testChangingObjectWithLength.jitstats = {
  recorderAborted: 0,
  sideExitIntoInterpreter: 15 // empirically determined
};
test(testChangingObjectWithLength);

function testNEWINIT()
{
    var a;
    for (var i = 0; i < 10; ++i)
        a = [{}];
    return uneval(a);
}
testNEWINIT.expected = "[{}]";
test(testNEWINIT);

function testNEWINIT_DOUBLE()
{
    for (var z = 0; z < 2; ++z) { ({ 0.1: null })}
    return "ok";
}
testNEWINIT_DOUBLE.expected = "ok";
test(testNEWINIT_DOUBLE);

function testIntOverflow() {
    // int32_max - 7
    var ival = 2147483647 - 7;
    for (var i = 0; i < 30; i++) {
        ival += 2;
    }
    return (ival < 2147483647);
}
testIntOverflow.expected = false;
testIntOverflow.jitstats = {
    recorderStarted: 2,
    recorderAborted: 0,
    traceCompleted: 2,
    traceTriggered: 2,
};
test(testIntOverflow);

function testIntUnderflow() {
    // int32_min + 8
    var ival = -2147483648 + 8;
    for (var i = 0; i < 30; i++) {
        ival -= 2;
    }
    return (ival > -2147483648);
}
testIntUnderflow.expected = false;
testIntUnderflow.jitstats = {
    recorderStarted: 2,
    recorderAborted: 0,
    traceCompleted: 2,
    traceTriggered: 2,
};
test(testIntUnderflow);

function testCALLELEM()
{
    function f() {
        return 5;
    }

    function g() {
        return 7;
    }

    var x = [f,f,f,f,g];
    var y = 0;
    for (var i = 0; i < 5; ++i)
        y = x[i]();
    return y;
}
testCALLELEM.expected = 7;
test(testCALLELEM);

function testNewString()
{
  var o = { toString: function() { return "string"; } };
  var r = [];
  for (var i = 0; i < 5; i++)
    r.push(typeof new String(o));
  for (var i = 0; i < 5; i++)
    r.push(typeof new String(3));
  for (var i = 0; i < 5; i++)
    r.push(typeof new String(2.5));
  for (var i = 0; i < 5; i++)
    r.push(typeof new String("string"));
  for (var i = 0; i < 5; i++)
    r.push(typeof new String(null));
  for (var i = 0; i < 5; i++)
    r.push(typeof new String(true));
  for (var i = 0; i < 5; i++)
    r.push(typeof new String(undefined));
  return r.length === 35 && r.every(function(v) { return v === "object"; });
}
testNewString.expected = true;
testNewString.jitstats = {
  recorderStarted:  7,
  recorderAborted: 0,
  traceCompleted: 7,
  sideExitIntoInterpreter: 7
};
test(testNewString);

function testWhileObjectOrNull()
{
  try
  {
    for (var i = 0; i < 3; i++)
    {
      var o = { p: { p: null } };
      while (o.p)
        o = o.p;
    }
    return "pass";
  }
  catch (e)
  {
    return "threw exception: " + e;
  }
}
testWhileObjectOrNull.expected = "pass";
test(testWhileObjectOrNull);

function testDenseArrayProp()
{
    [].__proto__.x = 1;
    ({}).__proto__.x = 2;
    var a = [[],[],[],({}).__proto__];
    for (var i = 0; i < a.length; ++i)
        uneval(a[i].x);
    delete [].__proto__.x;
    delete ({}).__proto__.x;
    return "ok";
}
testDenseArrayProp.expected = "ok";
test(testDenseArrayProp);

function testNewWithNonNativeProto()
{
  function f() { }
  var a = f.prototype = [];
  for (var i = 0; i < 5; i++)
    var o = new f();
  return Object.getPrototypeOf(o) === a && o.splice === Array.prototype.splice;
}
testNewWithNonNativeProto.expected = true;
testNewWithNonNativeProto.jitstats = {
  recorderStarted: 1,
  recorderAborted: 0,
  sideExitIntoInterpreter: 1
};
test(testNewWithNonNativeProto);

function testLengthOnNonNativeProto()
{
  var o = {};
  o.__proto__ = [3];
  for (var j = 0; j < 5; j++)
    o[0];

  var o2 = {};
  o2.__proto__ = [];
  for (var j = 0; j < 5; j++)
    o2.length;

  function foo() { }
  foo.__proto__ = [];
  for (var j = 0; j < 5; j++)
    foo.length;

  return "no assertion";
}
testLengthOnNonNativeProto.expected = "no assertion";
test(testLengthOnNonNativeProto);

function testDeepPropertyShadowing()
{
    function h(node) {
        var x = 0;
        while (node) {
            x++;
            node = node.parent;
        }
        return x;
    }
    var tree = {__proto__: {__proto__: {parent: null}}};
    h(tree);
    h(tree);
    tree.parent = {};
    assertEq(h(tree), 2);
}
test(testDeepPropertyShadowing);

// Complicated whitebox test for bug 487845.
function testGlobalShapeChangeAfterDeepBail() {
    function f(name) {
        this[name] = 1;  // may change global shape
        for (var i = 0; i < 4; i++)
            ; // MonitorLoopEdge eventually triggers assertion
    }

    // When i==3, deep-bail, then change global shape enough times to exhaust
    // the array of GlobalStates.
    var arr = [[], [], [], ["bug0", "bug1", "bug2", "bug3", "bug4"]];
    for (var i = 0; i < arr.length; i++)
        arr[i].forEach(f);
}
test(testGlobalShapeChangeAfterDeepBail);
for (let i = 0; i < 5; i++)
    delete this["bug" + i];

function testFunctionIdentityChange()
{
  function a() {}
  function b() {}

  var o = { a: a, b: b };

  for (var prop in o)
  {
    for (var i = 0; i < 1000; i++)
      o[prop]();
  }

  return true;
}
testFunctionIdentityChange.expected = true;
testFunctionIdentityChange.jitstats = {
  recorderStarted: 2,
  traceCompleted: 2,
  sideExitIntoInterpreter: 3
};
test(testFunctionIdentityChange);

function testStringObjectLength() {
    var x = new String("foo"), y = 0;
    for (var i = 0; i < 10; ++i)
        y = x.length;
    return y;
}
testStringObjectLength.expected = 3;
test(testStringObjectLength);

var _quit;
function testNestedDeepBail()
{
    _quit = false;
    function loop() {
        for (var i = 0; i < 4; i++)
            ;
    }
    loop();

    function f() {
        loop();
        _quit = true;
    }
    var stk = [[1], [], [], [], []];
    while (!_quit)
        stk.pop().forEach(f);
}
test(testNestedDeepBail);
delete _quit;

function testSlowNativeCtor() {
    for (var i = 0; i < 4; i++)
	new Date().valueOf();
}
test(testSlowNativeCtor);

function testSlowNativeBail() {
    var a = ['0', '1', '2', '3', '+'];
    try {
	for (var i = 0; i < a.length; i++)
	    new RegExp(a[i]);
    } catch (exc) {
	assertEq(""+exc.stack.match(/^RegExp/), "RegExp");
    }
}
test(testSlowNativeBail);

/* Test the proper operation of the left shift operator. This is especially
 * important on ARM as an explicit mask is required at the native instruction
 * level. */
function testShiftLeft()
{
    var r = [];
    var i = 0;
    var j = 0;

    var shifts = [0,1,7,8,15,16,23,24,31];

    /* Samples from the simple shift range. */
    for (i = 0; i < shifts.length; i++)
        r[j++] = 1 << shifts[i];

    /* Samples outside the normal shift range. */
    for (i = 0; i < shifts.length; i++)
        r[j++] = 1 << (shifts[i] + 32);

    /* Samples far outside the normal shift range. */
    for (i = 0; i < shifts.length; i++)
        r[j++] = 1 << (shifts[i] + 224);
    for (i = 0; i < shifts.length; i++)
        r[j++] = 1 << (shifts[i] + 256);

    return r.join(",");
}
testShiftLeft.expected =
    "1,2,128,256,32768,65536,8388608,16777216,-2147483648,"+
    "1,2,128,256,32768,65536,8388608,16777216,-2147483648,"+
    "1,2,128,256,32768,65536,8388608,16777216,-2147483648,"+
    "1,2,128,256,32768,65536,8388608,16777216,-2147483648";
test(testShiftLeft);

/* Test the proper operation of the logical right shift operator. This is
 * especially important on ARM as an explicit mask is required at the native
 * instruction level. */
function testShiftRightLogical()
{
    var r = [];
    var i = 0;
    var j = 0;

    var shifts = [0,1,7,8,15,16,23,24,31];

    /* Samples from the simple shift range. */
    for (i = 0; i < shifts.length; i++)
        r[j++] = -2147483648 >>> shifts[i];

    /* Samples outside the normal shift range. */
    for (i = 0; i < shifts.length; i++)
        r[j++] = -2147483648 >>> (shifts[i] + 32);

    /* Samples far outside the normal shift range. */
    for (i = 0; i < shifts.length; i++)
        r[j++] = -2147483648 >>> (shifts[i] + 224);
    for (i = 0; i < shifts.length; i++)
        r[j++] = -2147483648 >>> (shifts[i] + 256);

    return r.join(",");
}
/* Note: Arguments to the ">>>" operator are converted to unsigned 32-bit
 * integers during evaluation. As a result, -2147483648 >>> 0 evaluates to the
 * unsigned interpretation of the same value, which is 2147483648. */
testShiftRightLogical.expected =
    "2147483648,1073741824,16777216,8388608,65536,32768,256,128,1,"+
    "2147483648,1073741824,16777216,8388608,65536,32768,256,128,1,"+
    "2147483648,1073741824,16777216,8388608,65536,32768,256,128,1,"+
    "2147483648,1073741824,16777216,8388608,65536,32768,256,128,1";
test(testShiftRightLogical);

/* Test the proper operation of the arithmetic right shift operator. This is
 * especially important on ARM as an explicit mask is required at the native
 * instruction level. */
function testShiftRightArithmetic()
{
    var r = [];
    var i = 0;
    var j = 0;

    var shifts = [0,1,7,8,15,16,23,24,31];

    /* Samples from the simple shift range. */
    for (i = 0; i < shifts.length; i++)
        r[j++] = -2147483648 >> shifts[i];

    /* Samples outside the normal shift range. */
    for (i = 0; i < shifts.length; i++)
        r[j++] = -2147483648 >> (shifts[i] + 32);

    /* Samples far outside the normal shift range. */
    for (i = 0; i < shifts.length; i++)
        r[j++] = -2147483648 >> (shifts[i] + 224);
    for (i = 0; i < shifts.length; i++)
        r[j++] = -2147483648 >> (shifts[i] + 256);

    return r.join(",");
}
testShiftRightArithmetic.expected =
    "-2147483648,-1073741824,-16777216,-8388608,-65536,-32768,-256,-128,-1,"+
    "-2147483648,-1073741824,-16777216,-8388608,-65536,-32768,-256,-128,-1,"+
    "-2147483648,-1073741824,-16777216,-8388608,-65536,-32768,-256,-128,-1,"+
    "-2147483648,-1073741824,-16777216,-8388608,-65536,-32768,-256,-128,-1";
test(testShiftRightArithmetic);

function testStringConstructorWithExtraArg() {
    for (let i = 0; i < 5; ++i)
        new String(new String(), 2);
    return "ok";
}
testStringConstructorWithExtraArg.expected = "ok";
test(testStringConstructorWithExtraArg);

function testConstructorBail() {
    for (let i = 0; i < 5; ++i) new Number(/x/);
}
test(testConstructorBail);

function testNewArrayCount()
{
  var a = [];
  for (var i = 0; i < 5; i++)
    a = [0];
  assertEq(a.__count__, 1);
  for (var i = 0; i < 5; i++)
    a = [0, , 2];
  assertEq(a.__count__, 2);
}
test(testNewArrayCount);

function testNewArrayCount2() {
    var x = 0;
    for (var i = 0; i < 10; ++i)
        x = new Array(1,2,3).__count__;
    return x;
}
testNewArrayCount2.expected = 3;
test(testNewArrayCount2);

/*****************************************************************************
 *                                                                           *
 *  _____ _   _  _____ ______ _____ _______                                  *
 * |_   _| \ | |/ ____|  ____|  __ \__   __|                                 *
 *   | | |  \| | (___ | |__  | |__) | | |                                    *
 *   | | | . ` |\___ \|  __| |  _  /  | |                                    *
 *  _| |_| |\  |____) | |____| | \ \  | |                                    *
 * |_____|_| \_|_____/|______|_|  \_\ |_|                                    *
 *                                                                           *
 *                                                                           *
 *  _______ ______  _____ _______ _____                                      *
 * |__   __|  ____|/ ____|__   __/ ____|                                     *
 *    | |  | |__  | (___    | | | (___                                       *
 *    | |  |  __|  \___ \   | |  \___ \                                      *
 *    | |  | |____ ____) |  | |  ____) |                                     *
 *    |_|  |______|_____/   |_| |_____/                                      *
 *                                                                           *
 *                                                                           *
 *  ____  ______ ______ ____  _____  ______    _    _ ______ _____  ______   *
 * |  _ \|  ____|  ____/ __ \|  __ \|  ____|  | |  | |  ____|  __ \|  ____|  *
 * | |_) | |__  | |__ | |  | | |__) | |__     | |__| | |__  | |__) | |__     *
 * |  _ <|  __| |  __|| |  | |  _  /|  __|    |  __  |  __| |  _  /|  __|    *
 * | |_) | |____| |   | |__| | | \ \| |____   | |  | | |____| | \ \| |____   *
 * |____/|______|_|    \____/|_|  \_\______|  |_|  |_|______|_|  \_\______|  *
 *                                                                           *
 *****************************************************************************/

// math-trace-tests.js is a separate file here.

// MANDELBROT STUFF deleted

/*****************************************************************************
 *  _   _  ____     _   __  ____  _____  ______                              *
 * | \ | |/ __ \   |  \/  |/ __ \|  __ \|  ____|                             *
 * |  \| | |  | |  | \  / | |  | | |__) | |__                                *
 * | . ` | |  | |  | |\/| | |  | |  _  /|  __|                               *
 * | |\  | |__| |  | |  | | |__| | | \ \| |____                              *
 * |_| \_|\____/   |_|  |_|\____/|_|  \_\______|                             *
 *                                                                           *
 *  _______ ______  _____ _______ _____                                      *
 * |__   __|  ____|/ ____|__   __/ ____|                                     *
 *    | |  | |__  | (___    | | | (___                                       *
 *    | |  |  __|  \___ \   | |  \___ \                                      *
 *    | |  | |____ ____) |  | |  ____) |                                     *
 *    |_|  |______|_____/   |_| |_____/                                      *
 *                                                                           *
 *           ______ _______ ______ _____     _    _ ______ _____  ______ _   *
 *     /\   |  ____|__   __|  ____|  __ \   | |  | |  ____|  __ \|  ____| |  *
 *    /  \  | |__     | |  | |__  | |__) |  | |__| | |__  | |__) | |__  | |  *
 *   / /\ \ |  __|    | |  |  __| |  _  /   |  __  |  __| |  _  /|  __| | |  *
 *  / ____ \| |       | |  | |____| | \ \   | |  | | |____| | \ \| |____|_|  *
 * /_/    \_\_|       |_|  |______|_|  \_\  |_|  |_|______|_|  \_\______(_)  *
 *                                                                           *
 *****************************************************************************/

/* NOTE: Keep this test last, since it screws up all for...in loops after it. */
function testGlobalProtoAccess() {
  return "ok";
}
this.__proto__.a = 3; for (var j = 0; j < 4; ++j) { [a]; }
testGlobalProtoAccess.expected = "ok";
test(testGlobalProtoAccess);

jit(false);

/* Keep these at the end so that we can see the summary after the trace-debug spew. */
if (gReportSummary) {
  print("\npassed:", passes.length && passes.join(","));
  print("\nFAILED:", fails.length && fails.join(","));
}
