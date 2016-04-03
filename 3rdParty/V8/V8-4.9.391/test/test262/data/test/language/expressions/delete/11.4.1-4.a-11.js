// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    This test is actually testing the [[Delete]] internal method (8.12.8). Since the
    language provides no way to directly exercise [[Delete]], the tests are placed here.
es5id: 11.4.1-4.a-11
description: >
    delete operator returns true on deleting arguments
    propterties(arguments.callee)
flags: [noStrict]
---*/

function testcase() {
  function foo(a,b)
  {
    return (delete arguments.callee); 
  }
  var d = delete arguments.callee;

  assert.sameValue(d, true, 'd');
  assert.sameValue(arguments.callee, undefined, 'arguments.callee');
 }
testcase();
