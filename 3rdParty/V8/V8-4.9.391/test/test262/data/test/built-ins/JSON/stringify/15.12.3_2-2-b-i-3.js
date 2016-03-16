// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.12.3_2-2-b-i-3
description: >
    JSON.stringify converts Boolean wrapper objects returned from a
    toJSON call to literal Boolean values.
---*/

  var obj = {
    prop:42,
    toJSON: function () {return new Boolean(true)}
    };

assert.sameValue(JSON.stringify([obj]), '[true]', 'JSON.stringify([obj])');
