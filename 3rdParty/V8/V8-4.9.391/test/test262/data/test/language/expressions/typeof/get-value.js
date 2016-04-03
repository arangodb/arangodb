// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator "typeof" uses GetValue
es5id: 11.4.3_A2_T1
es6id: 12.5.6.1
description: Either Type(x) is not Reference or GetBase(x) is not null
---*/

assert.sameValue(
  typeof 0,
   "number",
  '#1: typeof 0 === "number". Actual: ' + (typeof 0)
);

var x = 0;
assert.sameValue(
  typeof x,
   "number",
  '#2: typeof x === "number". Actual: ' + (typeof x)
);

var x = new Object();
assert.sameValue(
  typeof x,
   "object",
  '#3: var x = new Object(); typeof x === "object". Actual: ' + (typeof x)
);
