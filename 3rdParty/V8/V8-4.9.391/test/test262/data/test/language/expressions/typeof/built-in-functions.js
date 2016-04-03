// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    There are two types of Function objects. Internal functions
    are built-in objects of the language, such as parseInt and Math.exp
es5id: 10.1.1_A2_T1
es6id: 12.5.6.1
description: Checking types of parseInt and Math.exp
---*/

assert.sameValue(
  typeof Math.exp,
  "function",
  '#1: typeof Math.exp!=="function" '+typeof Math.exp
);

assert.sameValue(
  typeof parseInt,
  "function",
  '#2: typeof parseInt!=="function" '+typeof parseInt
);
