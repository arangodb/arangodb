// Copyright (C) 2016 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.Collator.prototype.compare
description: >
  The bound Collator compare function is an anonymous function.
info: >
  10.3.3 get Intl.Collator.prototype.compare

  ...
  4. If collator.[[boundCompare]] is undefined, then
    a. Let F be a new built-in function object as defined in 10.3.4.
    b. Let bc be BoundFunctionCreate(F, collator, « »).
    c. Perform ! DefinePropertyOrThrow(bc, "length", PropertyDescriptor {[[Value]]: 2, [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true}).
    d. Set collator.[[boundCompare]] to bc.
  ...
---*/

var compareFn = new Intl.Collator().compare;

assert.sameValue(Object.prototype.hasOwnProperty.call(compareFn, "name"), false);
