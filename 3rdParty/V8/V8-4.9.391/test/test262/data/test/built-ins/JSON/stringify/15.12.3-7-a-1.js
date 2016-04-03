// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.12.3-7-a-1
description: >
    JSON.stringify only uses the first 10 characters of a string space
    arguments.
---*/

  var obj = {a1: {b1: [1,2,3,4], b2: {c1: 1, c2: 2}},a2: 'a2'};

assert.sameValue(JSON.stringify(obj,null, '0123456789xxxxxxxxx'), JSON.stringify(obj,null, '0123456789'), 'JSON.stringify(obj,null, "0123456789xxxxxxxxx")');
