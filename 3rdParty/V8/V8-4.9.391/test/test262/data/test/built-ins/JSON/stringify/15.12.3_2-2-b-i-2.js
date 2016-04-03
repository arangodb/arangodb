// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.12.3_2-2-b-i-2
description: >
    JSON.stringify converts Number wrapper objects returned from a
    toJSON call to literal Number.
---*/

  var obj = {
    prop:42,
    toJSON: function () {return new Number(42)}
    };

assert.sameValue(JSON.stringify([obj]), '[42]', 'JSON.stringify([obj])');
