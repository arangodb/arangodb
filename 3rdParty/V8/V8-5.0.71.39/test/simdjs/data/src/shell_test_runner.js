// This is a simple script for running the tests from a standalone JS shell.

load("ecmascript_simd.js");

// clearer marking
var currentName = '<global>';
var numFails = 0;

if (typeof skipValueTests === 'undefined')
  skipValueTests = false;
if (typeof skipFromBitsTests === 'undefined')
  skipValueTests = true;

function printIndented(str) {
  console.log(str.split('\n').map(function (s) { return '  ' + s }).join('\n'));
}

function fail(str) {
  var e = Error(str);
  console.log(e.toString());
  printIndented(e.stack);
  numFails++;
}

function test(name, func) {
  currentName = name;
  if (typeof skipValueTests !== 'undefined' && skipValueTests &&
      name.indexOf('value semantics') != -1) return;
  if (typeof skipRoundTripTests !== 'undefined' && skipRoundTripTests &&
      name.indexOf('Float32x4 Int32x4 round trip') != -1) return;
  try {
    func();
  } catch (e) {
    console.log('exception thrown from ' + currentName + ': ' + e.toString());
    if (e.stack)
      printIndented(e.stack);
    numFails++;
  }
}

function equal(a, b) {
  if (a != b)
    fail('equal(' + a + ', ' + b + ') failed in ' + currentName);
}

function notEqual(a, b) {
  if (a == b)
    fail('notEqual(' + a + ', ' + b + ') failed in ' + currentName);
}

function throws(func) {
  var pass = false;
  try {
    func();
  } catch (e) {
    pass = true;
  }
  if (!pass)
    fail('throws failed in ' + currentName);
}

function ok(x) {
  if (!x)
    fail('not ok in ' + currentName);
}

load("ecmascript_simd_tests.js");

if (numFails > 0) {
  print('total number of fails and exceptions: ' + numFails);
  quit(1);
}
