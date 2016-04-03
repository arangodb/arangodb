// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator "typeof" uses GetValue
es5id: 11.4.3_A2_T2
es6id: 12.5.6.1
description: If GetBase(x) is null, return "undefined"
---*/

assert.sameValue(
  typeof x, "undefined", '#1: typeof x === "undefined". Actual: ' + (typeof x)
);
