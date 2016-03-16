// Copyright 2015 Microsoft Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
description: Elements added after the call to from
es6id: 22.1.2.1
---*/

var arrayIndex = -1;
var originalLength = 7;
var obj = {
    length: originalLength,
    0: 2,
    1: 4,
    2: 8,
    3: 16,
    4: 32,
    5: 64,
    6: 128
};
var array = [ 2, 4, 8, 16, 32, 64, 128 ];
function mapFn(value, index) {
    arrayIndex++;
    assert.sameValue(value, obj[arrayIndex], "Value mismatch in mapFn at index " + index + ".");
    assert.sameValue(index, arrayIndex, "Index mismatch in mapFn.");
    obj[originalLength + arrayIndex] = 2 * arrayIndex + 1;

    return obj[arrayIndex];
}


var a = Array.from(obj, mapFn);
assert.sameValue(a.length, array.length, "Length mismatch.");

for (var j = 0; j < a.length; j++) {
    assert.sameValue(a[j], array[j], "Element mismatch for array at index " + j + ".");
}
