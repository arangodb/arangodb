// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.3.4.5-15-3
description: >
    Function.prototype.bind - The [[Writable]] attribute of length
    property in F set as false
includes: [propertyHelper.js]
---*/

function foo() { }
var obj = foo.bind({});
var flength = obj.length;

assert(obj.hasOwnProperty("length"));
verifyNotWritable(obj, "length", null, 100);
assert.sameValue(obj.length, flength);
