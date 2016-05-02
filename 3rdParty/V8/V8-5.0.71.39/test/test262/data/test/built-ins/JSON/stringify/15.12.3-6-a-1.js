// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.12.3-6-a-1
description: >
    JSON.stringify treats numeric space arguments greater than 10 the
    same as a  space argument of 10.
---*/

  var obj = {a1: {b1: [1,2,3,4], b2: {c1: 1, c2: 2}},a2: 'a2'};

assert.sameValue(JSON.stringify(obj,null, 10), JSON.stringify(obj,null, 100), 'JSON.stringify(obj,null, 10)');
