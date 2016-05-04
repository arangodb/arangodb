// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 10.4.3-1-17-s
description: Strict Mode - checking 'this' (eval used within strict mode)
flags: [onlyStrict]
includes: [fnGlobalObject.js]
---*/

function testcase() {
  assert.sameValue(eval("typeof this"), "undefined", 'eval("typeof this")');
  assert.notSameValue(eval("this"), fnGlobalObject(), 'eval("this")');
}
testcase();
