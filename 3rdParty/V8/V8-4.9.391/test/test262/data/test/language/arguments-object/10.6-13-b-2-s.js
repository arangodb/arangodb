// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 10.6-13-b-2-s
description: arguments.caller exists in strict mode
flags: [onlyStrict]
---*/

function testcase() {
  var desc = Object.getOwnPropertyDescriptor(arguments,"caller");
  assert.notSameValue(desc, undefined);
 }
testcase();
