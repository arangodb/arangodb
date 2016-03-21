// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    Result of applying "typeof" operator to the object that is native and
    implements [[Call]] is "function"
es5id: 11.4.3_A3.7
es6id: 12.5.6.1
description: typeof (object with [[Call]]) === "function"
---*/

assert.sameValue(
  typeof new Function(),
   "function",
  '#1: typeof new Function() === "function". Actual: ' + (typeof new Function())
);

assert.sameValue(
  typeof Function(),
   "function",
  '#2: typeof Function() === "function". Actual: ' + (typeof Function())
);

assert.sameValue(
  typeof Object,
   "function",
  '#3: typeof Object === "function". Actual: ' + (typeof Object)
);

assert.sameValue(
  typeof String,
   "function",
  '#4: typeof String === "function". Actual: ' + (typeof String)
);

assert.sameValue(
  typeof Boolean,
   "function",
  '#5: typeof Boolean === "function". Actual: ' + (typeof Boolean)
);

assert.sameValue(
  typeof Number,
   "function",
  '#6: typeof Number === "function". Actual: ' + (typeof Number)
);

assert.sameValue(
  typeof Date,
   "function",
  '#7: typeof Date === "function". Actual: ' + (typeof Date)
);

assert.sameValue(
  typeof Error,
   "function",
  '#8: typeof Error === "function". Actual: ' + (typeof Error)
);

assert.sameValue(
  typeof RegExp,
   "function",
  '#9: typeof RegExp === "function". Actual: ' + (typeof RegExp)
);
