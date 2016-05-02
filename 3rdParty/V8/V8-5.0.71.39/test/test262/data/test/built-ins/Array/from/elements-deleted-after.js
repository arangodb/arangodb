// Copyright 2015 Microsoft Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
description: >
    Elements deleted after the call started and before visited are not
    visited
es6id: 22.1.2.1
---*/

var originalArray = [ 0, 1, -2, 4, -8, 16 ];
var array = [ 0, 1, -2, 4, -8, 16 ];
var a = [];
var arrayIndex = -1;
function mapFn(value, index) {
    this.arrayIndex++;
    assert.sameValue(value, array[this.arrayIndex], "Value mismatch in mapFn at index " + index + ".");
    assert.sameValue(index, this.arrayIndex, "Index mismatch in mapFn.");

    array.splice(array.length - 1, 1);
    return 127;
}


a = Array.from(array, mapFn, this);

assert.sameValue(a.length, originalArray.length / 2, "Length mismatch. Old array : " + (originalArray.length / 2) + ". array : " + a.length + ".");

for (var j = 0; j < originalArray.length / 2; j++) {
    assert.sameValue(a[j], 127, "Element mismatch for mapped array at index " + j + ".");
}
