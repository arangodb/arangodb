// Copyright (C) 2016 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.NumberFormat.prototype.format
description: >
  The bound NumberFormat format function is an anonymous function.
info: >
  11.4.3 get Intl.NumberFormat.prototype.compare

  ...
  4. If nf.[[boundFormat]] is undefined, then
    a. Let F be a new built-in function object as defined in Number Format Functions (11.1.3).
    b. Let bf be BoundFunctionCreate(F, nf, « »).
    c. Perform ! DefinePropertyOrThrow(bf, "length", PropertyDescriptor {[[Value]]: 1, [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true}).
    d. Set nf.[[boundFormat]] to bf.
  ...
---*/

var formatFn = new Intl.NumberFormat().format;

assert.sameValue(Object.prototype.hasOwnProperty.call(formatFn, "name"), false);
