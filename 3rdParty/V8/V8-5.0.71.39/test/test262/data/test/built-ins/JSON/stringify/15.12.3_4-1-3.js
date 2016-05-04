// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.12.3_4-1-3
description: JSON.stringify a indirectly circular object throws a error
---*/

  var obj = {p1: {p2: {}}};
  obj.p1.p2.prop = obj;

assert.throws(TypeError, function() {
     JSON.stringify(obj);
});
