// Copyright 2015 Microsoft Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
description: mapFn throws an exception
es6id: 22.1.2.1
---*/

var array = [ 2, 4, 8, 16, 32, 64, 128 ];
function mapFn(value, index, obj) {
    throw new Test262Error();
}

assert.throws(Test262Error, function(){Array.from(array, mapFn);});
