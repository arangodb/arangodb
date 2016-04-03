// Copyright (c) 2014 Thomas Dahlstrom. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Array.prototype.sort does not change non-existent elements to
    undefined elements, that means holes are preserved (cf. spec  text
    about [[Delete]] and sparse arrays)
author: Thomas Dahlstrom (tdahlstrom@gmail.com)
---*/

var array = ['a',,void 0];

//CHECK#1
if (array.length !== 3) {
    $ERROR('#1: array.length !== 3. Actual: ' + (array.length))
}

//CHECK#2
if (array.hasOwnProperty('0') !== true) {
    $ERROR("#2: array.hasOwnProperty('0'). Actual: " + array.hasOwnProperty('0'));
}

//CHECK#3
if (array.hasOwnProperty('1') !== false) {
    $ERROR("#3: array.hasOwnProperty('1'). Actual: " + array.hasOwnProperty('1'));
}

//CHECK#4
if (array.hasOwnProperty('2') !== true) {
    $ERROR("#4: array.hasOwnProperty('2'). Actual: " + array.hasOwnProperty('2'));
}

array.sort();

//CHECK#5
if (array.length !== 3) {
    $ERROR('#5: array.length !== 3. Actual: ' + (array.length))
}

//CHECK#6
if (array.hasOwnProperty('0') !== true) {
    $ERROR("#6: array.hasOwnProperty('0'). Actual: " + array.hasOwnProperty('0'));
}

//CHECK#7
if (array.hasOwnProperty('1') !== true) {
    $ERROR("#7: array.hasOwnProperty('1'). Actual: " + array.hasOwnProperty('1'));
}

//CHECK#8
if (array.hasOwnProperty('2') !== false) {
    $ERROR("#8: array.hasOwnProperty('2'). Actual: " + array.hasOwnProperty('2'));
}
