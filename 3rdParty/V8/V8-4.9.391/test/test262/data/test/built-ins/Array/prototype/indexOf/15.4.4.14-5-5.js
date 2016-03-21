// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-5-5
description: Array.prototype.indexOf returns 0 if fromIndex is null
---*/

  var a = [1,2,3];

// null resolves to 0
assert.sameValue(a.indexOf(1,null), 0, 'a.indexOf(1,null)');
