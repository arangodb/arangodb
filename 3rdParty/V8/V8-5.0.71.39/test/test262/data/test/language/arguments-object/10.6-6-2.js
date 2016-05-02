// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 10.6-6-2
description: "'length' property of arguments object has correct attributes"
---*/

function testcase() {
  var desc = Object.getOwnPropertyDescriptor(arguments,"length");

  assert.sameValue(desc.configurable, true, 'desc.configurable');
  assert.sameValue(desc.enumerable, false, 'desc.enumerable');
  assert.sameValue(desc.writable, true, 'desc.writable');
 }
testcase();
