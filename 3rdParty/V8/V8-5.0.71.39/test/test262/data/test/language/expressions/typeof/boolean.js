// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Result of applying "typeof" operator to boolean is "boolean"
es5id: 11.4.3_A3.3
es6id: 12.5.6.1
description: typeof (boolean value) === "boolean"
---*/

assert.sameValue(
  typeof true,
   "boolean",
  '#1: typeof true === "boolean". Actual: ' + (typeof true)
);

assert.sameValue(
  typeof false,
   "boolean",
  '#2: typeof false === "boolean". Actual: ' + (typeof false)
);

assert.sameValue(
  typeof !-1,
   "boolean",
  '#3: typeof !-1 === "boolean". Actual: ' + (typeof !-1)
);
