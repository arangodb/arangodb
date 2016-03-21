// Copyright 2015 Microsoft Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
description: Source array with boundary values
es6id: 22.1.2.1
---*/

var array = [ Number.MAX_VALUE, Number.MIN_VALUE, Number.NaN, Number.NEGATIVE_INFINITY, Number.POSITIVE_INFINITY ];
var arrayIndex = -1;
function mapFn(value, index) {
    this.arrayIndex++;
    assert.sameValue(value, array[this.arrayIndex], "Value mismatch in mapFn at index " + index + ".");
    assert.sameValue(index, this.arrayIndex, "Index mismatch in mapFn.");

    return value;
}

var a = Array.from(array, mapFn, this);

assert.sameValue(a.length, array.length, "Length mismatch.");
assert.sameValue(a[0], Number.MAX_VALUE, "Element mismatch for mapped array at index 0.");
assert.sameValue(a[1], Number.MIN_VALUE, "Element mismatch for mapped array at index 1.");
assert.sameValue(a[2], Number.NaN, "Element mismatch for mapped array at index 2.");
assert.sameValue(a[3], Number.NEGATIVE_INFINITY, "Element mismatch for mapped array at index 3.");
assert.sameValue(a[4], Number.POSITIVE_INFINITY, "Element mismatch for mapped array at index 4.");
