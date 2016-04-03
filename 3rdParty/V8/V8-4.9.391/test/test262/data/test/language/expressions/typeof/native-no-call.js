// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    Result of applying "typeof" operator to the object that is native and
    doesn't implement [[Call]] is "object"
es5id: 11.4.3_A3.6
es6id: 12.5.6.1
description: typeof (object without [[Call]]) === "object"
---*/

assert.sameValue(
  typeof this,
   "object",
  '#1: typeof this === "object". Actual: ' + (typeof this)
);

assert.sameValue(
  typeof new Object(),
   "object",
  '#2: typeof new Object() === "object". Actual: ' + (typeof new Object())
);

assert.sameValue(
  typeof new Array(1,2,3),
   "object",
  '#3: typeof new Array(1,2,3) === "object". Actual: ' + (typeof new Array(1,2,3))
);

assert.sameValue(
  typeof Array(1,2,3),
   "object",
  '#4: typeof Array(1,2,3) === "object". Actual: ' + (typeof Array(1,2,3))
);

assert.sameValue(
  typeof new String("x"),
   "object",
  '#5: typeof new String("x") === "object". Actual: ' + (typeof new String("x"))
);

assert.sameValue(
  typeof new Boolean(true),
   "object",
  '#6: typeof new Boolean(true) === "object". Actual: ' + (typeof new Boolean(true))
);

assert.sameValue(
  typeof new Number(1),
   "object",
  '#7: typeof new Number(1) === "object". Actual: ' + (typeof new Number(1))
);

//The Math object does not have a [[Construct]] property; 
//it is not possible to use the Math object as a constructor with the new operator.
//The Math object does not have a [[Call]] property; it is not possible to invoke the Math object as a object.
assert.sameValue(
  typeof Math,
   "object",
  '#8: typeof Math === "object". Actual: ' + (typeof Math)
);

assert.sameValue(
  typeof new Date(),
   "object",
  '#9: typeof new Date() === "object". Actual: ' + (typeof new Date())
);

assert.sameValue(
  typeof new Error(),
   "object",
  '#10: typeof new Error() === "object". Actual: ' + (typeof new Error())
);

assert.sameValue(
  typeof new RegExp(),
   "object",
  '#11: typeof new RegExp() === "object". Actual: ' + (typeof new RegExp())
);

assert.sameValue(
  typeof RegExp(),
   "object",
  '#12: typeof RegExp() === "object". Actual: ' + (typeof RegExp())
);
