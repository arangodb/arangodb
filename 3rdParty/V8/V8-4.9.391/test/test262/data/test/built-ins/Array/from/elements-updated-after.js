// Copyright 2015 Microsoft Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
description: Elements are updated after the call to from
es6id: 22.1.2.1
---*/

var array = [ 127, 4, 8, 16, 32, 64, 128 ];
var arrayIndex = -1;
function mapFn(value, index) {
    arrayIndex++;
    if (index + 1 < array.length) {
        array[index + 1] = 127;
    }
    assert.sameValue(value, 127, "Value mismatch in mapFn at index " + index + ".");
    assert.sameValue(index, arrayIndex, "Index mismatch in mapFn.");

    return value;
}

var a = Array.from(array, mapFn);
assert.sameValue(a.length, array.length, "Length mismatch.");
for (var j = 0; j < a.length; j++) {
    assert.sameValue(a[j], 127, "Element mismatch for mapped array.");
}
