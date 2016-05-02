// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.12.3-6-b-4
description: >
    JSON.stringify treats numeric space arguments (in the range 1..10)
    is equivalent to a string of spaces of that length.
---*/

  var obj = {a1: {b1: [1,2,3,4], b2: {c1: 1, c2: 2}},a2: 'a2'};
  var fiveSpaces = '     ';
  //               '12345'

assert.sameValue(JSON.stringify(obj,null, 5), JSON.stringify(obj, null, fiveSpaces), 'JSON.stringify(obj,null, 5)');
