// Copyright (C) 2016 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.DateTimeFormat.prototype.format
description: >
  The bound DateTimeFormat format function is an anonymous function.
info: >
  12.4.3 get Intl.DateTimeFormat.prototype.compare

  ...
  4. If the [[boundFormat]] internal slot of dtf is undefined, then
    a. Let F be a new built-in function object as defined in DateTime Format Functions (12.1.5).
    b. Let bf be BoundFunctionCreate(F, dft, « »).
    c. Perform ! DefinePropertyOrThrow(bf, "length", PropertyDescriptor {[[Value]]: 1, [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true}).
    d. Set dtf.[[boundFormat]] to bf.
  ...
---*/

var formatFn = new Intl.DateTimeFormat().format;

assert.sameValue(Object.prototype.hasOwnProperty.call(formatFn, "name"), false);
